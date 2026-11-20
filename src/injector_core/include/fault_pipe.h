#ifndef INJECTOR_CORE_INCLUDE_FAULT_PIPE_H_
#define INJECTOR_CORE_INCLUDE_FAULT_PIPE_H_
#include <optional>
#include <set>
#include <vector>

#include "fault.h"
#include "message.h"
#include "perturb.h"
#include "timestamp.h"

class FaultPipe {
  struct Stage {
    Fault fault;
    Perturb perturb;
  };
  std::vector<Stage> stages_;
  EventTimes events_map_;
  std::multiset<Emission> emissions_;

 public:
  explicit FaultPipe(const Faults& faults, const uint64_t scenario_seed,
                     const EventTimes& events_map)
      : events_map_{events_map} {
    for (const auto& fault : faults) {
      stages_.push_back(
          {fault, Perturb(scenario_seed + std::hash<std::string>{}(fault.id))});
    }
  }

  ~FaultPipe() = default;

  FaultPipe(const FaultPipe&) = delete;
  FaultPipe& operator=(const FaultPipe&) = delete;
  FaultPipe(FaultPipe&&) noexcept = default;
  FaultPipe& operator=(FaultPipe&&) = default;

  void submit(const Message& msg);

  inline std::multiset<Emission> emissions() const { return emissions_; }

  std::vector<Message> flush(const Timestamp& now);

  std::optional<Timestamp> next_due() const;
};

#endif  // INJECTOR_CORE_INCLUDE_FAULT_PIPE_H_
