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
  return [=](std::deque<Trace> traces) -> std::string {
    buffer->clear();
    buffer->str(std::string{});
    msgpack::pack(*buffer, traces);
    return buffer->str();
  };
}

AgentHttpEncoder::AgentHttpEncoder() : AgentHttpEncoder(config::tracer_version) {}

AgentHttpEncoder::AgentHttpEncoder(std::string version) {
  // Set up common headers and default encoder
  common_headers_ = {{"Content-Type", "application/msgpack"},
                     {"Datadog-Meta-Lang", "cpp"},
                     {"Datadog-Meta-Tracer-Version", version}};
  trace_encoder_ = msgpackEncoder();
}

const std::string agent_api_path = "/v0.3/traces";

std::string AgentHttpEncoder::path() { return agent_api_path; }

std::map<std::string, std::string> AgentHttpEncoder::headers(std::deque<Trace> &traces) {
  std::map<std::string, std::string> headers(common_headers_);
  headers["X-Datadog-Trace-Count"] = std::to_string(traces.size());
  return headers;
}

std::string AgentHttpEncoder::payload(std::deque<Trace> &traces) {
  return trace_encoder_(std::move(traces));
}

}  // namespace opentracing
}  // namespace datadog
