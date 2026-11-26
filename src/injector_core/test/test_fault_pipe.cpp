#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "events.h"
#include "fault.h"
#include "fault_params.h"
#include "fault_pipe.h"
#include "probability.h"
#include "test_helpers.h"
#include "timestamp.h"
#include "window.h"

using namespace std::chrono_literals;
using namespace fxcore;
using namespace fxtest;

namespace {

using Pipe = FaultPipe<std::string>;

constexpr Timestamp kEndOfTime = Timestamp::max();

Timestamp at(const Duration d) { return Timestamp{d}; }

Fault make_fault(std::string id, FaultParams params, Window window = {}) {
  static constexpr std::array<const char*, 5> kTypes{
      "delay", "drop", "duplicate", "reorder", "clock_skew"};
  std::string type{kTypes.at(params.index())};
  return {std::move(id), std::move(type), std::move(params), std::move(window)};
}

DropParams drop_every(const uint64_t n) {
  return {.probability = std::nullopt, .every_nth = n};
}

DropParams drop_with(const double p) {
  return {.probability = Probability{p}, .every_nth = std::nullopt};
}

DelayParams fixed_delay(const Duration d) {
  return {.mean = d, .jitter = 0ns, .distribution = std::nullopt};
}

// Submits messages 1..n (message i at i * 10ms) and flushes everything.
std::vector<Delivery> run(const Faults& faults, const uint64_t seed,
                          const uint64_t n) {
  Pipe pipe{faults, seed};
  for (uint64_t seq = 1; seq <= n; ++seq)
    pipe.submit(make_msg(at(std::chrono::milliseconds(10 * seq)), seq), {});
  return views(pipe.flush(kEndOfTime));
}

std::vector<uint64_t> sorted_sequences(const std::vector<Delivery>& ds) {
  std::vector<uint64_t> result;
  for (const auto& d : ds) result.push_back(d.sequence);
  std::ranges::sort(result);
  return result;
}

}  // namespace

// ---------------------------------------------------------------------------
// Queueing: submit / emissions / next_due / flush
// ---------------------------------------------------------------------------

TEST(FaultPipe, NothingPendingInitially) {
  Pipe pipe{{}, 1};
  EXPECT_TRUE(pipe.emissions().empty());
  EXPECT_FALSE(pipe.next_due().has_value());
  EXPECT_TRUE(pipe.flush(kEndOfTime).empty());
}

TEST(FaultPipe, NoFaultsPassesMessagesThroughUnchanged) {
  Pipe pipe{{}, 1};
  pipe.submit(make_msg(at(1s), 1), {});

  EXPECT_EQ(views(pipe.emissions()), std::vector{delivery(at(1s), 1)});
  EXPECT_EQ(pipe.next_due(), at(1s));
  EXPECT_EQ(views(pipe.flush(at(1s))), std::vector{delivery(at(1s), 1)});
  EXPECT_TRUE(pipe.emissions().empty());
  EXPECT_FALSE(pipe.next_due().has_value());
}

TEST(FaultPipe, FlushReleasesOnlyWhatIsDueInclusiveOfNow) {
  Pipe pipe{{}, 1};
  for (uint64_t seq = 1; seq <= 3; ++seq)
    pipe.submit(make_msg(at(std::chrono::seconds(seq)), seq), {});

  EXPECT_TRUE(pipe.flush(at(1s - 1ns)).empty());
  EXPECT_EQ(views(pipe.flush(at(2s))),
            (std::vector{delivery(at(1s), 1), delivery(at(2s), 2)}));
  EXPECT_EQ(pipe.next_due(), at(3s));
  EXPECT_TRUE(pipe.flush(at(3s - 1ns)).empty());
  EXPECT_EQ(views(pipe.flush(at(3s))), std::vector{delivery(at(3s), 3)});
}

TEST(FaultPipe, FlushOrdersByDueTimeNotSubmissionOrder) {
  Pipe pipe{{}, 1};
  pipe.submit(make_msg(at(3s), 1), {});
  pipe.submit(make_msg(at(1s), 2), {});
  pipe.submit(make_msg(at(2s), 3), {});

  EXPECT_EQ(pipe.next_due(), at(1s));
  EXPECT_EQ(views(pipe.flush(kEndOfTime)),
            (std::vector{delivery(at(1s), 2), delivery(at(2s), 3),
                         delivery(at(3s), 1)}));
}

