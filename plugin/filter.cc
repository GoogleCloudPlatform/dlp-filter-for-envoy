// Copyright 2020 Google LLC
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

#include "proxy_wasm_intrinsics.h"
#include "google/privacy/dlp/v2/dlp.pb.h"
#include "config.pb.h"
#include "buffer/buffer.h"
#include "sampling/sampling.h"
#include "google/protobuf/util/json_util.h"

static constexpr char DlpServiceName[] = "google.privacy.dlp.v2.DlpService";
static constexpr char InspectContentMethodName[] = "InspectContent";
static const int Timeout10s = 10000;
static constexpr char DlpUri[] = "dlp.googleapis.com";
static constexpr char DlpStat[] = "dlp_stat";
static constexpr char Separator[] = ":";
static constexpr char XGoogRequestParams[] = "x-goog-request-params";
static constexpr char VersionKey[] = "version";
static constexpr char ParentPrefix[] = "projects/";
static constexpr char LocationsInfix[] = "/locations/";
static constexpr char LocationGlobalSuffix[] = "global";
static const std::set<std::string> DefaultLabels{ "app", "version" };

// Number of messages sent for inspection
static Counter<>* inspected_ = Counter<>::New("dlp_stat_inspected");
// Number of messages not inspected (should be 0 if sampling is 100%)
static Counter<>* not_inspected_ = Counter<>::New("dlp_stat_not_inspected");
// Sum of all bytes sent for inspection (might differ from actually inspected
// bytes in case there was an issue on Cloud DLP side)
static Counter<>* total_bytes_inspected_ = Counter<>::New("dlp_stat_total_bytes_inspected");
// Sum of all bytes that could have been sent for inspection but weren't
// due to sampling or rpc-related issues.
static Counter<>* total_bytes_not_inspected_ = Counter<>::New("dlp_stat_total_bytes_not_inspected");
// When request sent to the server is too large
static Counter<>* request_too_large_ = Counter<>::New("dlp_stat_request_too_large");
// Any other error
static Counter<>* filter_error_ = Counter<>::New("dlp_stat_filter_error");
// Number of times grpc returned an error (grpc_status was different than 0)
static Counter<>* grpc_error_ = Counter<>::New("dlp_stat_grpc_error");
// Number of times particular grpc_status was returned
static Counter<int>* grpc_status_ = Counter<int>::New("grpc_status", "dlp_stat_code");
// Number of findings returned found by DLP
static Counter<>* findings_ = Counter<>::New("dlp_stat_findings");

using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::error::Code;
using google::protobuf::util::Status;
using google::privacy::dlp::v2::ContentItem;
using google::privacy::dlp::v2::InspectContentRequest;
using google::privacy::dlp::v2::InspectContentResponse;
using google::privacy::dlp::v2::Container;
using google::dlp_filter::Buffer;
using google::dlp_filter::Sampler;
using google::dlp_filter::PassthroughSampler;
using google::dlp_filter::ProbabilisticSampler;

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

class InspectContentCallHandler : public GrpcCallHandler<google::protobuf::Empty> {
 public:
  InspectContentCallHandler(
      std::string parent,
      size_t body_size,
      std::shared_ptr<NodeInfoContainerDetails> local_node_info)
    : parent_(parent),
      inspected_body_size_(body_size),
      local_node_info_(local_node_info)
      {}

  void onSuccess(size_t body_size) override {
    grpc_status_->record(1, static_cast<int>(GrpcStatus::Ok));
    WasmDataPtr response_data = getBufferBytes(WasmBufferType::GrpcReceiveBuffer, 0, body_size);
    inspected_->record(1);
    total_bytes_inspected_->record(inspected_body_size_);
    const InspectContentResponse& response = response_data->proto<InspectContentResponse>();
    if (response.has_result() && response.result().findings_size() > 0) {
      findings_->record(response.result().findings_size());
      for (auto& finding : response.result().findings()) {
        const std::string info_type = finding.info_type().name().data();
        std::string log_line = local_node_info_->fullPath();
        log_line += Separator;
        log_line += "DLP_DETECTED";
        log_line += Separator;
        log_line += info_type;
        logWarn(log_line);
      }
    } else {
      std::string log_line = local_node_info_->fullPath();
      log_line += Separator;
      log_line += "DLP_NOT_DETECTED";
      logWarn(log_line);
    }
  }

