#ifndef INJECTOR_CORE_INCLUDE_PERTURB_H_
#define INJECTOR_CORE_INCLUDE_PERTURB_H_

#include <optional>
#include <vector>

#include "fault_params.h"
#include "message.h"
#include "rng.h"

using Emissions = std::vector<Emission>;

class Perturb {
  RNG rng_;
  size_t count_{0};               // keep track of incoming messages
  std::optional<Emission> held_;  // For reorder
  size_t wait_;                   // For reorder

 public:
  explicit Perturb(const uint64_t& rng_seed)
      : rng_{RNG(rng_seed)} {}

  Emissions apply(const DelayParams& delay, const Message& message);
  Emissions apply(const DropParams& drop, const Message& message);
  Emissions apply(const DuplicateParams& duplicate, const Message& message);
  // Reorder changes the Payload content of the other end of window_size, and
  // keeps the timestamp of the arrived message.
  // [1 .... N] with 1 the oldest in the window -> send {N.timestamp, 1.message}
  Emissions apply(const ReorderParams& reorder, const Message& message);
  Emissions apply(const ClockSkewParams& clock_skew, const Message& message);
};

#endif  // INJECTOR_CORE_INCLUDE_PERTURB_HPP_
