#ifndef INJECTOR_CORE_INCLUDE_WINDOW_H_
#define INJECTOR_CORE_INCLUDE_WINDOW_H_
#include <optional>
#include <string>
#include <variant>

#include "core_types.h"

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
};

#endif  // INJECTOR_CORE_INCLUDE_WINDOW_H_
