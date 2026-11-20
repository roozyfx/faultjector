#ifndef INJECTOR_CORE_INCLUDE_FAULT_H_
#define INJECTOR_CORE_INCLUDE_FAULT_H_

#include <string>
#include <vector>

#include "fault_params.h"
#include "window.h"

namespace fxcore {

struct Fault {
  std::string id;
  std::string type;
  FaultParams params;
  Window window;
};  // struct Fault
using Faults = std::vector<Fault>;
}  // namespace fxcore
#endif  // INJECTOR_CORE_INCLUDE_FAULT_H_
