#include "rng.h"

#include <cmath>  // for std::isnan
#include <stdexcept>

template <numerics T>
inline T RNG::in_interval_uniform(const T& low, const T& high) {
  if (low > high) {
    throw std::invalid_argument("'low' should be less than 'high'.");
  }
  if (std::isnan(low) || std::isnan(high)) {
    throw std::invalid_argument("NaN bounds.");
  }

  if constexpr (std::is_integral_v<T>) {
    return in_interval_rnd(std::uniform_int_distribution<T>(low, high));
  } else if constexpr (std::is_floating_point_v<T>) {
    if (!(low <= high) || !(high - low <= std::numeric_limits<T>::max())) {
      throw std::invalid_argument("Infinite bounds.");
    }
    const auto h{std::nextafter(high, std::numeric_limits<T>::max())};
    return in_interval_rnd(std::uniform_real_distribution<T>(low, h));
  }
}

template <typename RND>
  requires RandomNumberDistribution<RND, Engine>
inline auto RNG::in_interval_rnd(RND rand_num_distribution) {
  return rand_num_distribution(engine_);
}

template <std::floating_point T>
inline T RNG::gaussian(const T& mean, const T& stddev) {
  if (stddev <= 0 || std::isnan(stddev) || !std::isfinite(stddev)) {
    throw std::invalid_argument("'stddev' must be a positive real number.");
  }
  if (std::isnan(mean) || std::isinf(mean)) {
    throw std::invalid_argument("'mean' must be a finite real number.");
  }
  return in_interval_rnd(std::normal_distribution<T>(mean, stddev));
}

ns RNG::gaussian(const ns& mean_ns, const ns& stddev_ns) {
  // with standard deviation at 0, fix the return to mean.
  if (stddev_ns == ns::zero()) return mean_ns;

  const double stddev{static_cast<double>(stddev_ns.count())};
  if (stddev < 0 || std::isnan(stddev) || !std::isfinite(stddev)) {
    throw std::invalid_argument("'stddev' must be a positive real number.");
  }

  const double mean{static_cast<double>(mean_ns.count())};
  if (std::isnan(mean) || std::isinf(mean)) {
    throw std::invalid_argument("'mean' must be a finite real number.");
  }

  const auto rand_d{
      in_interval_rnd(std::normal_distribution<double>(mean, stddev))};
  return ns(static_cast<int64_t>(rand_d));
}

double RNG::random_probability() { return in_interval_uniform(0., 1.); }
