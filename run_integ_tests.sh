# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

wget https://storage.googleapis.com/istio-build/proxy/envoy-alpha-dc78069b10cc94fa07bb974b7101dd1b42e2e7bf.tar.gz -O bazel-bin/envoy.tar.gz && \
tar xzf bazel-bin/envoy.tar.gz --directory bazel-bin && \
mkdir -p bazel-bin/src/envoy && \
mv bazel-bin/usr/local/bin/envoy bazel-bin/src/envoy/envoy && \
chmod a+x bazel-bin/src/envoy/envoy && \
go test test/envoye2e/dlp_plugin/dlp_plugin_test.go