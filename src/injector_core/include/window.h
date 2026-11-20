#ifndef INJECTOR_CORE_INCLUDE_WINDOW_H_
#define INJECTOR_CORE_INCLUDE_WINDOW_H_
#include <optional>
#include <string>
#include <variant>

#include "core_types.h"
#include "events.h"
#include "timestamp.h"

namespace fxcore {

struct AtTime {
  nanoseconds time{};
};

struct AfterEvent {
  std::string event;
  nanoseconds after{};
};

struct ForDuration {
  nanoseconds duration{};
};

using WindowStart = std::variant<AtTime, AfterEvent>;
using WindowEnd = std::variant<AtTime, ForDuration>;

struct Window {
  // unset means start from the beginning of simulation
  std::optional<WindowStart> start;
  // unset means to the end of simulation
  std::optional<WindowEnd> end;

  /**
   * @brief If the given Timestamp is within this Window
   *
   * @param ts Enquired Timestamp
   * @param map Map of events and their timestamp
   * @return true if is within this Window and false otherwise.
   */
  bool contains(const Timestamp& ts, const EventTimes& map) const;

 private:
  // Helper for contains to resolve the start of current window
  std::optional<Timestamp> handle_start(const EventTimes& events) const;
};

}  // namespace fxcore

#endif  // INJECTOR_CORE_INCLUDE_WINDOW_H_
