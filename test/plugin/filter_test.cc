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

#include <stdio.h>

#include "common/buffer/buffer_impl.h"
#include "common/http/message_impl.h"
#include "common/stats/isolated_store_impl.h"
#include "common/stream_info/stream_info_impl.h"

#include "extensions/common/wasm/wasm.h"
#include "extensions/common/wasm/wasm_state.h"
#include "extensions/filters/http/wasm/wasm_filter.h"

#include "test/mocks/grpc/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/environment.h"
#include "test/test_common/printers.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
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

using envoy::config::core::v3::TrafficDirection;
using Envoy::Extensions::Common::Wasm::PluginSharedPtr;
using Envoy::Extensions::Common::Wasm::Wasm;
using Envoy::Extensions::Common::Wasm::WasmHandleSharedPtr;
using GrpcService = envoy::config::core::v3::GrpcService;
using WasmFilterConfig = envoy::extensions::filters::http::wasm::v3::Wasm;

// Based on https://github.com/envoyproxy/envoy-wasm/blob/master/test/extensions/filters/http/wasm/wasm_filter_test.cc
class TestFilter : public Envoy::Extensions::Common::Wasm::Context {
public:
  TestFilter(
      Wasm* wasm,
      uint32_t root_context_id,
      Envoy::Extensions::Common::Wasm::PluginSharedPtr plugin)
      : Envoy::Extensions::Common::Wasm::Context(wasm, root_context_id, plugin) {}

  void scriptLog(spdlog::level::level_enum level, absl::string_view message) override {
    scriptLog_(level, message);
  }
  MOCK_METHOD2(scriptLog_, void(spdlog::level::level_enum level, absl::string_view message));
};

class TestRoot : public Envoy::Extensions::Common::Wasm::Context {
public:
  TestRoot() {}

  void scriptLog(spdlog::level::level_enum level, absl::string_view message) override {
    scriptLog_(level, message);
  }
  MOCK_METHOD2(scriptLog_, void(spdlog::level::level_enum level, absl::string_view message));
};

class WasmHttpFilterTest : public testing::TestWithParam<std::string> {
public:
  WasmHttpFilterTest() {}
  ~WasmHttpFilterTest() {}

  void SetUp() override { Envoy::Extensions::Common::Wasm::clearCodeCacheForTesting(false); }
  void TearDown() override { Envoy::Extensions::Common::Wasm::clearCodeCacheForTesting(false); }

  void setupConfig(const std::string& code, std::string root_id = "") {
    root_context_ = new TestRoot();
    WasmFilterConfig proto_config;
    proto_config.mutable_config()->mutable_vm_config()->set_vm_id("vm_id");
    proto_config.mutable_config()->mutable_vm_config()->set_runtime(
        absl::StrCat("envoy.wasm.runtime.", GetParam()));
    proto_config.mutable_config()
        ->mutable_vm_config()
        ->mutable_code()
        ->mutable_local()
        ->set_inline_bytes(code);
    Api::ApiPtr api = Api::createApiForTest(stats_store_);
    scope_ = Stats::ScopeSharedPtr(stats_store_.createScope("wasm."));
    auto name = "";
    auto vm_id = "";
    plugin_ = std::make_shared<Extensions::Common::Wasm::Plugin>(
        name, root_id, vm_id, TrafficDirection::INBOUND, local_info_, &listener_metadata_);
    Extensions::Common::Wasm::createWasmForTesting(
        proto_config.config().vm_config(), plugin_, scope_, cluster_manager_, init_manager_,
        dispatcher_, random_, *api, lifecycle_notifier_, remote_data_provider_,
        std::unique_ptr<Envoy::Extensions::Common::Wasm::Context>(root_context_),
        [this](WasmHandleSharedPtr wasm) { wasm_ = wasm; });
  }

  void setupFilter(const std::string root_id = "") {
    filter_ = std::make_unique<TestFilter>(
        wasm_->wasm().get(),
        wasm_->wasm()->getRootContext(root_id)->id(),
        plugin_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
    std::string configuration = TestEnvironment::readFileToStringForTest(
            TestEnvironment::runfilesDirectory("dlp")
            + std::string("/test/plugin/testdata/dlp_plugin.config"));
    root_context_->onConfigure(configuration, plugin_);
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

  Stats::IsolatedStoreImpl stats_store_;
  Stats::ScopeSharedPtr scope_;
  NiceMock<ThreadLocal::MockInstance> tls_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  NiceMock<Runtime::MockRandomGenerator> random_;
  NiceMock<Upstream::MockClusterManager> cluster_manager_;
  NiceMock<Init::MockManager> init_manager_;
  WasmHandleSharedPtr wasm_;
  PluginSharedPtr plugin_;
  std::unique_ptr<TestFilter> filter_;
  NiceMock<Envoy::Ssl::MockConnectionInfo> ssl_;
  NiceMock<Envoy::Network::MockConnection> connection_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> request_stream_info_;
  NiceMock<LocalInfo::MockLocalInfo> local_info_;
  NiceMock<Server::MockServerLifecycleNotifier> lifecycle_notifier_;
  envoy::config::core::v3::Metadata listener_metadata_;
  TestRoot* root_context_ = nullptr;
  Config::DataSource::RemoteAsyncDataProviderPtr remote_data_provider_;
};

INSTANTIATE_TEST_SUITE_P(Runtimes, WasmHttpFilterTest,
                         testing::Values("v8"));


TEST_P(WasmHttpFilterTest, GrpcCall) {
  setupConfig(TestEnvironment::readFileToStringForTest(
      TestEnvironment::runfilesDirectory("dlp_plugin") +
      std::string("/bazel-out/wasm-fastbuild/bin/filter.wasm")));
  EXPECT_CALL(*root_context_, scriptLog_(spdlog::level::debug, _)).Times(1);
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
  EXPECT_CALL(*root_context_, scriptLog_(spdlog::level::warn, _)).Times(1);

  // Verify headers
  Http::TestRequestHeaderMapImpl request_headers{{":path", "/"}, {"server", "envoy"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(request_headers, true));

  // Verify first part of the body is not inspected.
  const char data_part1[] = "José, \0my ssn is 98";
  Buffer::OwnedImpl buffer_part1 = Buffer::OwnedImpl(data_part1, sizeof(data_part1) - 1);
  EXPECT_EQ(Http::FilterDataStatus::Continue,
            filter_->decodeData(buffer_part1, false));
  EXPECT_EQ(callbacks, nullptr);

  // Verify second part of the body triggers inspection.
  const char data_part2[] = "7-65-4321.";
  Buffer::OwnedImpl buffer_part2 = Buffer::OwnedImpl(data_part2, sizeof(data_part2) - 1);
  EXPECT_EQ(Http::FilterDataStatus::Continue,
            filter_->decodeData(buffer_part2, true));
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