#ifndef INJECTOR_CORE_INCLUDE_SCENARIO_H_
#define INJECTOR_CORE_INCLUDE_SCENARIO_H_
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "injection.h"

namespace fxcore {

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
  nanoseconds timeout{};
  Injections injections{};
  RecordList record{};
  // FIXME uncomment
  //  Assertion assert{};
};
}  // namespace fxcore
#endif  // INJECTOR_CORE_INCLUDE_SCENARIO_H_
