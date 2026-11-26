#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rng.h"
#include "timestamp.h"

using namespace std::chrono_literals;
using namespace fxcore;

namespace {

std::vector<double> probabilities(RNG& rng, const size_t n) {
  std::vector<double> result;
  for (size_t i = 0; i < n; ++i) result.push_back(rng.random_probability());
  return result;
}

std::vector<Duration> gaussians(RNG& rng, const size_t n, const Duration mean,
                                const Duration stddev) {
  std::vector<Duration> result;
  for (size_t i = 0; i < n; ++i) result.push_back(rng.gaussian(mean, stddev));
  return result;
}

}  // namespace

TEST(RNG, KeepsItsSeed) {
  EXPECT_EQ(RNG{}.seed(), 0u);
  EXPECT_EQ(RNG{20260907}.seed(), 20260907u);
}

TEST(RNG, SameSeedGivesSameSequence) {
  RNG a{42};
  RNG b{42};
  EXPECT_EQ(probabilities(a, 100), probabilities(b, 100));
  EXPECT_EQ(gaussians(a, 100, 1s, 10ms), gaussians(b, 100, 1s, 10ms));
}

TEST(RNG, DifferentSeedsGiveDifferentSequences) {
  RNG a{1};
  RNG b{2};
  EXPECT_NE(probabilities(a, 100), probabilities(b, 100));
}

TEST(RNG, MoveContinuesTheSameSequence) {
  RNG original{7};
  RNG reference{7};
  original.random_probability();
  reference.random_probability();

  RNG moved{std::move(original)};
  EXPECT_EQ(moved.seed(), 7u);
  EXPECT_EQ(probabilities(moved, 10), probabilities(reference, 10));
}

TEST(RNG, RandomProbabilityIsInUnitInterval) {
  RNG rng{3};
  for (const double p : probabilities(rng, 10'000)) {
    EXPECT_GE(p, 0.0);
    EXPECT_LE(p, 1.0);
  }
}

TEST(RNG, RandomProbabilityIsRoughlyUniform) {
  RNG rng{4};
  const auto samples = probabilities(rng, 10'000);
  size_t below_half = 0;
  double sum = 0.0;
  for (const double p : samples) {
    sum += p;
    if (p < 0.5) ++below_half;
  }
  EXPECT_NEAR(sum / static_cast<double>(samples.size()), 0.5, 0.02);
  EXPECT_NEAR(static_cast<double>(below_half), 5'000.0, 300.0);
}

TEST(RNG, GaussianWithZeroStddevIsExactlyTheMean) {
  RNG rng{5};
  EXPECT_EQ(rng.gaussian(200ms, 0ns), 200ms);
  EXPECT_EQ(rng.gaussian(-3s, 0ns), -3s);
  EXPECT_EQ(rng.gaussian(0ns, 0ns), 0ns);
}

TEST(RNG, GaussianWithZeroStddevDoesNotAdvanceTheEngine) {
  RNG a{6};
  RNG b{6};
  a.gaussian(1s, 0ns);
  EXPECT_EQ(probabilities(a, 10), probabilities(b, 10));
}

TEST(RNG, GaussianRejectsNegativeStddev) {
  RNG rng{7};
  EXPECT_THROW(rng.gaussian(1s, -1ns), std::invalid_argument);
}

TEST(RNG, GaussianMatchesMeanAndStddev) {
  RNG rng{8};
  constexpr size_t n = 20'000;
  constexpr Duration mean = 200ms;
  constexpr Duration stddev = 20ms;
  const auto samples = gaussians(rng, n, mean, stddev);

  double sum = 0.0;
  for (const auto& s : samples) sum += static_cast<double>(s.count());
  const double sample_mean = sum / n;

  double sq = 0.0;
  for (const auto& s : samples) {
    const double d = static_cast<double>(s.count()) - sample_mean;
    sq += d * d;
  }
  const double sample_stddev = std::sqrt(sq / (n - 1));

  // Standard error of the mean is 20ms / sqrt(20000) ~= 0.14ms.
  EXPECT_NEAR(sample_mean, static_cast<double>(mean.count()),
              static_cast<double>(Duration{1ms}.count()));
  EXPECT_NEAR(sample_stddev, static_cast<double>(stddev.count()),
              0.05 * static_cast<double>(stddev.count()));
}
