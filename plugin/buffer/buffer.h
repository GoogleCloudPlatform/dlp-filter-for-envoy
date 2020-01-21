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
#pragma once

#include <string_view>
#include <string>

namespace google { namespace dlp_filter {

// Appendable in-memory data storage to a specified max_size limit.
// If data is appended beyond the limit, buffer will be marked as exceeded and
// no new data will be appended to it.
// It keeps track of how many bytes would be appended in consecutive calls
// regardless of the limit. This is used for reporting.
class Buffer {
 public:
  explicit Buffer(size_t max_size)
      : data_(),
        max_size_(max_size),
        appended_size_(),
        exceeded_() {}

  // Adds data to the buffer unless max_size is exceeded.
  // Size has to be equal to the length of the string passed in data.
  void append(const char* data, size_t size);

  // Whether total data appended to the buffer was beyond max_size.
  bool isExceeded() {
    return exceeded_;
  }

  // Whether nothing has been added to the buffer yet.
  bool isEmpty() {
    return data_.size() == 0;
  }

  // Size of the buffer, limited by max_size
  size_t size() {
    return data_.size();
  }

  // Sum of all data appended to the buffer, regardless of max_size limit.
  size_t appendedSize() {
    return appended_size_;
  }

  // Raw string, should not be freed by the caller.
  const char* data() {
    return data_.c_str();
  }

 private:
  std::string data_;
  size_t max_size_;
  size_t appended_size_;
  bool exceeded_;
};

}}