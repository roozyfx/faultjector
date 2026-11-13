#ifndef INJECTOR_CORE_INCLUDE_PERTURB_HPP_
#define INJECTOR_CORE_INCLUDE_PERTURB_HPP_
#include <vector>

#include "fault_parameters.h"
#include "message.h"
#include "rng.h"
#include "timestamp.h"

class Perturb {
  std::vector<Emission> buffer_;
  // RNG<std::mt19937_64> rng_;

 public:
  std::vector<Emission> operator()(const DelayParams& delay,
                                   const uint64_t rng_seed,
                                   const Message& message) {
    RNG rng_(rng_seed);
    const double mean_d{static_cast<double>(delay.mean.count())};
    const double stddev_d{static_cast<double>(delay.jitter.count())};
    // TODO what if delay.distribution?
    const auto random_ns{rng_.gaussian(delay.mean, delay.jitter)};
    const Timestamp random_ts{Timestamp(random_ns)};
    Emission perturbed{message.sensor_time + random_ts, message.message};
    buffer_.push_back(perturbed);
    return {perturbed};
  }

  std::vector<Emission> operator()(const DropParams& drop,
                                   const uint64_t rng_seed,
                                   const Message& message) {
    RNG rng_(rng_seed);
    if (drop.probability.has_value()) {
      const auto random_number{rng_.random_probability()};
      if (random_number <= drop.probability->value()) {
        return {};
      } else {
        Emission unperturbed{message.sensor_time, message.message};
        buffer_.push_back(unperturbed);
        return {unperturbed};
      }
    }

    if (drop.every_nth.has_value()) {
      // Already 'every_nth - 1' in buffer_, so this message is the nth
      if (buffer_.size() == drop.every_nth.value() - 1) {
        return {};
      }
    }
    // Shouldn't reach here
    return {};
  }

  std::vector<Emission> operator()(const DuplicateParams& duplicate,
                                   const uint64_t rng_seed,
                                   const Message& message) {
    RNG rng_(rng_seed);
    const auto random_number{rng_.random_probability()};
    std::vector<Emission> perturbed;
    if (random_number <= duplicate.probability.value()) {
      const auto duplicate_ts{message.sensor_time +
                              Timestamp(duplicate.spacing)};
      perturbed = {{message.sensor_time, message.message},
                   {message.sensor_time + duplicate_ts, message.message}};
    } else {
      perturbed = {{message.sensor_time, message.message}};
    }
    buffer_.insert_range(buffer_.cend(), perturbed);
    return perturbed;
  }

  // Reorder changes the Payload content of the other end of window_size, and
  // keeps the timestamp of the arrived message.
  // [1 .... N] with 1 the oldest in the window -> send {N.timestamp, 1.message}
  std::vector<Emission> operator()(const ReorderParams& reorder,
                                   const uint64_t rng_seed,
                                   const Message& message) {
    RNG rng_(rng_seed);
    const auto random_number{rng_.random_probability()};
    if (random_number <= reorder.probability.value()) {
      std::vector<Emission> window(reorder.window_size);
      std::ranges::copy(buffer_.end() - reorder.window_size - 1, buffer_.end(),
                        window.begin());
      Emission oldest{window[reorder.window_size - 1]};
      Emission perturbed{message.sensor_time, oldest.message};
      buffer_.push_back(perturbed);
      return {perturbed};
    }
    return {{message.sensor_time, message.message}};
  }

  std::vector<Emission> operator()(const ClockSkewParams& clock_skew,
                                   const uint64_t rng_seed,
                                   const Message& message) {
    // TODO
    return {
        {message.sensor_time + Timestamp(clock_skew.offset), message.message}};
  }
};

#endif  // INJECTOR_CORE_INCLUDE_PERTURB_H_
