# Copyright 2021 Google LLC
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
load("@proxy_wasm_cpp_sdk//bazel/wasm:wasm.bzl", "wasm_cc_binary")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "config_proto",
    srcs = [
        "config.proto",
    ],
    deps = [
        "@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics_proto",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [
        ":config_proto",
    ],
)

wasm_cc_binary(
    name = "filter.wasm",
    srcs = [
        "filter.cc",
        "filter.h",
    ],
    copts = [
        "-DPROXY_WASM_PROTOBUF",
    ],
    deps = [
        ":config_cc_proto",
        ":dlp_cc_proto",
        "//plugin/buffer",
        "//plugin/sampling",
        "@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics_full",
    ],
)

cc_library(
    name = "filter",
    srcs = [
        "filter.cc",
        "filter.h",
    ],
    copts = [
        "-DPROXY_WASM_PROTOBUF",
        "-DNULL_PLUGIN",
    ],
    deps = [
        ":config_cc_proto",
        ":dlp_cc_proto",
        "//plugin/buffer",
        "//plugin/sampling",
        "@proxy_wasm_cpp_host//:lib",
    ],
)

# DLP API client libraries
cc_proto_library(
    name = "dlp_cc_proto",
    deps = ["@com_google_googleapis//google/privacy/dlp/v2:dlp_proto"],
)

cc_test(
    name = "filter_test",
    srcs = [
        "filter_test.cc",
    ],
    copts = [
        "-DPROXY_WASM_PROTOBUF",
        "-DNULL_PLUGIN",
    ],
    deps = [
        ":filter",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@proxy_wasm_cpp_host//:lib",
    ],
)
