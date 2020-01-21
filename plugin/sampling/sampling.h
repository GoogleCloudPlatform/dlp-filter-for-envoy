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

#include <memory>
#include <string>
#include <random>

namespace google { namespace dlp_filter {

// Interface for samplers.
class Sampler {
 public:
  virtual bool sample() = 0;
  virtual ~Sampler() = default;
};

// Every item is considered part of the sample, always returns true.
class PassthroughSampler : public Sampler {
 public:
  static std::unique_ptr<PassthroughSampler> create() {
    std::unique_ptr<PassthroughSampler> sampler(new PassthroughSampler());
    return sampler;
  }

  bool sample() override {
    return true;
  }
};

// Only a defined percentage of items will be considered part of the sample.
//
// Each new call to this sampler is independent from the previous calls.
class ProbabilisticSampler : public Sampler {
 public:
  enum CreateStatus {
    Success,
    DenominatorNonPositive,
    NumeratorGreaterThanDenominator
  };

  // Creates new sampler and stores it on the out_sampler pointer.
  //
  // The out_sampler will not be updated if sampler cannot be created for any reason.
  static CreateStatus create(
      unsigned int numerator,
      unsigned int denominator,
      std::unique_ptr<Sampler>& out_sampler) {
    if (denominator <= 0) {
      return DenominatorNonPositive;
    } else if (numerator > denominator) {
      return NumeratorGreaterThanDenominator;
    }
    out_sampler.reset(new ProbabilisticSampler(numerator, denominator));
    return Success;
  }

  bool sample() override {
    return distribution(generator_) <= numerator_;
  }

 private:
  explicit ProbabilisticSampler(
      unsigned int numerator,
      unsigned int denominator) :
      numerator_(numerator),
      denominator_(denominator),
      generator_(),
      distribution(1, denominator_) {
  };
  const unsigned int numerator_;
  const unsigned int denominator_;
  std::default_random_engine generator_;
  std::uniform_int_distribution<int> distribution;
};

}}