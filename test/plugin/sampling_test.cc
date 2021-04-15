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

#include <memory>
#include "gtest/gtest.h"
#include "plugin/sampling/sampling.h"

using google::dlp_filter::PassthroughSampler;
using google::dlp_filter::ProbabilisticSampler;
using google::dlp_filter::Sampler;

TEST(PassthroughSampler, ReturnsTrue) {
  std::unique_ptr<Sampler> sampler(PassthroughSampler::create());
  EXPECT_TRUE(sampler->sample());
}

TEST(ProbabilisticSamplerAll, ReturnsTrue) {
  std::unique_ptr<Sampler> sampler;
  EXPECT_EQ(ProbabilisticSampler::create(1, 1, sampler),
      ProbabilisticSampler::CreateStatus::Success);
  EXPECT_TRUE(sampler->sample());
}

TEST(ProbabilisticSamplerNone, ReturnsFalse) {
  std::unique_ptr<Sampler> sampler;
  EXPECT_EQ(ProbabilisticSampler::create(0, 1, sampler),
      ProbabilisticSampler::CreateStatus::Success);
  EXPECT_FALSE(sampler->sample());
}

TEST(ProbabilisticSamplerZeroDenominator, ThrowsException) {
  std::unique_ptr<Sampler> sampler;
  EXPECT_EQ(ProbabilisticSampler::create(0, 0, sampler),
      ProbabilisticSampler::CreateStatus::DenominatorNonPositive);
}

TEST(ProbabilisticSamplerNumeratorTooLarge, ThrowsException) {
  std::unique_ptr<Sampler> sampler;
  EXPECT_EQ(ProbabilisticSampler::create(2, 1, sampler),
      ProbabilisticSampler::CreateStatus::NumeratorGreaterThanDenominator);
}