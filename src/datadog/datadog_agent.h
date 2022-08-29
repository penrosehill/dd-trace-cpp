#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "collector.h"
#include "event_scheduler.h"
#include "http_client.h"

namespace datadog {
namespace tracing {

struct FinalizedDatadogAgentConfig;
class Logger;
struct SpanData;
class TraceSampler;

class DatadogAgent : public Collector {
 public:
  struct TraceChunk {
    std::vector<std::unique_ptr<SpanData>> spans;
    std::shared_ptr<TraceSampler> response_handler;
  };

 private:
  std::mutex mutex_;
  std::shared_ptr<Logger> logger_;
  // `incoming_trace_chunks_` are what `send` appends to.
  std::vector<TraceChunk> incoming_trace_chunks_;
  // `outgoing_trace_chunks_` are what `flush` consumes from.
  std::vector<TraceChunk> outgoing_trace_chunks_;
  HTTPClient::URL traces_endpoint_;
  std::shared_ptr<HTTPClient> http_client_;
  std::shared_ptr<EventScheduler> event_scheduler_;
  EventScheduler::Cancel cancel_scheduled_flush_;

  void flush();

 public:
  DatadogAgent(const FinalizedDatadogAgentConfig&,
               const std::shared_ptr<Logger>&);
  ~DatadogAgent();

  virtual Expected<void> send(
      std::vector<std::unique_ptr<SpanData>>&& spans,
      const std::shared_ptr<TraceSampler>& response_handler) override;
};

}  // namespace tracing
}  // namespace datadog