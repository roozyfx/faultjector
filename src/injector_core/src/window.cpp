#include "window.h"

#include <optional>
#include <variant>

#include "events.h"
#include "timestamp.h"

namespace {
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
}  // namespace

std::optional<fxcore::Timestamp> fxcore::Window::handle_start(
    const EventTimes& events) const {
  if (!start) return time_zero;

  return std::visit(
      overloaded{
          [](const AtTime& t) -> std::optional<Timestamp> { return t.time; },
          [&](const AfterEvent& e) -> std::optional<Timestamp> {
            if (events.contains(e.event)) {
              return events.at(e.event) + e.after;
            }
            return std::nullopt;
          }},
      *start);
}

bool fxcore::Window::contains(const Timestamp& ts,
                              const EventTimes& events) const {
  auto strt = handle_start(events);

  if (!strt.has_value() || ts < strt.value()) return false;

  if (!end) return true;

  return std::visit(
      overloaded{[&](const AtTime& t) -> bool { return ts < t.time; },
                 [&](const ForDuration& d) -> bool {
                   return ts < *strt + d.duration;
                 }},
      *end);
}
