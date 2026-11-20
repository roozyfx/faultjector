#ifndef INJECTOR_CORE_INCLUDE_CORE_TYPES_H_
#define INJECTOR_CORE_INCLUDE_CORE_TYPES_H_

#include <chrono>
#include <cstdint>

namespace fxcore {
using SequenceNumber = uint64_t;

using nanoseconds = std::chrono::nanoseconds;
}  // namespace fxcore
#endif  // INJECTOR_CORE_INCLUDE_CORE_TYPES_H_
