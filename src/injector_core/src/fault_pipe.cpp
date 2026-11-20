#include "fault_pipe.h"

#include <variant>

#include "message.h"
#include "window.h"

using namespace fxcore;

void FaultPipe::submit(const Message& message) {
  std::vector<Emission> working{{message.sensor_time, message.message}};

  for (auto& stage : stages_) {
    std::vector<Emission> next;
    for (auto& em : working) {
      // Outside of the fault's window, passes through
      if (!stage.fault.window.contains(em.due_time, events_map_)) {
        // 'working' is substituted by 'next' later, so it's safe to move(em)
        next.push_back(std::move(em));
        continue;
      }

      const auto msg = to_message(em);
      std::vector<Emission> out =
          std::visit([&](const auto& f) { return stage.perturb.apply(f, msg); },
                     stage.fault.params);

      next.insert_range(next.cend(), out);
    }
    working = std::move(next);
  }

  emissions_.insert_range(working);
}

std::vector<Message> FaultPipe::flush(const Timestamp& now) {
  std::vector<Message> result;
  while (!emissions_.empty() && emissions_.begin()->due_time <= now) {
    auto em = emissions_.extract(emissions_.begin()).value();
    result.push_back({em.due_time, std::move(em.message)});
  }
  return result;
}

std::optional<Timestamp> FaultPipe::next_due() const {
  if (emissions_.empty()) return std::nullopt;
  return emissions_.cbegin()->due_time;
}
