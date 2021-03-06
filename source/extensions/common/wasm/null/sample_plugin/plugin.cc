#include <string>
#include <unordered_map>

#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"
#else
#include "extensions/common/wasm/null/null_plugin.h"
#include "absl/base/casts.h"
#endif

// NOLINT(namespace-envoy)
START_WASM_PLUGIN(ExamplePlugin)

// For performance testing: see wasm_speed_test.cc.
class PluginRootContext : public RootContext {
public:
  PluginRootContext(uint32_t id, StringView root_id) : RootContext(id, root_id) {}
  void onTick() override;
};

void PluginRootContext::onTick() {
  uint64_t t;
  if (WasmResult::Ok != proxy_get_current_time_nanoseconds(&t)) {
    logError(std::string("bad proxy_get_current_time_nanoseconds result"));
  }
}

class PluginContext : public Context {
public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  FilterHeadersStatus onRequestHeaders(uint32_t headers) override;
  FilterDataStatus onRequestBody(size_t body_buffer_length, bool end_of_stream) override;
  void onLog() override;
  void onDone() override;
};
static RegisterContextFactory register_PluginContext(CONTEXT_FACTORY(PluginContext),
                                                     ROOT_FACTORY(PluginRootContext));

FilterHeadersStatus PluginContext::onRequestHeaders(uint32_t) {
  logDebug(std::string("onRequestHeaders ") + std::to_string(id()));
  auto path = getRequestHeader(":path");
  logInfo(std::string("header path ") + std::string(path->view()));
  if (addRequestHeader("newheader", "newheadervalue") != WasmResult::Ok) {
    logInfo("addRequestHeader failed");
  }
  replaceRequestHeader("server", "envoy-wasm");
  return FilterHeadersStatus::Continue;
}

FilterDataStatus PluginContext::onRequestBody(size_t body_buffer_length, bool /* end_of_stream */) {
  auto body = getBufferBytes(WasmBufferType::HttpRequestBody, 0, body_buffer_length);
  logError(std::string("onRequestBody ") + std::string(body->view()));
  return FilterDataStatus::Continue;
}

void PluginContext::onLog() {
  setFilterStateStringValue("wasm_state", "wasm_value");
  auto path = getRequestHeader(":path");
  if (path->view() == "/test_context") {
    logWarn("request.path: " + getProperty({"request", "path"}).value()->toString());
    logWarn("node.metadata: " +
            getProperty({"node", "metadata", "istio.io/metadata"}).value()->toString());
    logWarn("metadata: " + getProperty({"metadata", "filter_metadata", "envoy.filters.http.wasm",
                                        "wasm_request_get_key"})
                               .value()
                               ->toString());
    int64_t responseCode;
    if (getValue({"response", "code"}, &responseCode)) {
      logWarn("response.code: " + absl::StrCat(responseCode));
    }
    logWarn("state: " + getProperty({"filter_state", "wasm_state"}).value()->toString());
  } else {
    logWarn("onLog " + std::to_string(id()) + " " + std::string(path->view()));
  }
}

void PluginContext::onDone() { logWarn("onDone " + std::to_string(id())); }

END_WASM_PLUGIN
