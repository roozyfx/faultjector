#ifndef INJECTOR_CORE_INCLUDE_UTILS_H_
#define INJECTOR_CORE_INCLUDE_UTILS_H_
#include <stdexcept>

class Probability {
  double value_{0.0};
  static constexpr double check(const double d) {
    if (d < 0. || d > 1.) throw std::out_of_range("value should be in [0, 1]");
    return d;
  }

 public:
  constexpr Probability() noexcept = default;  // default to 0.0
  explicit Probability(const double& d)
      : value_{check(d)} {}

  double value() const { return value_; }
};
#endif  //  INJECTOR_CORE_INCLUDE_UTILS_H_
