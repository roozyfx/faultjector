#include "window.h"

#include <chrono>
#include <optional>

#include "core_types.h"
#include "timestamp.h"

using namespace std::chrono_literals;

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};

std::optional<fxcore::Timestamp> fxcore::Window::handle_start(
    const EventTimes& events) const {
  if (!start) return Timestamp(0ns);

  return std::visit(
      overloaded{[](const AtTime& t) -> std::optional<Timestamp> {
                   return Timestamp(t.time);
                 },
                 [&](const AfterEvent& e) -> std::optional<Timestamp> {
                   if (events.contains(e.event)) {
                     return events.at(e.event) + Timestamp(e.after);
                   } else {
                     return std::nullopt;
                   }
                 }},
      *start);
}

bool fxcore::Window::contains(const Timestamp& ts,
                              const EventTimes& events) const {
  auto strt = handle_start(events);

  if (!strt.has_value() || ts < strt.value()) return false;

  if (!end) return true;

  return std::visit(
      overloaded{[&](const AtTime& t) -> bool { return ts.value() < t.time; },
                 [&](const ForDuration& d) -> bool {
                   return ts < strt.value() + Timestamp(d.duration);
                 }},
      *end);
}
