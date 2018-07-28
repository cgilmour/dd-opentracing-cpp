#ifndef DD_OPENTRACING_ENCODER_H
#define DD_OPENTRACING_ENCODER_H

#include <datadog/opentracing.h>

namespace datadog {
namespace opentracing {

typedef std::function<std::string(std::deque<Trace>)> TraceEncoder;

class AgentHttpEncoder : public HttpEncoder {
 public:
  AgentHttpEncoder();
  AgentHttpEncoder(std::string version);
  ~AgentHttpEncoder() override {}

  std::string path() override;
  std::map<std::string, std::string> headers(std::deque<Trace> &traces) override;
  std::string payload(std::deque<Trace> &traces) override;

 private:
  std::map<std::string, std::string> common_headers_;
  TraceEncoder trace_encoder_;
};

}  // namespace opentracing
}  // namespace datadog

#endif  // DD_OPENTRACING_ENCODER_H
