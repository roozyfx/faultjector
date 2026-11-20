#include <gtest/gtest.h>

#include <chrono>

#include "timestamp.h"
#include "window.h"
using namespace std::chrono_literals;
using namespace fxcore;

TEST(WindowContains, UnsetWindowIsAlwaysOpen) {
  const Window w{};
  const EventTimes map{};
  EXPECT_TRUE(w.contains(Timestamp(0s), map));
  EXPECT_TRUE(w.contains(Timestamp(1000s), map));
}

TEST(WindowContains, InclusiveAtTimeStart) {
  const Window w{.start = AtTime{10s}, .end = std::nullopt};
  const EventTimes map{};
  EXPECT_FALSE(w.contains(Timestamp(10s - 1ns), map));
  EXPECT_TRUE(w.contains(Timestamp(10s), map));
  EXPECT_TRUE(w.contains(Timestamp(100s), map));
}

TEST(WindowContains, ExclusiveAtTimeEnd) {
  const Window w{.start = std::nullopt, .end = AtTime{5s}};
  const EventTimes map{};
  EXPECT_TRUE(w.contains(Timestamp(0s), map));
  EXPECT_TRUE(w.contains(Timestamp(5s - 1ns), map));
  EXPECT_FALSE(w.contains(Timestamp(5s), map));
}

TEST(WindowContains, DurationFromStart) {
  const Window w{.start = AtTime{10s}, .end = ForDuration{15s}};
  const EventTimes map{};
  EXPECT_TRUE(w.contains(Timestamp(22s), map));
  EXPECT_FALSE(w.contains(Timestamp(6s), map));
  EXPECT_FALSE(w.contains(Timestamp(25s), map));
  EXPECT_FALSE(w.contains(Timestamp(30s), map));
}

TEST(WindowContains, DurationWithoutStartIsFromZero) {
  const Window w{.start = std::nullopt, .end = ForDuration{3s}};
  const EventTimes map{};
  EXPECT_TRUE(w.contains(Timestamp(0s), map));
  EXPECT_TRUE(w.contains(Timestamp(2500ms), map));
  EXPECT_FALSE(w.contains(Timestamp(3s), map));
}

TEST(WindowContains, AfterEventNotFiredIsClosed) {
  const Window w{.start = AfterEvent{"goal_accepted", 10s},
                 .end = ForDuration{15s}};
  EventTimes map{};
  EXPECT_FALSE(w.contains(Timestamp(100s), map));
  map["other_event"] = Timestamp(1s);
  EXPECT_FALSE(w.contains(Timestamp(100s), map));
}

TEST(WindowContains, AfterEventFromEventTime) {
  const Window w{.start = AfterEvent{"goal_accepted", 10s},
                 .end = ForDuration{15s}};
  const EventTimes map{{"goal_accepted", Timestamp(30s)}};

  EXPECT_FALSE(w.contains(Timestamp(40s - 1ns), map));
  EXPECT_TRUE(w.contains(Timestamp(40s), map));
  EXPECT_TRUE(w.contains(Timestamp(55s - 1ns), map));
  EXPECT_FALSE(w.contains(Timestamp(55s), map));
}

TEST(WindowContains, AfterEventWithSetEnd) {
  const Window w{.start = AfterEvent{"robot_burnt", 0s}, .end = AtTime{1min}};
  const EventTimes map{{"robot_burnt", Timestamp(30s)}};
  EXPECT_FALSE(w.contains(Timestamp(29s), map));
  EXPECT_TRUE(w.contains(Timestamp(30s), map));
  EXPECT_FALSE(w.contains(Timestamp(1min), map));
  EXPECT_FALSE(w.contains(Timestamp(60s), map));
}

TEST(WindowContains, EndBeforeStartIsEmpty) {
  const Window w{.start = AtTime{20s}, .end = AtTime{10s}};
  const EventTimes map{};
  EXPECT_FALSE(w.contains(Timestamp(4s), map));
  EXPECT_FALSE(w.contains(Timestamp(7s), map));
  EXPECT_FALSE(w.contains(Timestamp(12s), map));
}
