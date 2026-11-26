#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "fault_params.h"
#include "perturb.h"
#include "probability.h"
#include "test_helpers.h"
#include "timestamp.h"

using namespace std::chrono_literals;
using namespace fxcore;
using namespace fxtest;

namespace {

using StrPerturb = Perturb<std::string>;

Timestamp sec(const uint64_t s) { return Timestamp{std::chrono::seconds(s)}; }

// Feeds messages 1..n (message i at i seconds) and returns every emission, in
// the order they came out.
template <class Params>
std::vector<Delivery> feed(StrPerturb& perturb, const Params& params,
                           const uint64_t n) {
  std::vector<Delivery> result;
  for (uint64_t seq = 1; seq <= n; ++seq) {
    const auto out = views(perturb.apply(params, make_msg(sec(seq), seq)));
    result.insert(result.end(), out.begin(), out.end());
  }
  return result;
}

std::vector<uint64_t> sequences(const std::vector<Delivery>& deliveries) {
  std::vector<uint64_t> result;
  for (const auto& d : deliveries) result.push_back(d.sequence);
  return result;
}

}  // namespace

// ---------------------------------------------------------------------------
// Delay
// ---------------------------------------------------------------------------

TEST(PerturbDelay, ZeroJitterDelaysByExactlyTheMean) {
  StrPerturb perturb{1};
  const DelayParams delay{
      .mean = 200ms, .jitter = 0ns, .distribution = std::nullopt};
  EXPECT_EQ(views(perturb.apply(delay, make_msg(sec(1), 1))),
            std::vector{delivery(sec(1) + 200ms, 1)});
}

TEST(PerturbDelay, ZeroDelayLeavesMessageUnchanged) {
  StrPerturb perturb{1};
  const DelayParams delay{
      .mean = 0ns, .jitter = 0ns, .distribution = std::nullopt};
  EXPECT_EQ(views(perturb.apply(delay, make_msg(sec(1), 1))),
            std::vector{delivery(sec(1), 1)});
}

TEST(PerturbDelay, JitterVariesTheDelay) {
  StrPerturb perturb{2};
  const DelayParams delay{
      .mean = 200ms, .jitter = 20ms, .distribution = std::nullopt};
  const auto out = feed(perturb, delay, 100);
  ASSERT_EQ(out.size(), 100u);

  std::vector<Duration> delays;
  for (const auto& d : out) {
    EXPECT_EQ(d.payload, payload_of(d.sequence));
    delays.push_back(d.time - sec(d.sequence));
  }
  const auto [min, max] = std::ranges::minmax(delays);
  EXPECT_LT(min, 200ms);
  EXPECT_GT(max, 200ms);
  // 100 draws from N(200ms, 20ms) staying within 6 sigma.
  EXPECT_GT(min, 80ms);
  EXPECT_LT(max, 320ms);
}

TEST(PerturbDelay, JitterIsReproducibleForASeed) {
  StrPerturb a{3};
  StrPerturb b{3};
  const DelayParams delay{
      .mean = 200ms, .jitter = 20ms, .distribution = std::nullopt};
  EXPECT_EQ(feed(a, delay, 50), feed(b, delay, 50));
}

