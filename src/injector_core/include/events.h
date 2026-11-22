#ifndef INJECTOR_CORE_INCLUDE_EVENTS_H_
#define INJECTOR_CORE_INCLUDE_EVENTS_H_

#include <map>
#include <string>

#include "timestamp.h"

namespace fxcore {
using EventTimes = std::map<std::string, Timestamp>;
}

#endif  // INJECTOR_CORE_INCLUDE_EVENTS_H_
