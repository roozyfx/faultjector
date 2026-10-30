#ifndef INJECTOR_CORE_INCLUDE_FAULT_PARAMETERS_H_
#define INJECTOR_CORE_INCLUDE_FAULT_PARAMETERS_H_

#include <chrono>
#include <optional>
#include <variant>

#include "utils.h"

using Duration = std::chrono::nanoseconds;

/* There are 5 and only 5 type of Fault:
Delay, Drop, Duplicate, Reorder, Clock Skew */

// Delay: hold each message for mean +/- jitter.
struct DelayParams {
  Duration mean{};
  Duration jitter{};
  // Unset: the injector's default distribution.
  std::optional<std::string> distribution;
};

// Drop: exactly one of the two members is set.
struct DropParams {
  std::optional<Probability> probability;
  // Should be at least 1
  std::optional<uint64_t> every_nth;
};

// Duplicate: duplicate the message
struct DuplicateParams {
  Probability probability{};
  // time distance between the two copies
  Duration spacing{};
};

// Reorder: reorder messages in the window_size with the probability
struct ReorderParams {
  // Should be at least 2
  size_t window_size{};
  Probability probability{};
};

// Clock Skew
struct ClockSkewParams {
  // Added to `header.stamp`; may be negative.
  Duration offset{};
};

using FaultParams = std::variant<DelayParams, DropParams, DuplicateParams,
                                 ReorderParams, ClockSkewParams>;

#endif  // INJECTOR_CORE_INCLUDE_FAULT_PARAMETERS_H_
