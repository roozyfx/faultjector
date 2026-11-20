#ifndef INJECTOR_CORE_INCLUDE_INJECTION_H_
#define INJECTOR_CORE_INCLUDE_INJECTION_H_
#include <string>
#include <vector>

#include "fault.h"

namespace fxcore {
struct Injection {
  std::string topic;
  std::string source;
  Faults faults;
};  // struct Injection
using Injections = std::vector<Injection>;
}  // namespace fxcore
#endif  // INJECTOR_CORE_INCLUDE_INJECTION_H_
