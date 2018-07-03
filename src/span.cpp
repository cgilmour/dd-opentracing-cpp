#include "span.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace ot = opentracing;
using json = nlohmann::json;

namespace datadog {
namespace opentracing {

namespace {
const std::string datadog_span_type_tag = "span.type";
const std::string datadog_resource_name_tag = "resource.name";
const std::string datadog_service_name_tag = "service.name";
}  // namespace

Span::Span(std::shared_ptr<const Tracer> tracer, std::shared_ptr<SpanBuffer<Span>> buffer,
           TimeProvider get_time, uint64_t span_id, uint64_t trace_id, uint64_t parent_id,
           SpanContext context, TimePoint start_time, std::string span_service,
           std::string span_type, std::string span_name, ot::string_view resource,
           const ot::StartSpanOptions &options)
    : tracer_(std::move(tracer)),
      buffer_(std::move(buffer)),
      get_time_(get_time),
      span_id(span_id),
      trace_id(trace_id),
      parent_id(parent_id),
      context_(std::move(context)),
      start_time_(start_time),
      service(span_service),
      type(span_type),
      name("request " + span_name),
      resource(resource),
      error(0),
      start(std::chrono::duration_cast<std::chrono::nanoseconds>(
                start_time_.absolute_time.time_since_epoch())
                .count()),
      duration(0) {
  buffer_->registerSpan(*this);
}

Span::Span(Span &&other)
    : tracer_(other.tracer_),
      buffer_(other.buffer_),
      get_time_(other.get_time_),
      span_id(other.span_id),
      trace_id(other.trace_id),
      parent_id(other.parent_id),
      context_(std::move(other.context_)),
      start_time_(other.start_time_),
      service(other.service),
      type(other.type),
      name(other.name),
      resource(other.resource),
      error(other.error),
      start(other.start),
      duration(other.duration),
      meta(other.meta) {
  is_finished_ = (bool)other.is_finished_;  // Copy the value.
}

Span::~Span() {
  if (!is_finished_) {
    this->Finish();
  }
}

void Span::FinishWithOptions(const ot::FinishSpanOptions &finish_span_options) noexcept try {
  if (is_finished_.exchange(true)) {
    return;
  }
  std::lock_guard<std::mutex> lock{mutex_};
  // Set end time.
  auto end_time = get_time_();
  duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_).count();
  buffer_->finishSpan(std::move(*this));
} catch (const std::bad_alloc &) {
  // At least don't crash.
}

void Span::SetOperationName(ot::string_view operation_name) noexcept {
  std::lock_guard<std::mutex> lock_guard{mutex_};
  name = "request " + operation_name;
  resource = operation_name;
}

namespace {
// Visits and serializes an arbitrarily-nested variant type. Serialisation of value types is to
// string while any composite types are expressed in JSON. eg. string("fred") -> "fred"
// vector<string>{"felicity"} -> "[\"felicity\"]"
struct VariantVisitor {
  // Populated with the final result.
  std::string &result;
  VariantVisitor(std::string &result_) : result(result_) {}

 private:
  // Only set if VariantVisitor is recursing. Unfortunately we only really need an explicit
  // distinction (and all the conditionals below) to avoid the case of a simple string being
  // serialized to "\"string\"" - which is valid JSON but very silly never-the-less.
  json *json_result = nullptr;
  VariantVisitor(std::string &result_, json *json_result_)
      : result(result_), json_result(json_result_) {}

 public:
  void operator()(bool value) const {
    if (json_result != nullptr) {
      *json_result = value;
    } else {
      result = value ? "true" : "false";
    }
  }

  void operator()(double value) const {
    if (json_result != nullptr) {
      *json_result = value;
    } else {
      result = std::to_string(value);
    }
  }

  void operator()(int64_t value) const {
    if (json_result != nullptr) {
      *json_result = value;
    } else {
      result = std::to_string(value);
    }
  }

  void operator()(uint64_t value) const {
    if (json_result != nullptr) {
      *json_result = value;
    } else {
      result = std::to_string(value);
    }
  }

  void operator()(const std::string &value) const {
    if (json_result != nullptr) {
      *json_result = value;
    } else {
      result = value;
    }
  }

  void operator()(std::nullptr_t) const {
    if (json_result != nullptr) {
      *json_result = "nullptr";
    } else {
      result = "nullptr";
    }
  }

  void operator()(const char *value) const {
    if (json_result != nullptr) {
      *json_result = value;
    } else {
      result = std::string(value);
    }
  }

  void operator()(const std::vector<ot::Value> &values) const {
    json list;
    for (auto value : values) {
      json inner;
      std::string r;
      apply_visitor(VariantVisitor{r, &inner}, value);
      list.push_back(inner);
    }
    if (json_result != nullptr) {
      // We're a list in a dict/list.
      *json_result = list;
    } else {
      // We're a root object, so dump the string.
      result = list.dump();
    }
  }

  void operator()(const std::unordered_map<std::string, ot::Value> &value) const {
    json dict;
    for (auto pair : value) {
      json inner;
      std::string r;
      apply_visitor(VariantVisitor{r, &inner}, pair.second);
      dict[pair.first] = inner;
    }
    if (json_result != nullptr) {
      // We're a dict in a dict/list.
      *json_result = dict;
    } else {
      // We're a root object, so dump the string.
      result = dict.dump();
    }
  }
};
}  // namespace

void Span::SetTag(ot::string_view key, const ot::Value &value) noexcept {
  std::string result;
  apply_visitor(VariantVisitor{result}, value);
  {
    std::lock_guard<std::mutex> lock_guard{mutex_};
    if (key == datadog_span_type_tag) {
      type = result;
    } else if (key == datadog_resource_name_tag) {
      resource = result;
    } else if (key == datadog_service_name_tag) {
      service = result;
    } else {
      meta[key] = result;
    }
  }
}

void Span::SetBaggageItem(ot::string_view restricted_key, ot::string_view value) noexcept {
  context_.setBaggageItem(restricted_key, value);
}

std::string Span::BaggageItem(ot::string_view restricted_key) const noexcept {
  return context_.baggageItem(restricted_key);
}

void Span::Log(std::initializer_list<std::pair<ot::string_view, ot::Value>> fields) noexcept {}

const ot::SpanContext &Span::context() const noexcept { return context_; }

const ot::Tracer &Span::tracer() const noexcept { return *tracer_; }

uint64_t Span::traceId() const {
  return trace_id;  // Never modified, hence un-locked access.
}

uint64_t Span::spanId() const {
  return span_id;  // Never modified, hence un-locked access.
}

}  // namespace opentracing
}  // namespace datadog
