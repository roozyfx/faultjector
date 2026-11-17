#ifndef INJECTOR_CORE_INCLUDE_INJECTION_H_
#define INJECTOR_CORE_INCLUDE_INJECTION_H_
#include <string>
#include <vector>

#include "fault.h"

struct Injection {
  std::string topic;
  std::string source;
  Faults faults;
};
using Injections = std::vector<Injection>;

#endif  // INJECTOR_CORE_INCLUDE_INJECTION_H_
