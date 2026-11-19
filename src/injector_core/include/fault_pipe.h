#ifndef INJECTOR_CORE_INCLUDE_FAULT_PIPE_H_
#define INJECTOR_CORE_INCLUDE_FAULT_PIPE_H_
#include <optional>
#include <set>
#include <vector>

#include "fault.h"
#include "message.h"
#include "timestamp.h"

class FaultPipe {
  Faults faults_;
  uint64_t scenario_seed_;
  EventTimes events_map_;
  std::multiset<Emission> emissions_;

 public:
  explicit FaultPipe(const Faults& faults, const uint64_t scenario_seed,
                     const EventTimes& events_map)
      : faults_{faults},
        scenario_seed_{scenario_seed},
        events_map_{events_map} {}
  ~FaultPipe() = default;

  FaultPipe(const FaultPipe&) = delete;
  FaultPipe& operator=(const FaultPipe&) = delete;

  void submit(const Message& msg);

  inline std::multiset<Emission> emissions() const { return emissions_; }

  std::vector<Message> flush(const Timestamp& now);

  std::optional<Timestamp> next_due() const;
};

#endif  // INJECTOR_CORE_INCLUDE_FAULT_PIPE_H_
