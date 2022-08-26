#pragma once

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "dict_reader.h"
#include "dict_writer.h"
#include "http_client.h"

// TODO: Everywhere you see CHECK, I need to add error handling.
#define CHECK

namespace datadog {
namespace tracing {

class Curl : public HTTPClient {
  std::mutex mutex_;
  CURLM *multi_handle_;
  std::unordered_set<CURL *> request_handles_;
  std::list<CURL *> new_handles_;
  bool shutting_down_;
  std::thread event_loop_;

  struct Request {
    curl_slist *request_headers = nullptr;
    std::string request_body;
    ResponseHandler on_response;
    ErrorHandler on_error;
    char error_buffer[CURL_ERROR_SIZE] = "";
    std::unordered_map<std::string, std::string> response_headers_lower;
    std::string response_body;

    ~Request();
  };

  class HeaderWriter : public DictWriter {
    curl_slist *list_ = nullptr;
    std::string buffer_;

   public:
    ~HeaderWriter();
    curl_slist *release();
    virtual void set(std::string_view key, std::string_view value) override;
  };

  class HeaderReader : public DictReader {
    std::unordered_map<std::string, std::string> *response_headers_lower_;
    mutable std::string buffer_;

   public:
    explicit HeaderReader(
        std::unordered_map<std::string, std::string> *response_headers_lower);
    virtual std::optional<std::string_view> lookup(
        std::string_view key) const override;
    virtual void visit(
        const std::function<void(std::string_view key, std::string_view value)>
            &visitor) const override;
  };

  void run();
  static std::size_t on_read_header(char *data, std::size_t, std::size_t length,
                                    void *user_data);
  static std::size_t on_read_body(char *data, std::size_t, std::size_t length,
                                  void *user_data);
  static bool is_non_whitespace(unsigned char);
  static char to_lower(unsigned char);
  static std::string_view trim(std::string_view);

 public:
  Curl();
  ~Curl();

  virtual Expected<void> post(const URL &url, HeadersSetter set_headers,
                              std::string body, ResponseHandler on_response,
                              ErrorHandler on_error) override;
};

inline Curl::Curl()
    : multi_handle_((curl_global_init(CURL_GLOBAL_ALL), curl_multi_init())),
      shutting_down_(false),
      event_loop_([this]() { run(); }) {}

inline Curl::~Curl() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    shutting_down_ = true;
  }
  CHECK curl_multi_wakeup(multi_handle_);
  event_loop_.join();
}

inline Expected<void> Curl::post(const HTTPClient::URL &url,
                                 HeadersSetter set_headers, std::string body,
                                 ResponseHandler on_response,
                                 ErrorHandler on_error) {
  auto request = new Request();

  request->request_body = std::move(body);
  request->on_response = std::move(on_response);
  request->on_error = std::move(on_error);

  CURL *handle = curl_easy_init();
  // TODO: no
  CHECK curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);
  // end TODO

  // TODO: error handling
  CHECK curl_easy_setopt(handle, CURLOPT_PRIVATE, request);
  CHECK curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, request->error_buffer);
  CHECK curl_easy_setopt(handle, CURLOPT_POST, 1);
  CHECK curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                         request->request_body.size());
  CHECK curl_easy_setopt(handle, CURLOPT_POSTFIELDS,
                         request->request_body.data());
  CHECK curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, &on_read_header);
  CHECK curl_easy_setopt(handle, CURLOPT_HEADERDATA, request);
  CHECK curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &on_read_body);
  CHECK curl_easy_setopt(handle, CURLOPT_WRITEDATA, request);
  if (url.scheme == "unix" || url.scheme == "http+unix" ||
      url.scheme == "https+unix") {
    CHECK curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH,
                           url.authority.c_str());
    // The authority section of the URL is ignored when a unix domain socket is
    // to be used.
    CHECK curl_easy_setopt(handle, CURLOPT_URL,
                           ("http://localhost" + url.path).c_str());
  } else {
    CHECK curl_easy_setopt(
        handle, CURLOPT_URL,
        (url.scheme + "://" + url.authority + url.path).c_str());
  }

  HeaderWriter writer;
  set_headers(writer);
  request->request_headers = writer.release();
  CHECK curl_easy_setopt(handle, CURLOPT_HTTPHEADER, request->request_headers);

  // TODO: Error handling
  std::list<CURL *> node;
  node.push_back(handle);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    new_handles_.splice(new_handles_.end(), node);
  }
  CHECK curl_multi_wakeup(multi_handle_);

  return std::nullopt;
}

inline std::size_t Curl::on_read_header(char *data, std::size_t,
                                        std::size_t length, void *user_data) {
  const auto request = static_cast<Request *>(user_data);
  // The idea is:
  //
  //         "    Foo-Bar  :   thingy, thingy, thing   \r\n"
  //    -> {"foo-bar", "thingy, thingy, thing"}
  //
  // There isn't always a colon.  Inputs without a colon can be ignored:
  //
  // > For an HTTP transfer, the status line and the blank line preceding the
  // > response body are both included as headers and passed to this
  // > function.
  //
  // https://curl.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
  //

  const char *const begin = data;
  const char *const end = begin + length;
  const char *const colon = std::find(begin, end, ':');
  if (colon == end) {
    return length;
  }

  const auto key = trim(std::string_view(begin, colon - begin));
  const auto value = trim(std::string_view(colon + 1, end - (colon + 1)));

  std::string key_lower;
  key_lower.reserve(key.size());
  std::transform(key.begin(), key.end(), std::back_inserter(key_lower),
                 &to_lower);

  request->response_headers_lower.emplace(std::move(key_lower), value);
  return length;
}

