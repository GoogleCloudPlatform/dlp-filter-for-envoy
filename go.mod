module istio.io/proxy

go 1.12

replace istio.io/proxy/test/envoye2e/env v0.0.0 => ./third_party/istio_proxy/env

require (
	cloud.google.com/go v0.56.0
	github.com/d4l3k/messagediff v1.2.2-0.20180726183240-b9e99b2f9263
	github.com/envoyproxy/go-control-plane v0.9.4
	github.com/ghodss/yaml v1.0.0
	github.com/golang/protobuf v1.3.5
	github.com/googleapis/gax-go v1.0.3 // indirect
	github.com/prometheus/client_model v0.0.0-20190812154241-14fe0d1b01d4
	github.com/prometheus/common v0.7.0
	google.golang.org/api v0.20.0
	google.golang.org/genproto v0.0.0-20200331122359-1ee6d9798940
	google.golang.org/grpc v1.28.0
	gopkg.in/yaml.v2 v2.2.4 // indirect
	istio.io/proxy/test/envoye2e/env v0.0.0
)
