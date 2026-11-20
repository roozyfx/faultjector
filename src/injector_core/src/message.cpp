#include "message.h"

bool operator<(const Emission& e1, const Emission& e2) {
  return e1.due_time.value() < e2.due_time.value();
}

Message to_message(const Emission& e) { return Message{e.due_time, e.message}; }
Emission to_emission(const Message& m) {
  return Emission{m.sensor_time, m.message};
}