  void onFailure(GrpcStatus status) override {
    std::function<void(GrpcStatus status, size_t body_size)> callback =
        [&](GrpcStatus status, size_t body_size) {
      grpc_status_->record(1, static_cast<int>(status));
      WasmDataPtr response_data = getBufferBytes(WasmBufferType::GrpcReceiveBuffer, 0, body_size);
      not_inspected_->record(1);
      total_bytes_not_inspected_->record(inspected_body_size_);
      grpc_error_->record(1);
      logWarn(std::string("InspectContent call to DLP failed with gRPC status code: ") +
        std::to_string(static_cast<int>(status)));
    };
  }

 private:
  std::string parent_;
  size_t inspected_body_size_;
  std::shared_ptr<NodeInfoContainerDetails> local_node_info_;
};

class DlpRootContext : public RootContext {
public:
  explicit DlpRootContext(uint32_t id, StringView root_id) : RootContext(id, root_id)  {}

  // Loads WASM configuration
  bool onConfigure(size_t config_size) override {
    // Load filter config
    logInfo("Starting onConfigure");
    const WasmDataPtr configuration = getBufferBytes(WasmBufferType::PluginConfiguration, 0, config_size);
    JsonParseOptions json_options;
    const Status options_status = JsonStringToMessage(
        configuration->toString(),
        &config_,
        json_options);
    if (options_status != Status::OK) {
      logWarn("Cannot parse plugin configuration JSON string: " + configuration->toString());
      return false;
    } else if (!config_.has_inspect()) {
      logWarn("Missing inspect configuration: " + configuration->toString());
      return false;
    } else if (!config_.inspect().has_destination()) {
      logWarn("Missing destination configuration: " + configuration->toString());
      return false;
    } else if (!config_.inspect().destination().has_operation()) {
      logWarn("Missing operation configuration: " + configuration->toString());
      return false;
    } else if (!config_.inspect().destination().operation().has_store_local()) {
      logWarn("At least one operation type has to be defined: " + configuration->toString());
      return false;
    }

    // With this configuration, from user perspective, findings are stored locally in proxy logs.
    // Technically we call inspect content operation and write the findings from the response into
    // logs.
    const dlp::StoreFindingsLocally& local_config =
        config_.inspect().destination().operation().store_local();
    if (local_config.project_id().empty()) {
      logWarn("Missing project_id: " + configuration->toString());
      return false;
    }
    std::string parent = std::string(ParentPrefix);
    parent += local_config.project_id();
    parent += LocationsInfix;
    if (local_config.location_id().empty()) {
      parent += LocationGlobalSuffix;
    } else {
      parent += local_config.location_id();
    }
    parent_ = parent;

    const Status sampler_status = createSampler();
    if (sampler_status != Status::OK) {
      logWarn("Cannot load sampler configuration: "
              + sampler_status.error_message().as_string());
      return false;
    }

    // Load NodeInfo
    const Status node_info_status =
        extractPartialLocalNodeInfo(local_node_info_);
    if (node_info_status != Status::OK) {
      logWarn("Cannot load NodeInfo: " + node_info_status.error_message().as_string());
      return false;
    }

    // Prepare grpc_service string to be used in grpc call
    GrpcService grpc_service;
    GrpcService_GoogleGrpc* google_grpc = grpc_service.mutable_google_grpc();
    if (config_.inspect().destination().has_grpc_config()) {
      google_grpc->MergeFrom(config_.inspect().destination().grpc_config());
    } else {
      google_grpc->set_target_uri(DlpUri);
      google_grpc->mutable_channel_credentials()->mutable_google_default();
      google_grpc->set_stat_prefix(DlpStat);
    }
    if (!grpc_service.SerializeToString(&grpc_service_string_)) {
      logWarn("Cannot serialize service config to string.");
      return false;
    }
    logDebug("Configuration successful.");
    return true;
  }

