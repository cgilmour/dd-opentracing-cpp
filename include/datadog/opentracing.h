#ifndef DD_INCLUDE_OPENTRACING_TRACER_H
#define DD_INCLUDE_OPENTRACING_TRACER_H

#include <opentracing/tracer.h>

#include <deque>
#include <map>

namespace ot = opentracing;

namespace datadog {
namespace opentracing {

struct TracerOptions {
  // Hostname or IP address of the Datadog agent.
  std::string agent_host = "localhost";
  // Port on which the Datadog agent is running.
  uint32_t agent_port = 8126;
  // The name of the service being traced.
  // See:
  // https://help.datadoghq.com/hc/en-us/articles/115000702546-What-is-the-Difference-Between-Type-Service-Resource-and-Name-
  std::string service;
  // The type of service being traced.
  // (see above URL for definition)
  std::string type = "web";
  // What percentage of traces are sent to the agent, real number in [0, 1].
  // 0 = discard all traces, 1 = keep all traces.
  // Setting this lower reduces performance overhead at the cost of less data.
  double sample_rate = 1.0;
  // Max amount of time to wait between sending traces to agent, in ms. Agent discards traces older
  // than 10s, so that is the upper bound.
  int64_t write_period_ms = 1000;
};

// Forward-declarations for Writer
class SpanData;
using Trace = std::unique_ptr<std::vector<std::unique_ptr<SpanData>>>;

// HttpEncoder provides methods for a Writer to use when publishing
// a collection of traces to the Datadog agent.
class HttpEncoder {
 public:
  HttpEncoder() {}
  virtual ~HttpEncoder() {}

  virtual const std::string path() = 0;
  virtual void addTrace(Trace trace) = 0;
  virtual void clearTraces() = 0;
  virtual uint64_t pendingTraces() = 0;
  virtual const std::map<std::string, std::string> headers() = 0;
  virtual const std::string payload() = 0;
};

// A Writer is used to submit completed traces to the Datadog agent.
class Writer {
 public:
  Writer();

  virtual ~Writer() {}

  // Writes the given Trace.
  virtual void write(Trace trace) = 0;

 protected:
  // Used to encode the completed traces for submitting to the agent.
  std::unique_ptr<HttpEncoder> encoder_;
};

std::shared_ptr<ot::Tracer> makeTracer(const TracerOptions &options);
std::shared_ptr<ot::Tracer> makeTracer(std::shared_ptr<Writer> writer);

}  // namespace opentracing
}  // namespace datadog

#endif  // DD_INCLUDE_OPENTRACING_TRACER_H
