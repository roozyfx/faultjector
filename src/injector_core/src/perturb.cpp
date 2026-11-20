#include "perturb.h"

#include <algorithm>

#include "message.h"
#include "timestamp.h"

using fxcore::Emissions;

Emissions fxcore::Perturb::apply(const DelayParams& delay,
                                 const Message& message) {
  ++count_;
  // TODO what if delay.distribution?
  auto random_ns{rng_.gaussian(delay.mean, delay.jitter)};

  // TODO
  //  If stddev is close to mean, clamping would be a problem, clamping
  //  random_ns to 0. If that is a realistic use-case do something else
  //  instead, e.g. redraw. I found that unrealistic
  random_ns = std::clamp(random_ns, ns::zero(), ns::max());
  const Timestamp random_ts{Timestamp(random_ns)};

  return {{message.sensor_time + random_ts, message.message}};
}

Emissions fxcore::Perturb::apply(const DropParams& drop,
                                 const Message& message) {
  Emission unperturbed{to_emission(message)};

  if (drop.probability) {
    const auto random_number{rng_.random_probability()};
    if (random_number <= drop.probability->value()) {
      return {};
    }
  }

  // This is the n'th message, drop it and reset the counter
  if (drop.every_nth && ++count_ % *drop.every_nth == 0) {
    return {};
  }

  // No dropping happens
  ++count_;
  return {unperturbed};
}

Emissions fxcore::Perturb::apply(const DuplicateParams& duplicate,
                                 const Message& message) {
  const auto random_number{rng_.random_probability()};
  if (random_number <= duplicate.probability.value()) {
    count_ += 2;
    return {
        to_emission(message),
        {message.sensor_time + Timestamp(duplicate.spacing), message.message}};
  }
  ++count_;
  return {to_emission(message)};
}

// Reorder changes the Payload content of the other end of window_size, and
// keeps the timestamp of the arrived message.
// [1 .... N] with 1 the oldest in the window -> send {N.timestamp, 1.message}
Emissions fxcore::Perturb::apply(const ReorderParams& reorder,
                                 const Message& message) {
  Emission em{to_emission(message)};

  const auto random_number{rng_.random_probability()};
  if (held_) {
    if (--wait_ > 0) return {};

    Emission late{em.due_time, std::move(held_->message)};
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

Emissions fxcore::Perturb::apply(const ClockSkewParams& clock_skew,
                                 const Message& message) {
  // TODO
  return {
      {message.sensor_time + Timestamp(clock_skew.offset), message.message}};
}
