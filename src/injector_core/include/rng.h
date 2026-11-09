#ifndef INJECTOR_CORE_INCLUDE_RNG_H_
#define INJECTOR_CORE_INCLUDE_RNG_H_
#include <chrono>
#include <cmath>  // for std::isnan
#include <concepts>
#include <random>
#include <stdexcept>
#include <type_traits>

// roughly what std wants from a distribution
template <class Distribution, class Engine>
concept RandomNumberDistribution = requires(Distribution dist, Engine& gen) {
  typename Distribution::result_type;
  typename Distribution::param_type;
  { dist(gen) } -> std::convertible_to<typename Distribution::result_type>;
  { dist.min() } -> std::convertible_to<typename Distribution::result_type>;
  { dist.max() } -> std::convertible_to<typename Distribution::result_type>;
};

template <typename T, typename... Us>
concept any_of_types = (std::same_as<T, Us> || ...);

// concept to limit types to only numbers and exclude chars and bool
template <typename T>
concept numerics =
    (std::integral<T> || std::floating_point<T>) &&
    !any_of_types<std::remove_cv_t<T>, bool, char, signed char, unsigned char,
                  wchar_t, char8_t, char16_t, char32_t>;

/* seeded RandomNumberGenerator class.
same seed => same numbers, on the same stdlib
*/
template <typename Engine = std::mt19937_64>
class RNG {
 public:
  explicit RNG(const uint64_t& seed)
      : seed_{seed} {
    std::random_device rd;
    engine_ = Engine(rd());
    engine_.seed(seed);
  }

  RNG(const RNG&) = delete;
  RNG& operator=(const RNG&) = delete;
  RNG(const RNG&&) = delete;
  RNG& operator=(const RNG&&) = delete;
  ~RNG() = default;

  uint64_t seed() const { return seed_; }

  void seed(const uint64_t& seed) {
    seed_ = seed;
    engine_.seed(seed_);
  }

  // ints/reals uniform distribution over [low, high]
  template <numerics T>
  T in_interval_uniform(const T& low, const T& high) {
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

  // gaussian distribution for floating_point numbers
  template <std::floating_point T>
  T gaussian(const T& mean, const T& stddev) {
    if (stddev <= 0 || std::isnan(stddev) || !std::isfinite(stddev)) {
      throw std::invalid_argument("'stddev' must be a positive real number.");
    }
    if (std::isnan(mean) || std::isinf(mean)) {
      throw std::invalid_argument("'mean' must be a finite real number.");
    }
    return in_interval_rnd(std::normal_distribution<T>(mean, stddev));
  }

  // gaussian distribution for nanoseconds
  using ns = std::chrono::nanoseconds;
  ns gaussian(const ns& mean_ns, const ns& stddev_ns) {
    const double mean{mean_ns.count()};
    const double stddev{stddev_ns.count()};
    if (stddev <= 0 || std::isnan(stddev) || !std::isfinite(stddev)) {
      throw std::invalid_argument("'stddev' must be a positive real number.");
    }
    if (std::isnan(mean) || std::isinf(mean)) {
      throw std::invalid_argument("'mean' must be a finite real number.");
    }
    const auto rand_d{
        in_interval_rnd(std::normal_distribution<double>(mean, stddev))};
    return ns(static_cast<int64_t>(rand_d));
  }

  // pass an arbitrary distribution. Note that params are not checked for an
  // arbitrary one, only uniform and normal dist. are checked (above)
  template <typename RND>
    requires RandomNumberDistribution<RND, Engine>
  auto in_interval_rnd(RND rand_num_distribution) {
    return rand_num_distribution(engine_);
  }

  // [0, 1)
  double random_probability() { return in_interval_uniform(0., 1.); }

 private:
  Engine engine_;
  uint64_t seed_;
};
#endif  // INJECTOR_CORE_INCLUDE_RNG_H_
