#include <datadog/tracer_config.h>
#include <datadog/tracer.h>

#include <iostream>

int main() {
    namespace dd = datadog::tracing;
    dd::TracerConfig config;
    config.defaults.service = "benchsvc";
    const auto finalized = dd::finalize_config(config);
    if (const auto *error = finalized.if_error()) {
        std::cerr << *error << '\n';
        return int(error->code);
    }
    dd::Tracer tracer{*finalized};
}
