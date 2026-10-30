#ifndef INJECTOR_CORE_INCLUDE_SCENARIO_PARSER_H_
#define INJECTOR_CORE_INCLUDE_SCENARIO_PARSER_H_

#include <expected>
#include <string_view>

#include "scenario.h"

std::expected<Scenario, std::string> parse_scenario(
    std::string_view scenario_file);

#endif  // INJECTOR_CORE_INCLUDE_SCENARIO_PARSER_H_
