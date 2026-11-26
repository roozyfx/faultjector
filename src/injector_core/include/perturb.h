#ifndef INJECTOR_CORE_INCLUDE_PERTURB_H_
#define INJECTOR_CORE_INCLUDE_PERTURB_H_

#include <optional>
#include <utility>
#include <vector>

#include "fault_params.h"
#include "message.h"
#include "rng.h"

namespace fxcore {
template <Payload P>
using Emissions = std::vector<Emission<P>>;

template <Payload P>
class Perturb {
  RNG rng_;
  size_t count_{0};  // keep track of incoming messages
  std::optional<Emission<P>> held_{std::nullopt};  // For reorder
  size_t wait_{0};                                 // For reorder

 public:
  explicit Perturb(const uint64_t& rng_seed)
      : rng_{RNG(rng_seed)} {}

  Emissions<P> apply(const DelayParams& delay, const Message<P>& message);
  Emissions<P> apply(const DropParams& drop, const Message<P>& message);
  Emissions<P> apply(const DuplicateParams& duplicate,
                     const Message<P>& message);
  // Reorder changes the Payload content of the other end of window_size, and
  // keeps the timestamp of the arrived message.
  // [1 .... N] with 1 the oldest in the window -> send {N.timestamp, 1.message}
  Emissions<P> apply(const ReorderParams& reorder, const Message<P>& message);
  Emissions<P> apply(const ClockSkewParams& clock_skew,
                     const Message<P>& message);

  inline std::optional<Emission<P>> release() {
    wait_ = 0;
    return std::exchange(held_, std::nullopt);
  }
};

template <Payload P>
Emissions<P> fxcore::Perturb<P>::apply(const DelayParams& delay,
                                       const Message<P>& message) {
  ++count_;
  // TODO what if delay.distribution?

  const Duration random_duration{rng_.gaussian(delay.mean, delay.jitter)};

  return {{message.received_time + random_duration, message.sequence,
           message.payload}};
}

template <Payload P>
Emissions<P> fxcore::Perturb<P>::apply(const DropParams& drop,
                                       const Message<P>& message) {
  Emission unperturbed{to_emission(message)};

  if (drop.probability) {
    const auto random_number{rng_.random_probability()};
    if (random_number <= drop.probability->value()) {
      return {};
    }
    ++count_;
  } else if (drop.every_nth &&
             ++count_ % *drop.every_nth == 0) {  // drop the nth message
    return {};
  }

  // No dropping happens
  return {unperturbed};
}

template <Payload P>
Emissions<P> fxcore::Perturb<P>::apply(const DuplicateParams& duplicate,
                                       const Message<P>& message) {
  const auto random_number{rng_.random_probability()};
  if (random_number <= duplicate.probability.value()) {
    count_ += 2;
    return {to_emission(message),
            {message.received_time + Duration(duplicate.spacing),
             message.sequence, message.payload}};
  }
  ++count_;
  return {to_emission(message)};
}

// Reorder changes the Payload content of the other end of window_size, and
// keeps the timestamp of the arrived message.
// [1 .... N] with 1 the oldest in the window -> send {N.timestamp, 1.message}
template <Payload P>
Emissions<P> fxcore::Perturb<P>::apply(const ReorderParams& reorder,
                                       const Message<P>& message) {
  Emission em{to_emission(message)};

  const auto random_number{rng_.random_probability()};
  if (held_) {
    if (--wait_ > 0) return {em};

    Emission late{em.due_time, held_->sequence, std::move(held_->payload)};
    held_.reset();
    return {em, std::move(late)};
  }
  if (random_number <= reorder.probability.value()) {
    held_ = std::move(em);
    wait_ = reorder.window_size - 1;
    return {};
  }
  return {to_emission(message)};
}

template <Payload P>
Emissions<P> fxcore::Perturb<P>::apply(const ClockSkewParams& clock_skew
                                       [[maybe_unused]],
                                       const Message<P>& message) {
  // TODO
  return {{to_emission(message)}};
}

}  // namespace fxcore
#endif  // INJECTOR_CORE_INCLUDE_PERTURB_H_
