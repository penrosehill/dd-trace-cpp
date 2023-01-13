#include <datadog/cerr_logger.h>
#include <datadog/curl.h>
#include <datadog/tracer_config.h>
#include <datadog/tracer.h>

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    namespace dd = datadog::tracing;
    dd::TracerConfig config;
    config.defaults.service = "benchsvc";
    const auto logger = std::make_shared<dd::CerrLogger>();
    const auto client = std::make_shared<dd::Curl>(logger);
    config.agent.http_client = client;
    config.logger = logger;

    const auto finalized = dd::finalize_config(config);
    if (const auto *error = finalized.if_error()) {
        std::cerr << *error << '\n';
        return int(error->code);
    }

    dd::Tracer tracer{*finalized};
    for (int i = 0; i < 100; ++i) {
        tracer.create_span();
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    client->drain(std::chrono::steady_clock::now() + std::chrono::seconds(3));
}
