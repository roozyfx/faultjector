#ifndef INJECTOR_CORE_INCLUDE_SCENARIO_FORMATTER_H_
#define INJECTOR_CORE_INCLUDE_SCENARIO_FORMATTER_H_
#include <format>

#include "scenario.h"
#include "timestamp.h"

/* Formatter for Scenario type, so that std::print/ln can show the output easily
and cleanly.
TODO: Since it is only needed for test purposes, is still (And probably stays!)
in an incomplete state, until necessity asks for it! With the same reason, it
can be removed from the project.
*/

template <>
struct std::formatter<fxcore::Scenario> : public std::formatter<std::string> {
  auto format(const fxcore::Scenario& scenario,
              std::format_context& ctx) const {
    auto out{ctx.out()};
    std::format_to(out, "name: {}\n", scenario.name);
    std::format_to(out, "seed: {}\n", scenario.seed);
    std::format_to(out, "timeout: {}\n", scenario.timeout);
    std::format_to(out, "Injections: [\n");
    for (const auto& injection : scenario.injections) {
      std::format_to(out, "\ttopic: {}\n", injection.topic);
      std::format_to(out, "\tsource: {}\n", injection.source);
      std::format_to(out, "\tfaults: [\n");
      for (auto const& fault : injection.faults) {
        std::format_to(out, "\tid: {}\n", fault.id);
        std::format_to(out, "\ttype: {}\n", fault.type);
        std::format_to(out, "\tparams: // TODO\n");
        std::format_to(out, "\twindow: \n");
        if (fault.window.start.has_value()) {
          std::format_to(out, "\t\tstart: [");
          auto start = fault.window.start.value();
          if (const auto* at_time = std::get_if<fxcore::AtTime>(&start))
            std::format_to(out, "at: {}{}", (*at_time).time.time_since_epoch(),
                           "]\n");
          if (const fxcore::AfterEvent* after_event =
                  std::get_if<fxcore::AfterEvent>(&start))
            std::format_to(out, "event: {}, after: {}{}", (*after_event).event,
                           (*after_event).after.count(), "]\n");
        }
        if (fault.window.end.has_value()) {
          std::format_to(out, "\t\tend: [");
          auto end = fault.window.end.value();
          if (const auto* at_time = std::get_if<fxcore::AtTime>(&end))
            std::format_to(out, "at: {}{}", (*at_time).time.time_since_epoch(),
                           "]\n");
          if (const fxcore::ForDuration* for_duration =
                  std::get_if<fxcore::ForDuration>(&end))
            std::format_to(out, "duration: {}{}",
                           (*for_duration).duration.count(), "]\n");
        }
      }
    }
    std::format_to(out, "\n");

    std::format_to(out, "record: [");
    for (const auto& entry : scenario.record) {
      std::format_to(out, "{}, ", entry);
    }
    std::format_to(out, "]");

    return out;
  }
};

#endif  // INJECTOR_CORE_INCLUDE_SCENARIO_FORMATTER_H_
