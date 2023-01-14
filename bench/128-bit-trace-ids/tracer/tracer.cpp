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
    if (count == 0) {
      zeroed.notify_all();
    }
  }

  void decrement() {
    std::lock_guard<std::mutex> lock{mutex};
    --count;
    if (count == 0) {
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

int main() {
  std::signal(SIGTERM, on_signal);
  std::signal(SIGINT, on_signal);

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

  const int requests_per_trace = 3;
  Semaphore sync{1};

  while (!shutting_down) {
    const auto before = std::chrono::steady_clock::now();
    const auto root = tracer.create_span();
    for (int i = 0; i < requests_per_trace; ++i) {
      const auto child = root.create_child();
      const auto result = client->post(
          upstream, [&](dd::DictWriter& writer) { child.inject(writer); },
          "dummy body",
          [&](int, const dd::DictReader&, std::string) { sync.decrement(); },
          [&](dd::Error) { sync.decrement(); });
      if (!result) {
        sync.decrement();
      }
      sync.wait();
      sync.reset(1);
    }

    const auto after = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(after -
                                                                      before)
                     .count()
              << '\n';
  }
}
