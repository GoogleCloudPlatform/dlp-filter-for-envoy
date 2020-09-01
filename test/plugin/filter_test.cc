// Copyright 2016-2019 Envoy Project Authors
// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "test/test_common/wasm_base.h"
#include "external/com_google_googleapis/google/privacy/dlp/v2/dlp.pb.h"

using testing::_;
using testing::AtLeast;
using testing::Eq;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using google::privacy::dlp::v2::InspectContentRequest;
using google::privacy::dlp::v2::InspectContentResponse;
using google::privacy::dlp::v2::InspectResult;
using google::privacy::dlp::v2::Finding;
using google::privacy::dlp::v2::InfoType;

MATCHER_P(MapEq, rhs, "") {
  const Envoy::ProtobufWkt::Struct& obj = arg;
  EXPECT_TRUE(rhs.size() > 0);
  for (auto const& entry : rhs) {
    EXPECT_EQ(obj.fields().at(entry.first).string_value(), entry.second);
  }
  return true;
}

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Wasm {

using Envoy::Extensions::Common::Wasm::CreateContextFn;
using Envoy::Extensions::Common::Wasm::Plugin;
using Envoy::Extensions::Common::Wasm::PluginSharedPtr;
using Envoy::Extensions::Common::Wasm::Wasm;
using Envoy::Extensions::Common::Wasm::WasmHandleSharedPtr;
using Envoy::Extensions::Common::Wasm::Context;
using proxy_wasm::ContextBase;
using GrpcService = envoy::config::core::v3::GrpcService;

// Based on https://github.com/envoyproxy/envoy-wasm/blob/master/test/extensions/filters/http/wasm/wasm_filter_test.cc
class TestFilter : public Envoy::Extensions::Common::Wasm::Context {
public:
  TestFilter(
      Wasm* wasm,
      uint32_t root_context_id,
      Envoy::Extensions::Common::Wasm::PluginSharedPtr plugin)
      : Envoy::Extensions::Common::Wasm::Context(wasm, root_context_id, plugin) {}

  using Context::log;
  proxy_wasm::WasmResult log(uint32_t level, absl::string_view message) override {
    log_(static_cast<spdlog::level::level_enum>(level), message);
    return proxy_wasm::WasmResult::Ok;
  }
  MOCK_METHOD2(log_, void(spdlog::level::level_enum level, absl::string_view message));
};

class TestRoot : public Envoy::Extensions::Common::Wasm::Context {
public:
  TestRoot(Wasm* wasm, const std::shared_ptr<Plugin>& plugin) : Context(wasm, plugin) {}
};

class WasmHttpFilterTest : public Common::Wasm::WasmHttpFilterTestBase<testing::TestWithParam<std::string>> {
public:
  WasmHttpFilterTest() = default;
  ~WasmHttpFilterTest() override = default;

  CreateContextFn createContextFn() {
    return [](Wasm* wasm, const std::shared_ptr<Plugin>& plugin) -> ContextBase* {
      return new TestRoot(wasm, plugin);
    };
  }

  void setupBaseConfig(const std::string& runtime, const std::string& code, CreateContextFn create_root,
                 std::string root_id = "", std::string vm_configuration = "",
                 std::string plugin_configuration = "") {
    envoy::extensions::wasm::v3::VmConfig vm_config;
    vm_config.set_vm_id("vm_id");
    vm_config.set_runtime(absl::StrCat("envoy.wasm.runtime.", runtime));
    ProtobufWkt::StringValue vm_configuration_string;
    vm_configuration_string.set_value(vm_configuration);
    vm_config.mutable_configuration()->PackFrom(vm_configuration_string);
    vm_config.mutable_code()->mutable_local()->set_inline_bytes(code);
    Api::ApiPtr api = Api::createApiForTest(stats_store_);
    scope_ = Stats::ScopeSharedPtr(stats_store_.createScope("wasm."));
    auto name = "";
    auto vm_id = "";
    plugin_ = std::make_shared<Extensions::Common::Wasm::Plugin>(
        name, root_id, vm_id, plugin_configuration, false,
        envoy::config::core::v3::TrafficDirection::INBOUND, local_info_, &listener_metadata_);
    // Passes ownership of root_context_.
    Extensions::Common::Wasm::createWasm(
        vm_config, plugin_, scope_, cluster_manager_, init_manager_, dispatcher_, random_, *api,
        lifecycle_notifier_, remote_data_provider_,
        [this](WasmHandleSharedPtr wasm) { wasm_ = wasm; }, create_root);
    if (wasm_) {
      wasm_ = getOrCreateThreadLocalWasm(
          wasm_, plugin_, dispatcher_,
          [this, create_root](Wasm* wasm, const std::shared_ptr<Plugin>& plugin) {
            root_context_ = static_cast<Context*>(create_root(wasm, plugin));
            return root_context_;
          });
    }
  }

  void setupTest(std::string root_id = "", std::string vm_configuration = "") {
    std::string code = TestEnvironment::readFileToStringForTest(
        TestEnvironment::runfilesDirectory("dlp_plugin")
        + std::string("/bazel-out/wasm-fastbuild/bin/filter.wasm"));
    std::string configuration = TestEnvironment::readFileToStringForTest(
            TestEnvironment::runfilesDirectory("dlp")
            + std::string("/test/plugin/testdata/dlp_plugin.config"));

    setupBaseConfig(GetParam(), code, createContextFn(), root_id, vm_configuration, configuration);
  }

  void setupFilter(const std::string root_id = "") {
    setupFilterBase<TestFilter>(root_id);
  }

  std::unique_ptr<Buffer::Instance> createInspectResult(std::string info_type_name) {
    InspectContentResponse inspect_content_response;
    inspect_content_response.mutable_result()
        ->add_findings()
        ->mutable_info_type()
        ->set_name(info_type_name);
    std::string response_string;
    EXPECT_TRUE(inspect_content_response.SerializeToString(&response_string));
    return std::make_unique<Buffer::OwnedImpl>(response_string);
  }

  TestRoot& root_context() { return *static_cast<TestRoot*>(root_context_); }
  TestFilter& filter() { return *static_cast<TestFilter*>(context_.get()); }
};

INSTANTIATE_TEST_SUITE_P(Runtimes, WasmHttpFilterTest, testing::Values("v8"));


TEST_P(WasmHttpFilterTest, GrpcCall) {
  setupTest("", "grpc_call");
  setupFilter();


  Grpc::MockAsyncRequest request;
  Grpc::RawAsyncRequestCallbacks* callbacks = nullptr;
  Grpc::MockAsyncClientManager client_manager;
  auto client_factory = std::make_unique<Grpc::MockAsyncClientFactory>();
  auto async_client = std::make_unique<Grpc::MockAsyncClient>();
  Tracing::Span* parent_span{};
  EXPECT_CALL(*async_client, sendRaw(_, _, _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view service_full_name, absl::string_view method_name,
                           Buffer::InstancePtr&& buffer, Grpc::RawAsyncRequestCallbacks& cb,
                           Tracing::Span& span, const Http::AsyncClient::RequestOptions& options)
                           -> Grpc::AsyncRequest* {
        EXPECT_EQ(service_full_name, "google.privacy.dlp.v2.DlpService");
        EXPECT_EQ(method_name, "InspectContent");
        InspectContentRequest inspect_content_request;
        Envoy::Grpc::Common::parseBufferInstance(std::move(buffer), inspect_content_request);
        const char expectation[] = "José, \0my ssn is 987-65-4321.";
        const std::string expectationString(expectation, sizeof(expectation) - 1);
        EXPECT_EQ(inspect_content_request.item().byte_item().data(), expectationString);
        callbacks = &cb;
        parent_span = &span;
        EXPECT_EQ(options.timeout->count(), 10000);
        return &request;
      }));
  EXPECT_CALL(*client_factory, create).WillOnce(Invoke([&]() -> Grpc::RawAsyncClientPtr {
    return std::move(async_client);
  }));
  EXPECT_CALL(cluster_manager_, grpcAsyncClientManager())
      .WillOnce(Invoke([&]() -> Grpc::AsyncClientManager& { return client_manager; }));
  EXPECT_CALL(client_manager, factoryForGrpcService(_, _, _))
      .WillOnce(Invoke([&](const GrpcService&, Stats::Scope&, bool) -> Grpc::AsyncClientFactoryPtr {
        return std::move(client_factory);
      }));
//  EXPECT_CALL(root_context(), log_(spdlog::level::warn, _)).Times(1);

  // Verify headers
  Http::TestRequestHeaderMapImpl request_headers{{":path", "/"}, {"server", "envoy"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter().decodeHeaders(request_headers, true));

  // Verify first part of the body is not inspected.
  const char data_part1[] = "José, \0my ssn is 98";
  Buffer::OwnedImpl buffer_part1 = Buffer::OwnedImpl(data_part1, sizeof(data_part1) - 1);
  EXPECT_EQ(Http::FilterDataStatus::Continue,
            filter().decodeData(buffer_part1, false));
  EXPECT_EQ(callbacks, nullptr);

  // Verify second part of the body triggers inspection.
  const char data_part2[] = "7-65-4321.";
  Buffer::OwnedImpl buffer_part2 = Buffer::OwnedImpl(data_part2, sizeof(data_part2) - 1);
  EXPECT_EQ(Http::FilterDataStatus::Continue,
            filter().decodeData(buffer_part2, true));
  EXPECT_NE(callbacks, nullptr);
  NiceMock<Tracing::MockSpan> span;
  if (callbacks) {
    callbacks->onSuccessRaw(
        createInspectResult("US_SOCIAL_SECURITY_NUMBER"),
        span);
  }
}



} // namespace Wasm
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy