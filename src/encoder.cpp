#include "encoder.h"
#include "span.h"
#include "version_number.h"

#include <sstream>

namespace datadog {
namespace opentracing {

TraceEncoder msgpackEncoder() {
  std::stringstream buffer;
  return [&](std::deque<Trace> traces) {
    buffer.clear();
    buffer.str(std::string{});
    msgpack::pack(buffer, traces);
    return buffer.str();
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

std::string AgentHttpEncoder::path() { return agent_api_path; }

std::map<std::string, std::string> AgentHttpEncoder::headers(std::deque<Trace> traces) {
  std::map<std::string, std::string> headers(common_headers_);
  headers["X-Datadog-Trace-Count"] = std::to_string(traces.size());
  return headers;
}

std::string AgentHttpEncoder::payload(std::deque<Trace> traces) {
  return trace_encoder_(std::move(traces));
}

}  // namespace opentracing
}  // namespace datadog
