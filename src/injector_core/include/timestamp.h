#ifndef INJECTOR_CORE_INCLUDE_TIMESTAMP_H_
#define INJECTOR_CORE_INCLUDE_TIMESTAMP_H_
#include <chrono>

namespace fxcore {

struct SimClock {
  using duration = std::chrono::nanoseconds;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<SimClock>;
  static constexpr bool is_steady = false;
};

using Timestamp = SimClock::time_point;
using Duration = SimClock::duration;
inline constexpr Timestamp time_zero = Timestamp{Duration{0}};
inline constexpr Duration zero_duration = Duration{0};

}  // namespace fxcore
#endif  // INJECTOR_CORE_INCLUDE_TIMESTAMP_H_
