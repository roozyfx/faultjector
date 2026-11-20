#ifndef INJECTOR_CORE_INCLUDE_RNG_H_
#define INJECTOR_CORE_INCLUDE_RNG_H_
#include <chrono>
#include <concepts>
#include <random>
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

using ns = std::chrono::nanoseconds;
// To make it easier to change if desired. But not as template parameter,
// since it doesn't make sense in this project
using Engine = std::mt19937_64;

/* seeded RandomNumberGenerator class.
same seed => same numbers, on the same stdlib
*/
class RNG {
 public:
  explicit RNG(const uint64_t& seed = 0)
      : seed_{seed} {
    std::random_device rd;
    engine_ = Engine(rd());
    engine_.seed(seed);
  }

  RNG(RNG&&) = default;
  RNG& operator=(RNG&&) = default;
  // Prevent copies
  RNG(const RNG&) = delete;
  RNG& operator=(const RNG&) = delete;
  ~RNG() = default;

  inline uint64_t seed() const { return seed_; }

  // gaussian distribution for floating_point numbers
  template <std::floating_point T>
  T gaussian(const T& mean, const T& stddev);

  // gaussian distribution for nanoseconds
  ns gaussian(const ns& mean_ns, const ns& stddev_ns);

  // [0, 1)
  double random_probability();

 private:
  // ints/reals uniform distribution over [low, high]
  template <numerics T>
  T in_interval_uniform(const T& low, const T& high);

  // pass an arbitrary distribution. Note that params are not checked for an
  // arbitrary one, only uniform and normal dist. are checked (above)
  template <typename RND>
    requires RandomNumberDistribution<RND, Engine>
  auto in_interval_rnd(RND rand_num_distribution);

  Engine engine_;
  uint64_t seed_;
};

#endif  // INJECTOR_CORE_INCLUDE_RNG_H_
