// Copyright 2019 Istio Authors
// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package client_test

import (
  "fmt"
  "path/filepath"
  "testing"
  "time"

  "os/exec"
  "strings"

  "istio.io/proxy/test/envoye2e/env"
  fake_dlp "istio.io/proxy/test/envoye2e/dlp_plugin/fake_dlp"
  dlppb "google.golang.org/genproto/googleapis/privacy/dlp/v2"
)

const outboundFilterConfig = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          local:
            inline_string: "envoy.wasm.metadata_exchange"
- name: envoy.filters.http.wasm
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: type.googleapis.com/envoy.extensions.filters.http.wasm.v3.Wasm
    value:
      config:
        name: "dlp_plugin"
        root_id: ""
        vm_config:
          vm_id: "dlp_vm_id"
          runtime: "envoy.wasm.runtime.v8"
          code:
            local:
              filename: "%s"
        configuration:
          "@type": "type.googleapis.com/google.protobuf.StringValue"
          value: |
            {
              "inspect": {
                "destination": {
                  "operation": {
                    "store_local": {
                      "project_id": "test-project",
                      "location_id": "test-location",
                      "inspect_template_name": "test-template"
                    }
                  },
                  "grpc_config": {
                    "target_uri": "localhost:12312"
                  }
                }
              }
            }`

const inboundFilterConfig = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          local:
            inline_string: "envoy.wasm.metadata_exchange"
- name: envoy.filters.http.wasm
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: type.googleapis.com/envoy.extensions.filters.http.wasm.v3.Wasm
    value:
      config:
        name: "dlp_plugin"
        root_id: ""
        vm_config:
          vm_id: "dlp_vm_id"
          runtime: "envoy.wasm.runtime.v8"
          code:
            local:
              filename: "%s"
        configuration:
          "@type": "type.googleapis.com/google.protobuf.StringValue"
          value: |
            {
              "inspect": {
                "destination": {
                  "operation": {
                    "store_local": {
                      "project_id": "test-project",
                      "location_id": "test-location",
                      "inspect_template_name": "test-template"
                    }
                  },
                  "grpc_config": {
                    "target_uri": "localhost:12312"
                  }
                }
              }
            }`

const outboundNodeMetadata = `"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "productpage",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-productpage",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://apis/apps/v1/namespaces/default/deployments/productpage-v1",
"WORKLOAD_NAME": "productpage-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container productpage",
"POD_NAME": "productpage-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_cluster_location": "us-east4-b"
},
"LABELS": {
 "app": "productpage",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "productpage-v1-84975bc778-pxz2w",`

const inboundNodeMetadata = `"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "ratings",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-ratings",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://apis/apps/v1/namespaces/default/deployments/ratings-v1",
"WORKLOAD_NAME": "ratings-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container ratings",
"POD_NAME": "ratings-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_cluster_location": "us-east4-b"
},
"LABELS": {
 "app": "ratings",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "ratings-v1-84975bc778-pxz2w",`

const statsConfig = `stats_config:
  use_all_default_tags: true`

// Stats in Server Envoy proxy.
var expectedPrometheusStats = map[string]env.Stat{
  "envoy_dlp_stat_inspected": {Value: 20},
}

func verifyInspectContent(got *dlppb.InspectContentRequest) error {
  exp := strings.Repeat("Hi, this is my SSN: 987-65-4321.", 1000)
  if string(got.Item.GetByteItem().GetData()) != exp {
    return fmt.Errorf("Received request data %v should be equal to %v", got.Item, exp)
  }
  exp_parent := "projects/test-project/locations/test-location"
  if got.Parent != exp_parent {
    return fmt.Errorf("Received request parent %v should be equal to %v", got.Parent, exp_parent)
  }
  exp_template_name := "test-template"
  if got.InspectTemplateName != exp_template_name {
    return fmt.Errorf("Received request template %v should be equal to %v", got.InspectTemplateName,
        exp_template_name)
  }
  return nil;
}

func GetWorkspace() string {
  workspace, _ := exec.Command("bazel", "info", "workspace").Output()
  return strings.TrimSuffix(string(workspace), "\n")
}

func TestDlpPlugin(t *testing.T) {
  s := env.NewClientServerEnvoyTestSetup(env.BasicFlowTest, t)
  s.SetFiltersBeforeEnvoyRouterInClientToProxy(fmt.Sprintf(outboundFilterConfig, filepath.Join(GetWorkspace(), "plugin/bazel-bin/filter.wasm")))
  s.SetFiltersBeforeEnvoyRouterInClientToApp(fmt.Sprintf(inboundFilterConfig, filepath.Join(GetWorkspace(), "plugin/bazel-bin/filter.wasm")))
  s.SetServerNodeMetadata(inboundNodeMetadata)
  s.SetClientNodeMetadata(outboundNodeMetadata)
  s.SetExtraConfig(statsConfig)
  if err := s.SetUpClientServerEnvoy(); err != nil {
    t.Fatalf("Failed to setup test: %v", err)
  }
  defer s.TearDownClientServerEnvoy()

  port := uint16(12312)
  fakeDlp, grpcServer := fake_dlp.NewFakeDlp(port, 0)
  defer grpcServer.Stop()

  url := fmt.Sprintf("http://127.0.0.1:%d/echo", s.Ports().AppToClientProxyPort)

  // Enable trace
  enableGrpcLogsURL := fmt.Sprintf("http://localhost:%d/logging?grpc=trace", s.Ports().ClientAdminPort)
  env.HTTPPost(enableGrpcLogsURL, "", "")
  enableWasmLogsURL := fmt.Sprintf("http://localhost:%d/logging?wasm=debug", s.Ports().ClientAdminPort)
  env.HTTPPost(enableWasmLogsURL, "", "")

  // Issues a GET echo request with SSN in body
  tag := "OKGet"
  for i := 0; i < 10; i++ {
    if _, _, err := env.HTTPPost(url, "text/plain", strings.Repeat("Hi, this is my SSN: 987-65-4321.", 1000)); err != nil {
      t.Errorf("Failed in request %s: %v", tag, err)
    }
  }

  inspectContentReceived := 0
  to := time.NewTimer(1 * time.Second)

  // Collect requests received on mock server side
  for inspectContentReceived < 10 {
    select {
    case req := <-fakeDlp.InspectContentReq:
      if err := verifyInspectContent(req); err != nil {
        t.Errorf("InspectContentRequest verification failed: %v", err)
      }
      inspectContentReceived++;
    case <-to.C:
      to.Stop()
      rcv := fmt.Sprintf(
        "inspectContentReceived: %d",
        inspectContentReceived,
      )
      t.Fatal("timeout: DLP did not receive required requests: " + rcv)
    }
  }

  // Verify that correct stats have been recorded
  s.VerifyPrometheusStats(expectedPrometheusStats, s.Ports().ServerAdminPort)

}