TEST(PerturbDelay, NegativeJitterThrows) {
  StrPerturb perturb{4};
  const DelayParams delay{
      .mean = 200ms, .jitter = -1ms, .distribution = std::nullopt};
  EXPECT_THROW(perturb.apply(delay, make_msg(sec(1), 1)),
               std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Drop
// ---------------------------------------------------------------------------

TEST(PerturbDrop, EveryNthDropsEachNthMessage) {
  StrPerturb perturb{1};
  const DropParams drop{.probability = std::nullopt, .every_nth = 3};
  EXPECT_EQ(sequences(feed(perturb, drop, 9)),
            (std::vector<uint64_t>{1, 2, 4, 5, 7, 8}));
}

TEST(PerturbDrop, EveryFirstDropsEverything) {
  StrPerturb perturb{1};
  const DropParams drop{.probability = std::nullopt, .every_nth = 1};
  EXPECT_TRUE(feed(perturb, drop, 10).empty());
}

TEST(PerturbDrop, KeptMessageIsUnchanged) {
  StrPerturb perturb{1};
  const DropParams drop{.probability = std::nullopt, .every_nth = 2};
  EXPECT_EQ(views(perturb.apply(drop, make_msg(sec(1) + 5ns, 1))),
            std::vector{delivery(sec(1) + 5ns, 1)});
}

TEST(PerturbDrop, ProbabilityOneDropsEverything) {
  StrPerturb perturb{1};
  const DropParams drop{.probability = Probability{1.0},
                        .every_nth = std::nullopt};
  EXPECT_TRUE(feed(perturb, drop, 100).empty());
}

TEST(PerturbDrop, ProbabilityZeroKeepsEverything) {
  StrPerturb perturb{1};
  const DropParams drop{.probability = Probability{0.0},
                        .every_nth = std::nullopt};
  const auto out = feed(perturb, drop, 100);
  ASSERT_EQ(out.size(), 100u);
  for (const auto& d : out) EXPECT_EQ(d, delivery(sec(d.sequence), d.sequence));
}

TEST(PerturbDrop, ProbabilityDropsRoughlyThatFraction) {
  StrPerturb perturb{5};
  const DropParams drop{.probability = Probability{0.3},
                        .every_nth = std::nullopt};
  const auto kept = feed(perturb, drop, 2000).size();
  EXPECT_NEAR(static_cast<double>(kept), 1400.0, 100.0);
}

TEST(PerturbDrop, ProbabilityIsReproducibleForASeed) {
  StrPerturb a{6};
  StrPerturb b{6};
  const DropParams drop{.probability = Probability{0.5},
                        .every_nth = std::nullopt};
  EXPECT_EQ(feed(a, drop, 200), feed(b, drop, 200));
}

// ---------------------------------------------------------------------------
// Duplicate
// ---------------------------------------------------------------------------

TEST(PerturbDuplicate, ProbabilityOneAddsACopyAfterSpacing) {
  StrPerturb perturb{1};
  const DuplicateParams dup{.probability = Probability{1.0}, .spacing = 5ms};
  EXPECT_EQ(views(perturb.apply(dup, make_msg(sec(1), 1))),
            (std::vector{delivery(sec(1), 1), delivery(sec(1) + 5ms, 1)}));
}

TEST(PerturbDuplicate, ZeroSpacingAddsACopyAtTheSameTime) {
  StrPerturb perturb{1};
  const DuplicateParams dup{.probability = Probability{1.0}, .spacing = 0ns};
  EXPECT_EQ(views(perturb.apply(dup, make_msg(sec(1), 1))),
            (std::vector{delivery(sec(1), 1), delivery(sec(1), 1)}));
}

TEST(PerturbDuplicate, ProbabilityZeroEmitsOnlyTheOriginal) {
  StrPerturb perturb{1};
  const DuplicateParams dup{.probability = Probability{0.0}, .spacing = 5ms};
  EXPECT_EQ(feed(perturb, dup, 100).size(), 100u);
}

TEST(PerturbDuplicate, ProbabilityDuplicatesRoughlyThatFraction) {
  StrPerturb perturb{7};
  const DuplicateParams dup{.probability = Probability{0.5}, .spacing = 1ms};
  const auto emitted = feed(perturb, dup, 2000).size();
  EXPECT_NEAR(static_cast<double>(emitted), 3000.0, 150.0);
}

// ---------------------------------------------------------------------------
// Reorder
// ---------------------------------------------------------------------------

TEST(PerturbReorder, HeldMessageComesOutAtTheEndOfTheWindow) {
  StrPerturb perturb{1};
  const ReorderParams reorder{.window_size = 3,
                              .probability = Probability{1.0}};

  EXPECT_TRUE(perturb.apply(reorder, make_msg(sec(1), 1)).empty());
  EXPECT_EQ(views(perturb.apply(reorder, make_msg(sec(2), 2))),
            std::vector{delivery(sec(2), 2)});
  // The held message keeps its own sequence and payload, but takes the time
  // of the message that releases it.
  EXPECT_EQ(views(perturb.apply(reorder, make_msg(sec(3), 3))),
            (std::vector{delivery(sec(3), 3), delivery(sec(3), 1)}));
}

TEST(PerturbReorder, WindowOfTwoSwapsNeighbours) {
  StrPerturb perturb{1};
  const ReorderParams reorder{.window_size = 2,
                              .probability = Probability{1.0}};

  EXPECT_TRUE(perturb.apply(reorder, make_msg(sec(1), 1)).empty());
  EXPECT_EQ(views(perturb.apply(reorder, make_msg(sec(2), 2))),
            (std::vector{delivery(sec(2), 2), delivery(sec(2), 1)}));
}

TEST(PerturbReorder, StreamDeliversEveryMessageExactlyOnce) {
  StrPerturb perturb{1};
  const ReorderParams reorder{.window_size = 3,
                              .probability = Probability{1.0}};

  EXPECT_EQ(
      feed(perturb, reorder, 9),
      (std::vector{
          delivery(sec(2), 2), delivery(sec(3), 3), delivery(sec(3), 1),
          delivery(sec(5), 5), delivery(sec(6), 6), delivery(sec(6), 4),
          delivery(sec(8), 8), delivery(sec(9), 9), delivery(sec(9), 7)}));
}

TEST(PerturbReorder, ProbabilityZeroPassesThrough) {
  StrPerturb perturb{1};
  const ReorderParams reorder{.window_size = 3,
                              .probability = Probability{0.0}};
  EXPECT_EQ(sequences(feed(perturb, reorder, 5)),
            (std::vector<uint64_t>{1, 2, 3, 4, 5}));
}

// ---------------------------------------------------------------------------
// Clock skew
// ---------------------------------------------------------------------------

// Skew shifts header.stamp inside the payload, which the core cannot see; it
// must not change when, or in which order, a message is delivered.
TEST(PerturbClockSkew, DoesNotChangeDelivery) {
  StrPerturb perturb{1};
  for (const Duration offset : {50ms, -50ms}) {
    const ClockSkewParams skew{.offset = offset};
    EXPECT_EQ(views(perturb.apply(skew, make_msg(sec(1), 1))),
              std::vector{delivery(sec(1), 1)});
  }
}