TEST(FaultPipe, EqualDueTimesFlushInSubmissionOrder) {
  Pipe pipe{{}, 1};
  for (uint64_t seq = 1; seq <= 4; ++seq)
    pipe.submit(make_msg(at(1s), seq), {});

  EXPECT_EQ(views(pipe.flush(kEndOfTime)),
            (std::vector{delivery(at(1s), 1), delivery(at(1s), 2),
                         delivery(at(1s), 3), delivery(at(1s), 4)}));
}

TEST(FaultPipe, MovedPipeKeepsPendingMessages) {
  Pipe source{{make_fault("lag", fixed_delay(1s))}, 1};
  source.submit(make_msg(at(1s), 1), {});

  Pipe moved{std::move(source)};
  EXPECT_EQ(views(moved.flush(kEndOfTime)), std::vector{delivery(at(2s), 1)});
}

// ---------------------------------------------------------------------------
// Stages and windows
// ---------------------------------------------------------------------------

TEST(FaultPipe, DelayMovesTheDueTime) {
  Pipe pipe{{make_fault("lag", fixed_delay(200ms))}, 1};
  pipe.submit(make_msg(at(1s), 1), {});

  EXPECT_EQ(pipe.next_due(), at(1200ms));
  EXPECT_TRUE(pipe.flush(at(1200ms - 1ns)).empty());
  // The flushed message is stamped with the time it is delivered.
  EXPECT_EQ(views(pipe.flush(at(1200ms))),
            std::vector{delivery(at(1200ms), 1)});
}

TEST(FaultPipe, FaultAppliesOnlyInsideItsWindow) {
  const Window window{.start = AtTime{at(10s)}, .end = AtTime{at(20s)}};
  Pipe pipe{{make_fault("blackout", drop_every(1), window)}, 1};

  const std::array times{5s, 10s, 15s, 20s, 25s};
  for (uint64_t seq = 1; seq <= times.size(); ++seq)
    pipe.submit(make_msg(at(times[seq - 1]), seq), {});

  EXPECT_EQ(views(pipe.flush(kEndOfTime)),
            (std::vector{delivery(at(5s), 1), delivery(at(20s), 4),
                         delivery(at(25s), 5)}));
}

TEST(FaultPipe, EventWindowFollowsTheEventsMap) {
  const Window window{.start = AfterEvent{"go", 0s}, .end = std::nullopt};
  Pipe pipe{{make_fault("blackout", drop_every(1), window)}, 1};
  const EventTimes fired{{"go", at(2s)}};

  pipe.submit(make_msg(at(3s), 1), {});         // event not fired: passes
  pipe.submit(make_msg(at(3s), 2), fired);      // inside window: dropped
  pipe.submit(make_msg(at(1500ms), 3), fired);  // before event: passes

  EXPECT_EQ(views(pipe.flush(kEndOfTime)),
            (std::vector{delivery(at(1500ms), 3), delivery(at(3s), 1)}));
}

TEST(FaultPipe, StagesRunInDeclarationOrder) {
  const DuplicateParams dup{.probability = Probability{1.0}, .spacing = 1ms};

  // Both copies reach the drop stage, which drops the second one.
  Pipe dup_then_drop{
      {make_fault("dup", dup), make_fault("drop", drop_every(2))}, 1};
  dup_then_drop.submit(make_msg(at(1s), 1), {});
  EXPECT_EQ(views(dup_then_drop.emissions()), std::vector{delivery(at(1s), 1)});

  // The single message passes the drop stage, then gets duplicated.
  Pipe drop_then_dup{
      {make_fault("drop", drop_every(2)), make_fault("dup", dup)}, 1};
  drop_then_dup.submit(make_msg(at(1s), 1), {});
  EXPECT_EQ(views(drop_then_dup.emissions()),
            (std::vector{delivery(at(1s), 1), delivery(at(1s + 1ms), 1)}));
}

TEST(FaultPipe, DelaysAccumulateAcrossStages) {
  Pipe pipe{{make_fault("lag1", fixed_delay(100ms)),
             make_fault("lag2", fixed_delay(50ms))},
            1};
  pipe.submit(make_msg(at(1s), 1), {});
  EXPECT_EQ(views(pipe.emissions()), std::vector{delivery(at(1150ms), 1)});
}

