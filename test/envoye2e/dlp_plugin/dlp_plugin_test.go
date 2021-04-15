// Copyright 2019 Istio Authors
// Copyright 2021 Google LLC
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

package dlp_plugin

import (
	"os"
	"path/filepath"
	"testing"
	"time"
	"strings"
	"fmt"
	"strconv"

	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
	"istio.io/proxy/testdata"

	dlp_driver "github.com/GoogleCloudPlatform/dlp-filter-for-envoy/third_party/istio_proxy/driver"

  grpc "google.golang.org/grpc"
	test "github.com/GoogleCloudPlatform/dlp-filter-for-envoy/test/envoye2e"
  fake_dlp "github.com/GoogleCloudPlatform/dlp-filter-for-envoy/test/envoye2e/dlp_plugin/fake_dlp"
)

type DlpMock struct {
  Port       uint16

  FakeDlp    *fake_dlp.FakeDlpServiceServer
  GrpcServer *grpc.Server
}

func (d *DlpMock) Run(params *driver.Params) error {
  // Start a fake DLP server
  fakeDlp, grpcServer := fake_dlp.NewFakeDlp(d.Port, 0)
  d.FakeDlp = fakeDlp
  d.GrpcServer = grpcServer
  return nil
}

func (d *DlpMock) Cleanup() {
  d.GrpcServer.Stop()
}

var _ driver.Step = &DlpMock{}

type DlpCheck struct {
  dlpMock      *DlpMock
  InspectCount int
  RequestBody  string
  FullProject  string
  TemplateName string
}

func (d *DlpCheck) Run(params *driver.Params) error {
  inspectContentReceived := 0
  to := time.NewTimer(1 * time.Second)

  // Collect requests received on mock server side
  for inspectContentReceived < d.InspectCount {
    select {
    case req := <-d.dlpMock.FakeDlp.InspectContentReq:
      exp := d.RequestBody
      if string(req.Item.GetByteItem().GetData()) != exp {
        return fmt.Errorf("Received request data %v should be equal to %v", req.Item, exp)
      }
      exp_parent := d.FullProject
      if req.Parent != exp_parent {
        return fmt.Errorf("Received request parent %v should be equal to %v", req.Parent, exp_parent)
      }
      exp_template_name := d.TemplateName
      if req.InspectTemplateName != exp_template_name {
        return fmt.Errorf("Received request template %v should be equal to %v", req.InspectTemplateName,
            exp_template_name)
      }
      inspectContentReceived++;
      return nil;
    case <-to.C:
      to.Stop()
      rcv := fmt.Sprintf(
        "inspectContentReceived: %d",
        inspectContentReceived,
      )
      return fmt.Errorf("timeout: DLP did not receive required requests: " + rcv)
    }
  }
  return nil
}

func (d *DlpCheck) Cleanup() {
}

var _ driver.Step = &DlpCheck{}

var TestCases = []struct {
	Name               string
	Method             string
	RequestBody        string
	Project            string
  TemplateName       string
  Location           string
  FullProject        string
	ResponseCode       int
	RequestCount       int
	InspectCount int
}{
	{
		Name:               "Success",
		Method:             "POST",
		RequestBody:        strings.Repeat("Hi, this is my SSN: 987-65-4321.", 1000),
		Project:            "test-project",
    TemplateName:       "test-template",
    Location:           "test-location",
    FullProject:        "projects/test-project/locations/test-location",
		ResponseCode:       200,
		RequestCount:       10,
		InspectCount:       20,
	},
}

func TestDlpFilter(t *testing.T) {
	for _, testCase := range TestCases {
		t.Run(testCase.Name, func(t *testing.T) {
			params := driver.NewTestParams(t, map[string]string{
				"DlpWasmFile":  filepath.Join(env.GetBazelBinOrDie(), "plugin/filter.wasm"),
				"Project":      testCase.Project,
				"TemplateName": testCase.TemplateName,
				"Location":     testCase.Location,
			}, test.ExtensionE2ETests)
			dlpPort := params.Ports.Max + 1
			params.Vars["DlpGrpcUrl"] = "localhost:" + strconv.Itoa(int(dlpPort))
			params.Vars["ServerHTTPFilters"] = params.LoadTestData("test/envoye2e/dlp_plugin/testdata/server_filter.yaml.tmpl")
			params.Vars["InspectCount"] = strconv.Itoa(testCase.InspectCount)
			dlpMock := &DlpMock{
			  Port: dlpPort,
			}
			dlpCheck := &DlpCheck{
			  dlpMock: dlpMock,
        InspectCount: testCase.InspectCount,
        RequestBody:  testCase.RequestBody,
        FullProject:  testCase.FullProject,
        TemplateName: testCase.TemplateName,
			}

			if err := (&driver.Scenario{
				Steps: []driver.Step{
				  // Spin up Envoy and fake DLP
					&driver.XDS{},
					&driver.Update{
						Node: "server", Version: "0", Listeners: []string{string(testdata.MustAsset("listener/server.yaml.tmpl"))},
					},
					&driver.Envoy{
						Bootstrap:       params.FillTestData(string(testdata.MustAsset("bootstrap/server.yaml.tmpl"))),
						DownloadVersion: os.Getenv("ISTIO_TEST_VERSION"),
					},
					dlpMock,
					&driver.Sleep{Duration: 1 * time.Second},
					// Run actual test
					&driver.Repeat{
						N: testCase.RequestCount,
						Step: &dlp_driver.HTTPCall{
              Port:            params.Ports.ServerPort,
              Method:          testCase.Method,
              RequestBody:     testCase.RequestBody,
              ResponseCode:    testCase.ResponseCode,
						},
					},
					// Verify envoy logged the requests and DLP received the body
					&driver.Stats{
						AdminPort: params.Ports.ServerAdmin,
						Matchers: map[string]driver.StatMatcher{
							"envoy_dlp_stat_inspected": &driver.
								ExactStat{Metric: "test/envoye2e/dlp_plugin/testdata/stats.yaml.tmpl"},
						},
					},
          dlpCheck,
				},
			}).Run(params); err != nil {
				t.Fatal(err)
			}
		})
	}
}