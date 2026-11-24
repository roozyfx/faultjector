#ifndef INJECTOR_CORE_INCLUDE_FAULT_PIPE_H_
#define INJECTOR_CORE_INCLUDE_FAULT_PIPE_H_
#include <optional>
#include <set>
#include <vector>

#include "fault.h"
#include "message.h"
#include "perturb.h"
#include "timestamp.h"

namespace fxcore {

template <Payload P>
class FaultPipe {
  struct Stage {
    Fault fault;
    Perturb<P> perturb;
  };
  std::vector<Stage> stages_;
  std::multiset<Emission<P>> emissions_;

 public:
  explicit FaultPipe(const Faults& faults, const uint64_t scenario_seed) {
    for (const auto& fault : faults) {
      stages_.push_back(
          {fault,
           Perturb<P>(scenario_seed + std::hash<std::string>{}(fault.id))});
    }
  }

  ~FaultPipe() = default;

  FaultPipe(const FaultPipe&) = delete;
  FaultPipe& operator=(const FaultPipe&) = delete;
  FaultPipe(FaultPipe&&) noexcept = default;
  FaultPipe& operator=(FaultPipe&&) = default;

  void submit(const Message<P>& message, const EventTimes& events_map) {
    std::vector<Emission<P>> working{
        {message.received_time, message.sequence, message.payload}};

    for (auto& stage : stages_) {
      std::vector<Emission<P>> next;
      for (auto& em : working) {
        // Outside of the fault's window, passes through
        if (!stage.fault.window.contains(em.due_time, events_map)) {
          // 'working' is substituted by 'next' later, so it's safe to move(em)
          next.push_back(std::move(em));
          continue;
        }

        const auto msg = to_message<P>(em);
        std::vector<Emission<P>> out = std::visit(
            [&](const auto& f) { return stage.perturb.apply(f, msg); },
            stage.fault.params);

        next.insert_range(next.cend(), out);
      }
      working = std::move(next);
    }

    emissions_.insert_range(working);
  }

  inline const std::multiset<Emission<P>>& emissions() const {
    return emissions_;
  }

  std::vector<Message<P>> flush(const Timestamp& now) {
    std::vector<Message<P>> result;
    while (!emissions_.empty() && emissions_.begin()->due_time <= now) {
      auto em = emissions_.extract(emissions_.begin()).value();
      result.push_back({em.due_time, em.sequence, std::move(em.payload)});
    }
    return result;
  }

  std::optional<Timestamp> next_due() const {
    if (emissions_.empty()) return std::nullopt;
    return emissions_.cbegin()->due_time;
  }
};
}  // namespace fxcore
#endif  // INJECTOR_CORE_INCLUDE_FAULT_PIPE_H_