// A stage's window is checked against the time the previous stage produced,
// not the time the message was received.
TEST(FaultPipe, LaterStageWindowSeesEarlierStageDueTime) {
  const Window from_10s{.start = AtTime{at(10s)}, .end = std::nullopt};
  Pipe pipe{{make_fault("lag", fixed_delay(5s)),
             make_fault("blackout", drop_every(1), from_10s)},
            1};

  pipe.submit(make_msg(at(3s), 1), {});  // due 8s: outside, kept
  pipe.submit(make_msg(at(6s), 2), {});  // due 11s: inside, dropped

  EXPECT_EQ(views(pipe.flush(kEndOfTime)), std::vector{delivery(at(8s), 1)});
}

TEST(FaultPipe, ReorderHoldsAMessageAcrossSubmits) {
  const ReorderParams reorder{.window_size = 2,
                              .probability = Probability{1.0}};
  Pipe pipe{{make_fault("shuffle", reorder)}, 1};

  pipe.submit(make_msg(at(1s), 1), {});
  EXPECT_TRUE(pipe.emissions().empty());
  EXPECT_FALSE(pipe.next_due().has_value());

  pipe.submit(make_msg(at(2s), 2), {});
  EXPECT_EQ(views(pipe.flush(at(2s))),
            (std::vector{delivery(at(2s), 2), delivery(at(2s), 1)}));
}

// Known bug: a message held by a reorder stage is never released once its
// window closes, because messages outside the window skip the stage. Enable
// once the held message is flushed on that path (and on shutdown).
TEST(FaultPipe, ReorderReleasesHeldMessageWhenWindowCloses) {
  const ReorderParams reorder{.window_size = 3,
                              .probability = Probability{1.0}};
  const Window until_2s{.start = std::nullopt, .end = AtTime{at(2s)}};
  Pipe pipe{{make_fault("shuffle", reorder, until_2s)}, 1};

  pipe.submit(make_msg(at(1s), 1), {});  // held
  pipe.submit(make_msg(at(3s), 2), {});  // window closed
  pipe.submit(make_msg(at(4s), 3), {});

  EXPECT_EQ(sorted_sequences(views(pipe.flush(kEndOfTime))),
            (std::vector<uint64_t>{1, 2, 3}));
}

// ---------------------------------------------------------------------------
// Reproducibility
// ---------------------------------------------------------------------------

namespace {
Faults noisy_faults() {
  return {
      make_fault("lag", DelayParams{.mean = 100ms,
                                    .jitter = 30ms,
                                    .distribution = std::nullopt}),
      make_fault("loss", drop_with(0.3)),
      make_fault("dup", DuplicateParams{.probability = Probability{0.2},
                                        .spacing = 1ms}),
  };
}
}  // namespace

TEST(FaultPipe, SameSeedReproducesTheSameOutput) {
  const auto first = run(noisy_faults(), 77, 200);
  EXPECT_FALSE(first.empty());
  EXPECT_EQ(first, run(noisy_faults(), 77, 200));
}

TEST(FaultPipe, DifferentSeedsGiveDifferentOutput) {
  EXPECT_NE(run(noisy_faults(), 1, 200), run(noisy_faults(), 2, 200));
}

// Each stage is seeded from the scenario seed and its own fault id, so adding
// a fault in front of it does not change its random choices.
TEST(FaultPipe, FaultRandomnessDoesNotDependOnOtherFaults) {
  const Window never{.start = AfterEvent{"never", 0s}, .end = std::nullopt};
  const Faults alone{make_fault("loss", drop_with(0.5))};
  const Faults behind_another{
      make_fault(
          "idle",
          DuplicateParams{.probability = Probability{1.0}, .spacing = 1ms},
          never),
      make_fault("loss", drop_with(0.5))};

  EXPECT_EQ(run(alone, 9, 200), run(behind_another, 9, 200));
}

TEST(FaultPipe, FaultIdChangesItsRandomStream) {
  EXPECT_NE(run({make_fault("a", drop_with(0.5))}, 9, 200),
            run({make_fault("b", drop_with(0.5))}, 9, 200));
}
