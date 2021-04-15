# Cloud DLP Filter for Envoy

## Overview

Cloud DLP Filter for Envoy is a WebAssembly ("Wasm") HTTP filter for Envoy sidecar proxies inside an
Istio service mesh. DLP Filter for Envoy captures proxy data plane traffic and sends it for
inspection to Cloud DLP, where the payload is scanned for sensitive data, including PII.

DLP Filter for Envoy works with Istio, and uses the [envoy-wasm](https://github.com/envoyproxy/envoy-wasm)
fork of the [envoyproxy](https://github.com/envoyproxy) repository. 
Because the Wasm Application Binary Interface (ABI) is not yet stabilized, DLP Filter is likely to
break between Istio minor version updates.

> **Note:** DLP Filter for Envoy uses the Cloud DLP service for scanning traffic, and is subject to 
  [Cloud DLP pricing](https://cloud.google.com/dlp/pricing).

For a quick overview of how to use Cloud DLP in a service mesh using the DLP Filter for Envoy,
watch the following video:

[![Using Cloud DLP in a service mesh](https://img.youtube.com/vi/Vh_KufKwpMA/mqdefault.jpg)](https://www.youtube.com/watch?v=Vh_KufKwpMA)

## Before you begin

Before building the DLP Filter for Envoy, ensure the following components are installed:

*   Bazel (For installation instructions, see [Installing
    Bazel](https://docs.bazel.build/versions/master/install.html) on the Bazel website.)

## Build

To build the proxy container with DLP Filter for Envoy, do the following:

*   [Build DLP Filter for Envoy](#build-the-filter)
*   [Build the proxy image](#build-the-proxy-image)

### Build the filter

First, compile the DLP Filter for Envoy in the plugin/ folder:

```
bazel build //plugin:filter.wasm
```

The built DLP Filter will be here:

```
bazel-bin/plugin/filter.wasm
```

Run tests on the filter you just built:

```
bazel test //test/plugin/... && ./run_integ_tests.sh
```

### Build the proxy image

Build the proxy image with the filter by running the following command:

```
bazel run //container:dlp_proxy
```

The proxy image is installed in the local container image repository with the following name and
tag: `bazel/container:dlp_proxy`. 

You can verify installation by running the following command:

```
docker images | grep dlp_proxy
```

This produces output similar to below:

```
bazel/container                              dlp_proxy                      a56a854415c5        50 years ago        306MB
```

## Use DLP Filter for Envoy

For information about how to start using DLP Filter for Envoy quickly, see the following quickstart:

*   [Quickstart: Using DLP Filter for Envoy in Istio](docs/istio_quickstart.md)

## Contributing

See [docs/contributing.md](docs/contributing.md).

## Feedback

To send us feedback about the DLP Filter or Cloud DLP, please contact us through the 
[Cloud DLP support page](https://cloud.google.com/dlp/docs/support/getting-support).


