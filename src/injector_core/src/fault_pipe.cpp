#include "fault_pipe.h"

#include <string>
#include <variant>

#include "perturb.hpp"
#include "window.h"

void FaultPipe::submit(const Message& message) {
  std::vector<Emission> working{{message.sensor_time, message.message}};
  std::vector<Emission> next;

  for (const auto& fault : faults_) {
    // create a seed from the Scenario seed and current fault's id
    const auto rng_seed{scenario_seed_ + std::hash<std::string>{}(fault.id)};

    std::vector<Emission> next;
    for (auto& em : working) {
      if (!fault.window.contains(em.due_time, events_map_)) continue;

      const auto msg = to_message(em);
      std::vector<Emission> out =
          std::visit([&](auto&& flt) { return Perturb{}(flt, rng_seed, msg); },
                     fault.params);
      next.insert_range(next.cend(), out);
    }
    working = std::move(next);
  }

  emissions_.insert_range(working);
}

std::vector<Message> FaultPipe::flush(const Timestamp& now) {
  if (emissions_.empty()) return {};

  std::vector<Message> result;
  auto it = emissions_.cbegin();
  for (; it->due_time <= now; ++it) {
    result.push_back(to_message(*it));
  }

  std::erase_if(emissions_,
                [&now](const Emission& e) { return e.due_time <= now; });

  return result;
}

std::optional<Timestamp> FaultPipe::next_due() const {
  if (emissions_.empty()) return std::nullopt;
  return emissions_.cbegin()->due_time;
}
