#include "rng.h"

#include <cmath>  // for std::isnan
#include <limits>
#include <stdexcept>

#include "timestamp.h"

using namespace fxcore;

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

Duration RNG::gaussian(const Duration& mean, const Duration& stddev) {
  if (stddev < Duration::zero()) {
    throw std::invalid_argument("'stddev' must be non-negative.");
  }
  // with standard deviation at 0, fix the return to mean.
  if (stddev == Duration::zero()) return mean;

  return mean + Duration{std::llround(
                    gaussian(0.0, static_cast<double>(stddev.count())))};
}

double RNG::random_probability() { return in_interval_uniform(0., 1.); }
