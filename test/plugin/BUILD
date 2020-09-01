# Copyright 2016-2019 Envoy Project Authors
# Copyright 2020 Google LLC
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

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load(
    "@envoy//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

envoy_package()

envoy_extension_cc_test(
    name = "filter_test",
    size = "large",
    srcs = ["filter_test.cc"],
    data = [
        ":testdata",
        "@dlp_plugin//:module",
    ],
    extension_name = "envoy.filters.http.wasm",
    repository = "@envoy",
    deps = [
        "@dlp_plugin//:dlp_cc_proto",
        "@envoy//source/common/http:message_lib",
        "@envoy//source/extensions/filters/http/wasm:wasm_filter_lib",
        "@envoy//test/test_common:wasm_lib",
    ],
)

cc_test(
    name = "buffer_test",
    srcs = [
        "buffer_test.cc",
    ],
    deps = [
        "@dlp_plugin//buffer",
        "@gtest",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "sampling_test",
    srcs = [
        "sampling_test.cc",
    ],
    deps = [
        "@dlp_plugin//sampling",
        "@gtest",
        "@gtest//:gtest_main",
    ],
)

filegroup(
    name = "testdata",
    srcs = glob([
        "testdata/*",
    ]),
    visibility = ["//visibility:public"],
)
