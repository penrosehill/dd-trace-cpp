#include <datadog/cerr_logger.h>
#include <datadog/curl.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>

class Semaphore {
  std::mutex mutex;
  std::condition_variable zeroed;
  int count;

 public:
  explicit Semaphore(int count) : count(count) {}

  void reset(int new_count) {
    std::lock_guard<std::mutex> lock{mutex};
    count = new_count;
    if (count <= 0) {
      zeroed.notify_all();
    }
  }

  void decrement() {
    std::lock_guard<std::mutex> lock{mutex};
    --count;
    if (count <= 0) {
      zeroed.notify_all();
    }
  }

  void wait() {
    std::unique_lock lock{mutex};
    zeroed.wait(lock, [this]() { return count == 0; });
  }
};

volatile std::sig_atomic_t shutting_down = 0;
extern "C" void on_signal(int signal) {
  shutting_down = 1;
  std::signal(signal, SIG_IGN);
}

const auto pause_duration = std::chrono::milliseconds(10);
const int requests_per_trace = 1;

int with_tracing() {
  namespace dd = datadog::tracing;
  dd::TracerConfig config;
  config.defaults.service = "benchsvc";
  const auto logger = std::make_shared<dd::CerrLogger>();
  const auto client = std::make_shared<dd::Curl>(logger);
  config.agent.http_client = client;
  config.logger = logger;
  config.injection_styles.clear();
  config.injection_styles.push_back(dd::PropagationStyle::DATADOG);

  const auto finalized = dd::finalize_config(config);
  if (const auto* error = finalized.if_error()) {
    std::cerr << *error << '\n';
    return int(error->code);
  }

  dd::Tracer tracer{*finalized};
  dd::HTTPClient::URL upstream;
  upstream.scheme = "http";
  upstream.authority = std::getenv("UPSTREAM");
  upstream.path = "/";

  Semaphore sync{1};

  while (!shutting_down) {
    bool skip = false;
    const auto before = std::chrono::steady_clock::now();
    const auto root = tracer.create_span();
    for (int i = 0; i < requests_per_trace; ++i) {
      const auto child = root.create_child();
      const auto result = client->post(
          upstream, [&](dd::DictWriter& writer) { child.inject(writer); },
          "dummy body",
          [&](int, const dd::DictReader&, std::string) { sync.decrement(); },
          [&](dd::Error) {
            sync.decrement();
            skip = true;
          });
      if (!result) {
        sync.decrement();
        skip = true;
      }
      sync.wait();
      sync.reset(1);
    }

    if (!skip) {
      const auto after = std::chrono::steady_clock::now();
      std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(after -
                                                                        before)
                       .count()
                << '\n';
    }

    std::this_thread::sleep_for(pause_duration);
  }

  return 0;
}

int without_tracing() {
  namespace dd = datadog::tracing;
  const auto logger = std::make_shared<dd::CerrLogger>();
  const auto client = std::make_shared<dd::Curl>(logger);
  dd::HTTPClient::URL upstream;
  upstream.scheme = "http";
  upstream.authority = std::getenv("UPSTREAM");
  upstream.path = "/";

  Semaphore sync{1};

  while (!shutting_down) {
    bool skip = false;
    const auto before = std::chrono::steady_clock::now();
    for (int i = 0; i < requests_per_trace; ++i) {
      const auto result = client->post(
          upstream, [&](dd::DictWriter&) {}, "dummy body",
          [&](int, const dd::DictReader&, std::string) { sync.decrement(); },
          [&](dd::Error) {
            sync.decrement();
            skip = true;
          });
      if (!result) {
        sync.decrement();
        skip = true;
      }
      sync.wait();
      sync.reset(1);
    }

    if (!skip) {
      const auto after = std::chrono::steady_clock::now();
      std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(after -
                                                                        before)
                       .count()
                << '\n';
    }

    std::this_thread::sleep_for(pause_duration);
  }

  return 0;
}

int main() {
  std::signal(SIGTERM, on_signal);
  std::signal(SIGINT, on_signal);

  const auto env = std::getenv("BENCH_TRACING");
  if (!env) {
    std::cerr << "Missing BENCH_TRACING environment variable.\n";
    return 1;
  }
  if (env == std::string_view{"true"}) {
    return with_tracing();
  } else {
    return without_tracing();
  }
}
