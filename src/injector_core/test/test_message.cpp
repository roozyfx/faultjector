#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

#include "message.h"
#include "test_helpers.h"
#include "timestamp.h"

using namespace std::chrono_literals;
using namespace fxcore;
using namespace fxtest;

static_assert(Payload<std::string>);
static_assert(Payload<int>);
static_assert(!Payload<std::unique_ptr<int>>);

TEST(Emission, OrdersByDueTimeOnly) {
  const Em early{Timestamp{1s}, 9, "z"};
  const Em late{Timestamp{2s}, 1, "a"};
  EXPECT_TRUE(early < late);
  EXPECT_FALSE(late < early);
}

TEST(Emission, SameDueTimeIsEquivalent) {
  const Em a{Timestamp{1s}, 1, "a"};
  const Em b{Timestamp{1s}, 2, "b"};
  EXPECT_FALSE(a < b);
  EXPECT_FALSE(b < a);
}

TEST(Message, ToEmissionKeepsEveryField) {
  const auto m = make_msg(Timestamp{3s}, 4);
  EXPECT_EQ(view(to_emission(m)), delivery(Timestamp{3s}, 4));
}

TEST(Message, ToMessageKeepsEveryField) {
  const Em e{Timestamp{5s}, 6, payload_of(6)};
  EXPECT_EQ(view(to_message(e)), delivery(Timestamp{5s}, 6));
}

TEST(Message, RoundTripIsLossless) {
  const auto m = make_msg(Timestamp{7s + 1ns}, 8);
  EXPECT_EQ(view(to_message(to_emission(m))), view(m));
}
