#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

#include "probability.h"

using namespace fxcore;

TEST(Probability, DefaultsToZero) {
  EXPECT_DOUBLE_EQ(Probability{}.value(), 0.0);
}

TEST(Probability, KeepsValueInRange) {
  EXPECT_DOUBLE_EQ(Probability{0.25}.value(), 0.25);
  EXPECT_DOUBLE_EQ(Probability{0.5}.value(), 0.5);
}

TEST(Probability, BoundsAreInclusive) {
  EXPECT_DOUBLE_EQ(Probability{0.0}.value(), 0.0);
  EXPECT_DOUBLE_EQ(Probability{1.0}.value(), 1.0);
}

TEST(Probability, RejectsOutOfRange) {
  EXPECT_THROW(Probability{-0.01}, std::out_of_range);
  EXPECT_THROW(Probability{1.01}, std::out_of_range);
  EXPECT_THROW(Probability{-std::numeric_limits<double>::min()},
               std::out_of_range);
}

TEST(Probability, RejectsNonFinite) {
  EXPECT_THROW(Probability{std::numeric_limits<double>::quiet_NaN()},
               std::out_of_range);
  EXPECT_THROW(Probability{std::numeric_limits<double>::infinity()},
               std::out_of_range);
  EXPECT_THROW(Probability{-std::numeric_limits<double>::infinity()},
               std::out_of_range);
}
