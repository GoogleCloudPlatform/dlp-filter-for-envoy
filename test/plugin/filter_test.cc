// Copyright 2020-2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/filter.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "include/proxy-wasm/context.h"
#include "include/proxy-wasm/null.h"

using testing::_;
using testing::Invoke;

namespace proxy_wasm {
namespace null_plugin {
namespace dlp {

NullPluginRegistry* context_registry_;
RegisterNullVmPluginFactory register_dlp_plugin("dlp", []() {
  return std::make_unique<NullPlugin>(dlp::context_registry_);
});

class MockContext : public proxy_wasm::ContextBase {
 public:
  MockContext(WasmBase* wasm) : ContextBase(wasm) {}

  MOCK_METHOD(BufferInterface*, getBuffer, (WasmBufferType));
  MOCK_METHOD(WasmResult, log, (uint32_t, std::string_view));
  MOCK_METHOD(WasmResult, getHeaderMapValue,
              (WasmHeaderMapType /* type */, std::string_view /* key */,
                  std::string_view * /*result */));
  MOCK_METHOD(WasmResult, getProperty,
              (std::string_view /* path */, std::string * /* result */));
  MOCK_METHOD(WasmResult, grpcCall,
              (std::string_view /* grpc_service */, std::string_view /* service_name */,
                  std::string_view /* method_name */, const Pairs & /* initial_metadata */,
                  std::string_view /* request */, std::chrono::milliseconds /* timeout */,
                  GrpcToken * /* token_ptr */));
  MOCK_METHOD(WasmResult, sendLocalResponse,
              (uint32_t /* response_code */, std::string_view /* body */,
                  Pairs /* additional_headers */, uint32_t /* grpc_status */,
                  std::string_view /* details */));
};

class DlpTest : public ::testing::Test {
 protected:
  DlpTest() {
    // Initialize test VM
    test_vm_ = createNullVm();
    wasm_base_ =
        std::make_unique<WasmBase>(std::move(test_vm_), "test-vm", "", "");
    wasm_base_->initialize("dlp");

    // Initialize host side context
    mock_context_ = std::make_unique<MockContext>(wasm_base_.get());
    current_context_ = mock_context_.get();

    ON_CALL(*mock_context_, log(_, _))
        .WillByDefault([](uint32_t, std::string_view m) {
          std::cerr << m << "\n";
          return WasmResult::Ok;
        });

    ON_CALL(*mock_context_, getProperty(_, _))
        .WillByDefault([](std::string_view m, std::string*) {
          std::cerr << m << "\n";
          return WasmResult::Ok;
        });

    ON_CALL(*mock_context_, getHeaderMapValue(WasmHeaderMapType::RequestHeaders,
                                              _, _))
        .WillByDefault([&](WasmHeaderMapType, std::string_view header,
                           std::string_view* result) {
          if (header == ":path") {
            *result = path_;
          }
          if (header == ":method") {
            *result = method_;
          }
          if (header == "authorization") {
            *result = authorization_header_;
          }
          return WasmResult::Ok;
        });

    // Initialize Wasm sandbox context
    root_context_ = std::make_unique<DlpRootContext>(0, "");
    context_ = std::make_unique<DlpContext>(1, root_context_.get());
  }
  ~DlpTest() override {}

  std::unique_ptr<WasmBase> wasm_base_;
  std::unique_ptr<WasmVm> test_vm_;
  std::unique_ptr<MockContext> mock_context_;

  std::unique_ptr<DlpRootContext> root_context_;
  std::unique_ptr<DlpContext> context_;

  std::string path_;
  std::string method_;
  std::string cred_;
  std::string authorization_header_;
};

TEST_F(DlpTest, ValidRequest) {
  std::string configuration = R"(
{
  "inspect": {
    "destination": {
      "operation": {
        "store_local": {
          "project_id": "{project_id}",
        }
      }
    },
    "sampling": {
      "probability": {
        "numerator": 100,
        "denominator": "HUNDRED"
      }
    },
    "max_request_size_bytes": 500000
  }
})";

  BufferBase configBuffer;
  configBuffer.set({configuration.data(), configuration.size()});
  EXPECT_CALL(*mock_context_, getBuffer(WasmBufferType::PluginConfiguration))
      .WillOnce([&configBuffer](WasmBufferType) { return &configBuffer; });
  EXPECT_TRUE(root_context_->onConfigure(configuration.size()));

  // Verify headers
  EXPECT_EQ(FilterHeadersStatus::Continue, context_->onRequestHeaders(0, true));

  // Verify first part of the body is not inspected.
  const char data_part1[] = "José, \0my ssn is 98";
  BufferBase dataBuffer;
  dataBuffer.set({data_part1, sizeof(data_part1) - 1});
  EXPECT_CALL(*mock_context_, getBuffer(WasmBufferType::HttpRequestBody))
      .WillOnce([&dataBuffer](WasmBufferType) { return &dataBuffer; });
  EXPECT_EQ(FilterDataStatus::Continue,
            context_->onRequestBody(sizeof(data_part1) - 1, false));

  // Verify second part of the body triggers inspection.
  const char data_part2[] = "7-65-4321.";
  dataBuffer.set({data_part2, sizeof(data_part2) - 1});
  EXPECT_CALL(*mock_context_, getBuffer(WasmBufferType::HttpRequestBody))
      .WillOnce([&dataBuffer](WasmBufferType) { return &dataBuffer; });
  GrpcToken* token;
  EXPECT_CALL(*mock_context_, grpcCall(_, _, _, _, _, _, _))
      .WillOnce(Invoke([&](std::string_view grpc_service,
                           std::string_view service_name,
                           std::string_view method_name,
                           const Pairs& initial_metadata,
                           std::string_view request,
                           std::chrono::milliseconds timeout,
                           GrpcToken* token_ptr)
                           -> WasmResult {
        EXPECT_EQ(service_name, "google.privacy.dlp.v2.DlpService");
        EXPECT_EQ(method_name, "InspectContent");
        InspectContentRequest inspect_content_request;
        inspect_content_request.ParseFromString(std::string(request));
        const char expectation[] = "José, \0my ssn is 987-65-4321.";
        const std::string expectationString(expectation, sizeof(expectation) - 1);
        EXPECT_EQ(inspect_content_request.item().byte_item().data(), expectationString);
        EXPECT_EQ(timeout.count(), 10000);
        // Memorize token of this call
        token = token_ptr;
        return WasmResult::Ok;
      }));
  EXPECT_EQ(FilterDataStatus::Continue,
            context_->onRequestBody(sizeof(data_part2) - 1, true));
  EXPECT_NE(token, nullptr);
}

}  // namespace dlp
}  // namespace null_plugin
}  // namespace proxy_wasm