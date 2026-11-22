#include "message.h"

bool fxcore::operator<(const Emission& e1, const Emission& e2) {
  return e1.due_time < e2.due_time;
}

fxcore::Message fxcore::to_message(const Emission& e) {
  return Message{e.due_time, e.sequence, e.message};
}

fxcore::Emission fxcore::to_emission(const Message& m) {
  return Emission{m.received_time, m.sequence, m.message};
}
