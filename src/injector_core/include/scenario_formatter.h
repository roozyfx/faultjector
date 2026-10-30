#ifndef INJECTOR_CORE_INCLUDE_SCENARIO_FORMATTER_H_
#define INJECTOR_CORE_INCLUDE_SCENARIO_FORMATTER_H_
#include <format>

#include "scenario.h"

/* Formatter for Scenario type, so that std::print/ln can show the output easily
and cleanly.
TODO: Since it is only needed for test purposes, is still (And
probably stays!) in an incomplete state, until necessity asks for it! With the
same reason, it can be removed from the project.
*/
template <>
struct std::formatter<Scenario> : public std::formatter<std::string> {
  auto format(const Scenario& scenario, std::format_context& ctx) const {
    auto out{ctx.out()};
    std::format_to(ctx.out(), "name: {}\n", scenario.name);
    std::format_to(ctx.out(), "seed: {}\n", scenario.seed);
    std::format_to(ctx.out(), "timeout: {}\n", scenario.timeout);
    std::format_to(ctx.out(), "Injections: [\n");
    for (const auto& injection : scenario.injections) {
      std::format_to(ctx.out(), "\ttopic: {}\n", injection.topic);
      std::format_to(ctx.out(), "\tsource: {}\n", injection.source);
      std::format_to(ctx.out(), "\tfaults: [\n");
      for (auto const& fault : injection.faults) {
        std::format_to(ctx.out(), "\tid: {}\n", fault.id);
        std::format_to(ctx.out(), "\ttype: {}\n", fault.type);
        std::format_to(ctx.out(), "\tparams: // TODO\n");
        std::format_to(ctx.out(), "\twindow: \n");
        if (fault.window.start.has_value()) {
          std::format_to(ctx.out(), "\t\tstart: [");
          auto start = fault.window.start.value();
          if (const auto* at_time = std::get_if<AtTime>(&start))
            std::format_to(ctx.out(), "at: {}{}", (*at_time).time, "]\n");
          if (const AfterEvent* after_event = std::get_if<AfterEvent>(&start))
            std::format_to(ctx.out(), "event: {}, after: {}{}",
                           (*after_event).event, (*after_event).after, "]\n");
        }
        if (fault.window.end.has_value()) {
          std::format_to(ctx.out(), "\t\tend: [");
          auto end = fault.window.end.value();
          if (const auto* at_time = std::get_if<AtTime>(&end))
            std::format_to(ctx.out(), "at: {}{}", (*at_time).time, "]\n");
          if (const ForDuration* for_duration = std::get_if<ForDuration>(&end))
            std::format_to(ctx.out(), "duration: {}{}",
                           (*for_duration).duration, "]\n");
        }
      }
    }
    std::format_to(ctx.out(), "\n");

    std::format_to(ctx.out(), "record: [");
    for (const auto& entry : scenario.record) {
      std::format_to(ctx.out(), "{}, ", entry);
    }
    std::format_to(ctx.out(), "]");

    return ctx.out();
  }
};
#endif  // INJECTOR_CORE_INCLUDE_SCENARIO_FORMATTER_H_
