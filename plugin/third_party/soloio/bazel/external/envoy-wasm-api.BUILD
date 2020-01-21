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

package(default_visibility = ['//visibility:public'])

cc_library(
    name = "proxy_wasm_intrinsics",
    visibility = ["//visibility:public"],
    srcs = [
        "proxy_wasm_intrinsics.cc",
	# Commented out to use non-lite protobuf version
        # This might need to be revisited for performance
        #"proxy_wasm_intrinsics_lite.pb.cc",
        #"struct_lite.pb.cc",
    ],
    hdrs = [
        "proxy_wasm_intrinsics.h",
        "proxy_wasm_enums.h",
        "proxy_wasm_common.h",
        "proxy_wasm_externs.h",
        "proxy_wasm_api.h",
        "proxy_wasm_intrinsics.pb.h",
        #"proxy_wasm_intrinsics_lite.pb.h",
        #"struct_lite.pb.h",
    ],
    deps = [
        "@com_google_protobuf//:protobuf",
    ],
)

proto_library(
    name = "proxy_wasm_intrinsics_proto",
    srcs = [
        "proxy_wasm_intrinsics.proto",
        #"struct_lite.proto",
        #"proxy_wasm_intrinsics_lite.proto",
    ],
    deps = [
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

filegroup(
    visibility = ["//visibility:public"],
    name = "jslib",
    srcs = [
        "proxy_wasm_intrinsics.js",
    ],
)
