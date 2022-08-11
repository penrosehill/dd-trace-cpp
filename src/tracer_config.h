#pragma once

#include "propagation_styles.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace datadog {
namespace tracing {

class Collector;
class SpanSampler;
class TraceSampler;
class Logger;

struct CollectorConfig {};  // TODO: elsewhere
struct SpanSamplerConfig {};  // TODO: elsewhere
struct TraceSamplerConfig {};  // TODO: elsewhere

struct TracerConfig {
    struct {
        std::string service;
        std::optional<std::string> service_type;
        std::optional<std::string> environment;
        std::optional<std::string> version;
        std::optional<std::string> operation;  // a.k.a. name
        std::optional<std::string> operation_override;  // TODO: will we still need this?
        // ...
    } spans;

    std::variant<CollectorConfig, std::shared_ptr<Collector>> collector;
    std::variant<TraceSamplerConfig, std::shared_ptr<TraceSampler>> trace_sampler;
    std::variant<SpanSamplerConfig, std::shared_ptr<SpanSampler>> span_sampler;
    std::shared_ptr<Logger> logger;

    PropagationStyles injection_styles;
    PropagationStyles extraction_styles;
};

}  // namespace tracing
}  // namespace datadog