inline bool Curl::is_non_whitespace(unsigned char ch) {
  return !std::isspace(ch);
}

inline char Curl::to_lower(unsigned char ch) { return std::tolower(ch); }

inline std::string_view Curl::trim(std::string_view source) {
  const auto first_non_whitespace =
      std::find_if(source.begin(), source.end(), &is_non_whitespace);
  const auto after_last_non_whitespace =
      std::find_if(source.rbegin(), source.rend(), &is_non_whitespace).base();
  const auto trimmed_length = after_last_non_whitespace - first_non_whitespace;
  return std::string_view(first_non_whitespace, trimmed_length);
}

inline std::size_t Curl::on_read_body(char *data, std::size_t,
                                      std::size_t length, void *user_data) {
  const auto request = static_cast<Request *>(user_data);
  request->response_body.append(data, length);
  return length;
}

inline void Curl::run() {
  int num_running_handles;
  int num_messages_remaining;
  CURLMsg *message;
  std::unique_lock<std::mutex> lock(mutex_);

  for (;;) {
    CHECK curl_multi_perform(multi_handle_, &num_running_handles);

    while ((message =
                curl_multi_info_read(multi_handle_, &num_messages_remaining))) {
      if (message->msg != CURLMSG_DONE) {
        continue;
      }

      auto *const request_handle = message->easy_handle;
      char *user_data;
      // TODO: error handling
      CHECK curl_easy_getinfo(request_handle, CURLINFO_PRIVATE, &user_data);
      auto &request = *reinterpret_cast<Request *>(user_data);

      // `request` is done.  If we got a response, then call the response
      // handler.  If an error occurred, then call the error handler.
      const auto result = message->data.result;
      if (result != CURLE_OK) {
        std::string error_message;
        error_message += "Error sending request with libcurl (";
        error_message += curl_easy_strerror(result);
        error_message += "): ";
        error_message += request.error_buffer;
        request.on_error(
            Error{Error::CURL_REQUEST_FAILURE, std::move(error_message)});
      } else {
        long status;
        // TODO: error handling
        CHECK curl_easy_getinfo(request_handle, CURLINFO_RESPONSE_CODE,
                                &status);
        HeaderReader reader(&request.response_headers_lower);
        request.on_response(static_cast<int>(status), reader,
                            std::move(request.response_body));
      }

      CHECK curl_multi_remove_handle(multi_handle_, request_handle);
      curl_easy_cleanup(request_handle);
      request_handles_.erase(request_handle);
      delete &request;
    }

    const int max_wait_milliseconds = 10 * 1000;
    lock.unlock();
    CHECK curl_multi_poll(multi_handle_, nullptr, 0, max_wait_milliseconds,
                          nullptr);
    lock.lock();

    // New requests might have been added while we were sleeping.
    for (; !new_handles_.empty(); new_handles_.pop_front()) {
      CURL *const handle = new_handles_.front();
      CHECK curl_multi_add_handle(multi_handle_, handle);
      request_handles_.insert(handle);
    }

    if (shutting_down_) {
      break;
    }
  }

  // We're shutting down.  Clean up any remaining request handles.
  for (const auto &handle : request_handles_) {
    char *user_data;
    // TODO: error handling
    CHECK curl_easy_getinfo(handle, CURLINFO_PRIVATE, &user_data);
    delete reinterpret_cast<Request *>(user_data);

    CHECK curl_multi_remove_handle(multi_handle_, handle);
  }

  request_handles_.clear();
  CHECK curl_multi_cleanup(multi_handle_);
  curl_global_cleanup();
}

inline Curl::Request::~Request() { curl_slist_free_all(request_headers); }

inline Curl::HeaderWriter::~HeaderWriter() { curl_slist_free_all(list_); }

inline curl_slist *Curl::HeaderWriter::release() {
  auto list = list_;
  list_ = nullptr;
  return list;
}

inline void Curl::HeaderWriter::set(std::string_view key,
                                    std::string_view value) {
  buffer_.clear();
  buffer_ += key;
  buffer_ += ": ";
  buffer_ += value;

  list_ = curl_slist_append(list_, buffer_.c_str());
}

inline Curl::HeaderReader::HeaderReader(
    std::unordered_map<std::string, std::string> *response_headers_lower)
    : response_headers_lower_(response_headers_lower) {}

inline std::optional<std::string_view> Curl::HeaderReader::lookup(
    std::string_view key) const {
  buffer_.clear();
  std::transform(key.begin(), key.end(), std::back_inserter(buffer_),
                 &to_lower);

  const auto found = response_headers_lower_->find(buffer_);
  if (found == response_headers_lower_->end()) {
    return std::nullopt;
  }
  return found->second;
}

inline void Curl::HeaderReader::visit(
    const std::function<void(std::string_view key, std::string_view value)>
        &visitor) const {
  for (const auto &[key, value] : *response_headers_lower_) {
    visitor(key, value);
  }
}

}  // namespace tracing
}  // namespace datadog
