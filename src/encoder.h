#ifndef DD_OPENTRACING_ENCODER_H
#define DD_OPENTRACING_ENCODER_H

#include <datadog/opentracing.h>

namespace datadog {
namespace opentracing {

// A function that encodes a collection of traces.
typedef std::function<std::string(const std::deque<Trace> &)> TraceEncoder;

// A HTTP Encoder that provides the methods needed to publish
// traces to a Datadog Agent.
// This is initialized in a Writer and available in Writer subclasses.
class AgentHttpEncoder : public HttpEncoder {
 public:
  AgentHttpEncoder();
  ~AgentHttpEncoder() override {}

  // Returns the path that is used to submit HTTP requests to the agent.
  const std::string path() override;
  // Returns the HTTP headers that are required for the collection of traces.
  const std::map<std::string, std::string> headers(const std::deque<Trace> &traces) override;
  // Returns the encoded payload from the collection of traces.
  const std::string payload(const std::deque<Trace> &traces) override;

 private:
  // Holds the headers that are used for all HTTP requests.
  std::map<std::string, std::string> common_headers_;
  // The function that performs the encoding of the payload.
  TraceEncoder trace_encoder_;
};

}  // namespace opentracing
}  // namespace datadog

#endif  // DD_OPENTRACING_ENCODER_H
