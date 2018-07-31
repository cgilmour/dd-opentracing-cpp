#include "encoder.h"
#include "span.h"
#include "version_number.h"

#include <sstream>

namespace datadog {
namespace opentracing {

// Returns a TraceEncoder that uses msgpack to produce the encoded data.
TraceEncoder msgpackEncoder() {
  // Allocate and reuse a buffer instead of making a new buffer per call.
  auto buffer = std::make_shared<std::stringstream>();
  return [=](const std::deque<Trace> &traces) -> std::string {
    buffer->clear();
    buffer->str(std::string{});
    msgpack::pack(*buffer, traces);
    return buffer->str();
  };
}

AgentHttpEncoder::AgentHttpEncoder() {
  // Set up common headers and default encoder
  common_headers_ = {{"Content-Type", "application/msgpack"},
                     {"Datadog-Meta-Lang", "cpp"},
                     {"Datadog-Meta-Tracer-Version", config::tracer_version}};
  trace_encoder_ = msgpackEncoder();
}

const std::string agent_api_path = "/v0.3/traces";

const std::string AgentHttpEncoder::path() { return agent_api_path; }

void AgentHttpEncoder::addTrace(Trace trace) { traces_.push_back(std::move(trace)); }

void AgentHttpEncoder::clearTraces() { traces_.clear(); }

uint64_t AgentHttpEncoder::pendingTraces() { return traces_.size(); }

const std::map<std::string, std::string> AgentHttpEncoder::headers() {
  std::map<std::string, std::string> headers(common_headers_);
  headers["X-Datadog-Trace-Count"] = std::to_string(traces_.size());
  return headers;
}

const std::string AgentHttpEncoder::payload() { return trace_encoder_(traces_); }

}  // namespace opentracing
}  // namespace datadog
