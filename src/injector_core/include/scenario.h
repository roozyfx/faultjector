#ifndef INJECTOR_CORE_INCLUDE_SCENARIO_H_
#define INJECTOR_CORE_INCLUDE_SCENARIO_H_

// Sample scenario file:
/*
name: odom_lag_during_navigation
seed: 20260907
timeout: 120s

injections:
  - topic: /odom            # republished under this name
    source: /odom_raw       # subscribed here; default is <topic>_raw
    faults:
      - id: odom_lag        # names this fault in logs, reports and assertions
        type: delay
        params: { mean: 200ms, jitter: 20ms }
        window:
          start: { event: goal_accepted, after: 5s }
          end:   { duration: 15s }

record: [/odom, /scan, /tf, /cmd_vel, /plan]

assert:
  - goal_reached:          { within: 90s }
  - max_cross_track_error: { below: 0.30m, during: odom_lag }
  - max_cross_track_error: { below: 0.10m, after: odom_lag, settle: 5s }
*/
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "fault_parameters.h"

struct AtTime {
  Duration time{};
};

struct AfterEvent {
  std::string event;
  Duration after{};
};

struct ForDuration {
  Duration duration{};
};

using WindowStart = std::variant<AtTime, AfterEvent>;
using WindowEnd = std::variant<AtTime, ForDuration>;
struct Window {
  // unset means start from the beginning of simulation
  std::optional<WindowStart> start;
  // unset means to the end of simulation
  std::optional<WindowEnd> end;
};

struct Fault {
  std::string id;
  std::string type;
  FaultParams params;
  Window window;
};
using Faults = std::vector<Fault>;

struct Injection {
  std::string topic;
  std::string source;
  Faults faults;
};
using Injections = std::vector<Injection>;

using RecordList = std::vector<std::string>;

// TODO Check
/**
 * Assertions are evaluated offline against the recorded bag, so their
 * arguments are kept as written (`below: 0.30m` stays "0.30m"). The parser
 * only checks that `during` and `after` name a declared fault.
 */
struct Assertion {
  std::string name;
  std::map<std::string, std::string> args;
};

struct Scenario {
  std::string name{};
  uint64_t seed{};
  Duration timeout{};
  Injections injections{};
  RecordList record{};
  // FIXME uncomment
  //  Assertion assert{};
};

#endif  // INJECTOR_CORE_INCLUDE_SCENARIO_H_
