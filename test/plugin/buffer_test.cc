// Copyright 2020-2021 Google LLC
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

#include "gtest/gtest.h"
#include "plugin/buffer/buffer.h"

using google::dlp_filter::Buffer;

TEST(BufferUnlimitedTest, CanGrow) {
  Buffer buffer = Buffer(0);
  EXPECT_EQ(0, buffer.size());
  EXPECT_TRUE(buffer.isEmpty());
  EXPECT_FALSE(buffer.isExceeded());
  EXPECT_EQ(0, buffer.appendedSize());
  EXPECT_STREQ("", buffer.data());

  buffer.append("Test123", 5);
  EXPECT_EQ(5, buffer.size());
  EXPECT_FALSE(buffer.isExceeded());
  EXPECT_FALSE(buffer.isEmpty());
  EXPECT_EQ(5, buffer.appendedSize());
  EXPECT_STREQ("Test1", buffer.data());

  buffer.append("Test2", 5);
  EXPECT_EQ(10, buffer.size());
  EXPECT_FALSE(buffer.isExceeded());
  EXPECT_FALSE(buffer.isEmpty());
  EXPECT_EQ(10, buffer.appendedSize());
  EXPECT_STREQ("Test1Test2", buffer.data());
}

TEST(BufferLimitedTest, CannotGrow) {
  Buffer buffer = Buffer(8);
  EXPECT_EQ(0, buffer.size());
  EXPECT_TRUE(buffer.isEmpty());
  EXPECT_FALSE(buffer.isExceeded());
  EXPECT_EQ(0, buffer.appendedSize());
  EXPECT_STREQ("", buffer.data());

  buffer.append("Test123", 5);
  EXPECT_EQ(5, buffer.size());
  EXPECT_FALSE(buffer.isExceeded());
  EXPECT_FALSE(buffer.isEmpty());
  EXPECT_EQ(5, buffer.appendedSize());
  EXPECT_STREQ("Test1", buffer.data());

  buffer.append("Test2", 5);
  EXPECT_EQ(5, buffer.size());
  EXPECT_TRUE(buffer.isExceeded());
  EXPECT_FALSE(buffer.isEmpty());
  EXPECT_EQ(10, buffer.appendedSize());
  EXPECT_STREQ("Test1", buffer.data());
}