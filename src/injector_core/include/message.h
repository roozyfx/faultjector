#ifndef INJECTOR_CORE_INCLUDE_MESSAGE_H
#define INJECTOR_CORE_INCLUDE_MESSAGE_H

#include <concepts>

#include "timestamp.h"

namespace fxcore {
template <class P>
concept Payload = std::copyable<P>;

template <Payload P>
struct Emission {
  Timestamp due_time{};
  uint64_t sequence{};
  P payload{};
};

template <Payload P>
bool operator<(const Emission<P>& e1, const Emission<P>& e2) {
  return e1.due_time < e2.due_time;
}

template <Payload P>
struct Message {
  Timestamp received_time{};
  uint64_t sequence{};
  P payload{};
};

template <Payload P>
Message<P> to_message(const Emission<P>& e) {
  return Message{e.due_time, e.sequence, e.payload};
}

template <Payload P>
Emission<P> to_emission(const Message<P>& m) {
  return Emission{m.received_time, m.sequence, m.payload};
}

}  // namespace fxcore
#endif  // INJECTOR_CORE_INCLUDE_MESSAGE_H
