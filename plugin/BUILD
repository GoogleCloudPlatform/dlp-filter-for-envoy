# Copyright 2020 Google LLC
# Copyright 2020 Solo.io, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "config_proto",
    srcs = [
        "config.proto",
    ],
    deps = [
        "@envoy_wasm_api//:proxy_wasm_intrinsics_proto",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [
        ":config_proto",
    ],
)

cc_binary(
    name = "filter.wasm",
    srcs = [
        "filter.cc",
    ],
    additional_linker_inputs = ["@envoy_wasm_api//:jslib"],
    linkopts = [
        "--js-library",
        "external/envoy_wasm_api/proxy_wasm_intrinsics.js",
    ],
    deps = [
        ":config_cc_proto",
        ":dlp_cc_proto",
        "//buffer",
        "//sampling",
        "@envoy_wasm_api//:proxy_wasm_intrinsics",
    ],
)

# DLP API client libraries
cc_proto_library(
    name = "dlp_cc_proto",
    visibility = ["//visibility:public"],
    deps = ["@com_google_googleapis//google/privacy/dlp/v2:dlp_proto"],
)

filegroup(
    name = "module",
    srcs = glob([
        "bazel-out/wasm-fastbuild/bin/filter.wasm",
    ]),
    visibility = ["//visibility:public"],
)
