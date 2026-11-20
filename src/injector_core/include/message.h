#ifndef INJECTOR_CORE_INCLUDE_MESSAGE_H
#define INJECTOR_CORE_INCLUDE_MESSAGE_H

#include "timestamp.h"

// FIXME
#include <string>
struct Payload {
  std::string temp_str;
};

struct Emission {
  Timestamp due_time{};
  Payload message{};
};

bool operator<(const Emission& e1, const Emission& e2);

struct Message {
  Timestamp sensor_time{};
  Payload message{};
};

Message to_message(const Emission& e);
Emission to_emission(const Message& m);
#endif  // INJECTOR_CORE_INCLUDE_MESSAGE_H
