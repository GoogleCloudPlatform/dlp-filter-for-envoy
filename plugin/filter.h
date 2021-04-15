// Copyright 2021 Google LLC
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

#include <string>
#include <unordered_set>
#define ASSERT(_X) assert(_X)

#include "plugin/config.pb.h"
#include "buffer/buffer.h"
#include "sampling/sampling.h"
#include "google/privacy/dlp/v2/dlp.pb.h"
#include "google/protobuf/util/json_util.h"

static constexpr char Separator[] = ":";
static constexpr char VersionKey[] = "version";

using google::protobuf::util::Status;
using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::error::Code;
using google::privacy::dlp::v2::ContentItem;
using google::privacy::dlp::v2::InspectContentRequest;
using google::privacy::dlp::v2::InspectContentResponse;
using google::privacy::dlp::v2::Container;
using google::dlp_filter::Buffer;
using google::dlp_filter::Sampler;
using google::dlp_filter::PassthroughSampler;
using google::dlp_filter::ProbabilisticSampler;

#ifndef NULL_PLUGIN

#include "proxy_wasm_intrinsics.h"

#else

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
namespace dlp {

#endif

class NodeInfoContainerDetails {
 public:
  NodeInfoContainerDetails(
      const std::string mesh_id,
      const std::string ns,
      const std::string workload_name,
      const std::string name,
      const std::map<std::string, std::string> labels) :
      mesh_id_(mesh_id),
      namespace_(ns),
      workload_name_(workload_name),
      name_(name),
      labels_(labels) {}

  std::string fullPath() {
    std::string full_path = rootPath();
    full_path += Separator;
    full_path += relativePath();
    return full_path;
  }

  std::string rootPath() {
    std::string root_path = mesh_id_;
    root_path += Separator;
    root_path += namespace_;
    return root_path;
  }

  std::string relativePath() {
    std::string relativePath = workload_name_;
    relativePath += Separator;
    relativePath += name_;
    return relativePath;
  }

  std::string getVersion() {
    if (labels().count(VersionKey)) {
      return labels().at(VersionKey);;
    } else {
      return "";
    }
  }

  std::map<std::string, std::string> labels() {
    return labels_;
  }

 private:
  const std::string mesh_id_;
  const std::string namespace_;
  const std::string workload_name_;
  const std::string name_;
  const std::map<std::string, std::string> labels_;
};

class DlpRootContext : public RootContext {
 public:
  explicit DlpRootContext(uint32_t id, std::string_view root_id) : RootContext(id, root_id) {}
  bool onConfigure(size_t) override;
  size_t getMaxRequestSize();
  void inspect(Buffer* buffer);

 private:
  Status createSampler();
  Status extractPartialLocalNodeInfo(
    std::shared_ptr<NodeInfoContainerDetails>& details);
  std::string getFormattedLabel(const std::string& label);
  void inspectContent(Buffer* buffer);
  InspectContentRequest createInspectContentRequest(Buffer* buffer) const;

  // Parsed filter config
  ::dlp::PluginConfig config_;
  // InspectContent parent name consisting of parent/<project_id>/locations/<location_id>
  std::string parent_;
  // DLP Destination grpc config
  std::string grpc_service_string_;
  // NodeInfo from metadata_exchange filter
  std::shared_ptr<NodeInfoContainerDetails> local_node_info_;
  // Sampling strategy, based on configuration
  std::unique_ptr<Sampler> sampler_;
};

// Per-stream context.
class DlpContext : public Context {
 public:
  explicit DlpContext(uint32_t id, RootContext* root)
    : Context(id, root),
      request_buffer_(std::make_unique<Buffer>(dynamic_cast<DlpRootContext*>(root)->getMaxRequestSize())),
      response_buffer_(std::make_unique<Buffer>(dynamic_cast<DlpRootContext*>(root)->getMaxRequestSize()))
      {}
      FilterDataStatus onRequestBody(size_t body_buffer_length, bool end_of_stream) override;
      FilterDataStatus onResponseBody(size_t body_buffer_length, bool end_of_stream) override;

 private:
  void maybeInspect(Buffer* buffer, bool end_of_stream);
  void reportExceeded(size_t buffer_size);

  std::unique_ptr<Buffer> request_buffer_;
  std::unique_ptr<Buffer> response_buffer_;
  inline DlpRootContext* rootContext() {
    return dynamic_cast<DlpRootContext*>(this->root());
  };

};

#ifdef NULL_PLUGIN

}  // namespace dlp
}  // namespace null_plugin
}  // namespace proxy_wasm

#endif