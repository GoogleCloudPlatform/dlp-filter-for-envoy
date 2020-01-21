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

package fake_dlp

import (
  "context"
  "fmt"
  "log"
  "net"
  "time"

  grpc "google.golang.org/grpc"
  empty "github.com/golang/protobuf/ptypes/empty"
  dlppb "google.golang.org/genproto/googleapis/privacy/dlp/v2"
)

// FakeStackdriverMetricServer is a fake stackdriver server which implements all of monitoring v3 service method.
type FakeDlpServiceServer struct {
  delay               time.Duration
  InspectContentReq   chan *dlppb.InspectContentRequest
  HybridInspectReq    chan *dlppb.HybridInspectJobTriggerRequest
}

// Implementations of all DLP API methods
func (*FakeDlpServiceServer) ActivateJobTrigger(context.Context, *dlppb.ActivateJobTriggerRequest) (*dlppb.DlpJob, error) {
  return &dlppb.DlpJob{}, nil
}
func (*FakeDlpServiceServer) CancelDlpJob(context.Context, *dlppb.CancelDlpJobRequest) (*empty.Empty, error) {
  return nil, nil
}
func (*FakeDlpServiceServer) CreateDeidentifyTemplate(context.Context, *dlppb.CreateDeidentifyTemplateRequest) (*dlppb.DeidentifyTemplate, error) {
  return &dlppb.DeidentifyTemplate{}, nil
}
func (*FakeDlpServiceServer) CreateDlpJob(context.Context, *dlppb.CreateDlpJobRequest) (*dlppb.DlpJob, error) {
  return &dlppb.DlpJob{}, nil
}
func (*FakeDlpServiceServer) CreateInspectTemplate(context.Context, *dlppb.CreateInspectTemplateRequest) (*dlppb.InspectTemplate, error) {
  return &dlppb.InspectTemplate{}, nil
}
func (*FakeDlpServiceServer) CreateJobTrigger(context.Context, *dlppb.CreateJobTriggerRequest) (*dlppb.JobTrigger, error) {
  return &dlppb.JobTrigger{}, nil
}
func (*FakeDlpServiceServer) CreateStoredInfoType(context.Context, *dlppb.CreateStoredInfoTypeRequest) (*dlppb.StoredInfoType, error) {
  return &dlppb.StoredInfoType{}, nil
}
func (*FakeDlpServiceServer) DeidentifyContent(context.Context, *dlppb.DeidentifyContentRequest) (*dlppb.DeidentifyContentResponse, error) {
  return &dlppb.DeidentifyContentResponse{}, nil
}
func (*FakeDlpServiceServer) DeleteDeidentifyTemplate(context.Context, *dlppb.DeleteDeidentifyTemplateRequest) (*empty.Empty, error) {
  return nil, nil
}
func (*FakeDlpServiceServer) DeleteDlpJob(context.Context, *dlppb.DeleteDlpJobRequest) (*empty.Empty, error) {
  return nil, nil
}
func (*FakeDlpServiceServer) DeleteInspectTemplate(context.Context, *dlppb.DeleteInspectTemplateRequest) (*empty.Empty, error) {
  return nil, nil
}
func (*FakeDlpServiceServer) DeleteJobTrigger(context.Context, *dlppb.DeleteJobTriggerRequest) (*empty.Empty, error) {
  return nil, nil
}
func (*FakeDlpServiceServer) DeleteStoredInfoType(context.Context, *dlppb.DeleteStoredInfoTypeRequest) (*empty.Empty, error) {
  return nil, nil
}
func (*FakeDlpServiceServer) FinishDlpJob(context.Context, *dlppb.FinishDlpJobRequest) (*empty.Empty, error) {
  return nil, nil
}
func (*FakeDlpServiceServer) GetDeidentifyTemplate(context.Context, *dlppb.GetDeidentifyTemplateRequest) (*dlppb.DeidentifyTemplate, error) {
  return &dlppb.DeidentifyTemplate{}, nil
}
func (*FakeDlpServiceServer) GetDlpJob(context.Context, *dlppb.GetDlpJobRequest) (*dlppb.DlpJob, error) {
  return &dlppb.DlpJob{}, nil
}
func (*FakeDlpServiceServer) GetInspectTemplate(context.Context, *dlppb.GetInspectTemplateRequest) (*dlppb.InspectTemplate, error) {
  return &dlppb.InspectTemplate{}, nil
}
func (*FakeDlpServiceServer) GetJobTrigger(context.Context, *dlppb.GetJobTriggerRequest) (*dlppb.JobTrigger, error) {
  return &dlppb.JobTrigger{}, nil
}
func (*FakeDlpServiceServer) GetStoredInfoType(context.Context, *dlppb.GetStoredInfoTypeRequest) (*dlppb.StoredInfoType, error) {
  return &dlppb.StoredInfoType{}, nil
}
func (*FakeDlpServiceServer) HybridInspectDlpJob(context.Context, *dlppb.HybridInspectDlpJobRequest) (*dlppb.HybridInspectResponse, error) {
  return &dlppb.HybridInspectResponse{}, nil
}
func (server *FakeDlpServiceServer) HybridInspectJobTrigger(_ context.Context, req *dlppb.HybridInspectJobTriggerRequest) (*dlppb.HybridInspectResponse, error) {
  server.HybridInspectReq <- req
  time.Sleep(server.delay)
  return &dlppb.HybridInspectResponse{}, nil
}
func (server *FakeDlpServiceServer) InspectContent(_ context.Context, req *dlppb.InspectContentRequest) (*dlppb.InspectContentResponse, error) {
  server.InspectContentReq <- req
  time.Sleep(server.delay)
  return &dlppb.InspectContentResponse{}, nil
}
func (*FakeDlpServiceServer) ListDeidentifyTemplates(context.Context, *dlppb.ListDeidentifyTemplatesRequest) (*dlppb.ListDeidentifyTemplatesResponse, error) {
  return &dlppb.ListDeidentifyTemplatesResponse{}, nil
}
func (*FakeDlpServiceServer) ListDlpJobs(context.Context, *dlppb.ListDlpJobsRequest) (*dlppb.ListDlpJobsResponse, error) {
  return &dlppb.ListDlpJobsResponse{}, nil
}
func (*FakeDlpServiceServer) ListInfoTypes(context.Context, *dlppb.ListInfoTypesRequest) (*dlppb.ListInfoTypesResponse, error) {
  return &dlppb.ListInfoTypesResponse{}, nil
}
func (*FakeDlpServiceServer) ListInspectTemplates(context.Context, *dlppb.ListInspectTemplatesRequest) (*dlppb.ListInspectTemplatesResponse, error) {
  return &dlppb.ListInspectTemplatesResponse{}, nil
}
func (*FakeDlpServiceServer) ListJobTriggers(context.Context, *dlppb.ListJobTriggersRequest) (*dlppb.ListJobTriggersResponse, error) {
  return &dlppb.ListJobTriggersResponse{}, nil
}
func (*FakeDlpServiceServer) ListStoredInfoTypes(context.Context, *dlppb.ListStoredInfoTypesRequest) (*dlppb.ListStoredInfoTypesResponse, error) {
  return &dlppb.ListStoredInfoTypesResponse{}, nil
}
func (*FakeDlpServiceServer) RedactImage(context.Context, *dlppb.RedactImageRequest) (*dlppb.RedactImageResponse, error) {
  return &dlppb.RedactImageResponse{}, nil
}
func (*FakeDlpServiceServer) ReidentifyContent(context.Context, *dlppb.ReidentifyContentRequest) (*dlppb.ReidentifyContentResponse, error) {
  return &dlppb.ReidentifyContentResponse{}, nil
}
func (*FakeDlpServiceServer) UpdateDeidentifyTemplate(context.Context, *dlppb.UpdateDeidentifyTemplateRequest) (*dlppb.DeidentifyTemplate, error) {
  return &dlppb.DeidentifyTemplate{}, nil
}
func (*FakeDlpServiceServer) UpdateInspectTemplate(context.Context, *dlppb.UpdateInspectTemplateRequest) (*dlppb.InspectTemplate, error) {
  return &dlppb.InspectTemplate{}, nil
}
func (*FakeDlpServiceServer) UpdateJobTrigger(context.Context, *dlppb.UpdateJobTriggerRequest) (*dlppb.JobTrigger, error) {
  return &dlppb.JobTrigger{}, nil
}
func (*FakeDlpServiceServer) UpdateStoredInfoType(context.Context, *dlppb.UpdateStoredInfoTypeRequest) (*dlppb.StoredInfoType, error) {
  return &dlppb.StoredInfoType{}, nil
}

// NewFakeDlp creates a new fake Dlp server.
func NewFakeDlp(port uint16, delay time.Duration) (*FakeDlpServiceServer, *grpc.Server) {
  log.Printf("Dlp server listening on port %v\n", port)
  grpcServer := grpc.NewServer()
  fakeDlp := &FakeDlpServiceServer{
    delay:               delay,
    InspectContentReq:   make(chan *dlppb.InspectContentRequest, 100),
    HybridInspectReq:    make(chan *dlppb.HybridInspectJobTriggerRequest, 100),
  }
  dlppb.RegisterDlpServiceServer(grpcServer, fakeDlp)

  go func() {
    lis, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
    if err != nil {
      log.Fatalf("failed to listen: %v", err)
    }
    err = grpcServer.Serve(lis)
    if err != nil {
      log.Fatalf("fake DLP server terminated abnormally: %v", err)
    }
  }()
  return fakeDlp, grpcServer
}