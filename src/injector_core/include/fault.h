#ifndef INJECTOR_CORE_INCLUDE_FAULT_H_
#define INJECTOR_CORE_INCLUDE_FAULT_H_

#include <string>
#include <vector>

#include "fault_params.h"
#include "window.h"

struct Fault {
  std::string id;
  std::string type;
  FaultParams params;
  Window window;
};
using Faults = std::vector<Fault>;

#endif  // INJECTOR_CORE_INCLUDE_FAULT_H_