  Status createSampler() {
    if (config_.inspect().has_sampling()
        && config_.inspect().sampling().has_probability()) {
      const dlp::FractionalPercent& percent = config_.inspect().sampling().probability();
      unsigned int denominator;
      switch (percent.denominator()) {
        case dlp::FractionalPercent_DenominatorType_HUNDRED:
          denominator = 100;
          break;
        case dlp::FractionalPercent_DenominatorType_TEN_THOUSAND:
          denominator = 10000;
          break;
        case dlp::FractionalPercent_DenominatorType_MILLION:
          denominator = 1000000;
          break;
        default:
          return Status(
              Code::INVALID_ARGUMENT,
              std::string("Unknown sampling probability denominator: ")
              + std::to_string(config_.inspect().sampling().probability().denominator()));
      }
      const ProbabilisticSampler::CreateStatus status =
          ProbabilisticSampler::create(percent.numerator(), denominator, sampler_);
      if (status != ProbabilisticSampler::CreateStatus::Success) {
        return Status(
            Code::INVALID_ARGUMENT,
            std::string("Cannot create probabilistic sampler ")
            + std::to_string(percent.numerator()) + std::string("/") + std::to_string(denominator)
            + std::string(". Error code: " + std::to_string(status)));
      }
    } else {
      sampler_ = PassthroughSampler::create();
    }
    return Status::OK;
  }

  // Loads NodeInfo from metadata_exchange metadata.
  Status extractPartialLocalNodeInfo(
      std::shared_ptr<NodeInfoContainerDetails>& details) {
    std::map<std::string, std::string> labels;
    std::string value;
    for (const auto& label : DefaultLabels) {
        if (getValue({"node", "metadata", "LABELS", label}, &value)) {
          labels.insert(std::pair<std::string, std::string>(label, value));
        }
    }
    details.reset(
        new NodeInfoContainerDetails(
            getValue({"node", "metadata", "MESH_ID"}, &value) ? value : "",
            getValue({"node", "metadata", "NAMESPACE"}, &value) ? value : "",
            getValue({"node", "metadata", "WORKLOAD_NAME"}, &value) ? value : "",
            getValue({"node", "metadata", "NAME"}, &value) ? value : "",
            labels));
    return Status::OK;
  }

  std::string getFormattedLabel(const std::string& label) {
    int len = std::min(static_cast<unsigned long>(63), label.length());
    std::string canonical_label = label.substr(0, len);
    if (len > 0 && !std::isalpha(canonical_label[0])) {
      canonical_label[0] = 'x';
    }
    std::for_each(canonical_label.begin(), canonical_label.end(), [](char& c){
      if (std::isupper(c)) {
        c = std::tolower(c);
      }
      if (!(std::isalnum(c) || c == '_' || c == '-')) {
        c = '_';
      }
    });
    return canonical_label;
  }

  void inspect(Buffer* buffer) {
    inspectContent(buffer);
  }

  // Calls Cloud DLP endpoint InspectContent to inspect provided body
  void inspectContent(Buffer* buffer) {
    if (!sampler_->sample()) {
      not_inspected_->record(1);
      total_bytes_not_inspected_->record(buffer->appendedSize());
      return;
    }

    // Prepare request to be sent for inspection
    InspectContentRequest request = createInspectContentRequest(buffer);
    std::unique_ptr<GrpcCallHandlerBase> inspect_content_call_handler =
        std::make_unique<InspectContentCallHandler>(
            InspectContentCallHandler(
                parent_,
                buffer->size(),
                local_node_info_));

    HeaderStringPairs initial_metadata;
    initial_metadata.push_back(std::pair("parent", parent_));

    // Send grpc request
    grpcCallHandler(
        grpc_service_string_,
        DlpServiceName,
        InspectContentMethodName,
        initial_metadata,
        request,
        Timeout10s,
        std::move(inspect_content_call_handler));
  }

