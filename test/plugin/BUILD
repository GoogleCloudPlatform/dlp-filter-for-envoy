# Copyright 2020-2021 Google LLC
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

cc_test(
    name = "buffer_test",
    srcs = [
        "buffer_test.cc",
    ],
    deps = [
        "//plugin/buffer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "sampling_test",
    srcs = [
        "sampling_test.cc",
    ],
    deps = [
        "//plugin/sampling",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
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
        "//plugin:filter",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@proxy_wasm_cpp_host//:lib",
    ],
)
