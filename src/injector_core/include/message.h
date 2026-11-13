#ifndef INJECTOR_CORE_INCLUDE_MESSAGE_H
#define INJECTOR_CORE_INCLUDE_MESSAGE_H

#include "timestamp.h"

// FIXME
struct Payload {};

struct Emission {
  Timestamp due_time{};
  Payload message{};

  inline bool operator<(const Emission& e) { return due_time < e.due_time; }
};

struct Message {
  Timestamp sensor_time{};
  Payload message{};
};
#endif  // INJECTOR_CORE_INCLUDE_MESSAGE_H