  InspectContentRequest createInspectContentRequest(Buffer* buffer) const {
    InspectContentRequest request;
    // Passing data along with its size to correctly handle null bytes in the array
    request.mutable_item()->mutable_byte_item()->set_data(buffer->data(), buffer->size());
    request.set_parent(parent_);
    if (!config_.inspect().destination().operation().store_local().inspect_template_name().empty()) {
      request.set_inspect_template_name(
          config_.inspect().destination().operation().store_local().inspect_template_name());
    }
    if (!config_.inspect().destination().operation().store_local().location_id().empty()) {
      request.set_location_id(
          config_.inspect().destination().operation().store_local().location_id());
    }
    size_t inspected_body_size = buffer->size();
    return request;
  }

  size_t getMaxRequestSize() {
    return config_.inspect().max_request_size_bytes();
  }

 private:
  // Parsed filter config
  dlp::PluginConfig config_;
  // InspectContent parent name consisting of parent/<project_id>/locations/<location_id>
  std::string parent_;
  // DLP Destination grpc config
  std::string grpc_service_string_;
  // NodeInfo from metadata_exchange filter
  std::shared_ptr<NodeInfoContainerDetails> local_node_info_;
  // Sampling strategy, based on configuration
  std::unique_ptr<Sampler> sampler_;
};

// Context created per stream
class DlpContext : public Context {
public:
  explicit DlpContext(uint32_t id, RootContext* root)
    : Context(id, root),
      request_buffer_(std::make_unique<Buffer>(dynamic_cast<DlpRootContext*>(root)->getMaxRequestSize())),
      response_buffer_(std::make_unique<Buffer>(dynamic_cast<DlpRootContext*>(root)->getMaxRequestSize()))
      {}

  // Captures request body and passes it for inspection at DlpRootContext level
  FilterDataStatus onRequestBody(size_t body_buffer_length, bool end_of_stream) override {
    WasmDataPtr buffer = getBufferBytes(WasmBufferType::HttpRequestBody, 0, body_buffer_length);
    request_buffer_->append(buffer->data(), buffer->size());
    maybeInspect(request_buffer_.get(), end_of_stream);
    return FilterDataStatus::Continue;
  }

  // Captures response body and passes it for inspection at DlpRootContext level
  FilterDataStatus onResponseBody(size_t body_buffer_length, bool end_of_stream) override {
    WasmDataPtr buffer = getBufferBytes(WasmBufferType::HttpResponseBody, 0, body_buffer_length);
    response_buffer_->append(buffer->data(), buffer->size());
    maybeInspect(response_buffer_.get(), end_of_stream);
    return FilterDataStatus::Continue;
  }

  void maybeInspect(Buffer* buffer, bool end_of_stream) {
    if (end_of_stream) {
      if (buffer->isEmpty()) {
        // Nothing to inspect
      } else if (buffer->isExceeded()) {
        reportExceeded(buffer->appendedSize());
      } else {
        rootContext()->inspect(buffer);
      }
    }
  }

  void reportExceeded(size_t buffer_size) {
    request_too_large_->record(1);
    not_inspected_->record(1);
    total_bytes_not_inspected_->record(buffer_size);
  }

 private:
  std::unique_ptr<Buffer> request_buffer_;
  std::unique_ptr<Buffer> response_buffer_;
  inline DlpRootContext* rootContext() {
    return dynamic_cast<DlpRootContext*>(this->root());
  };
};

static RegisterContextFactory register_DlpContext(
    CONTEXT_FACTORY(DlpContext),
    ROOT_FACTORY(DlpRootContext));
