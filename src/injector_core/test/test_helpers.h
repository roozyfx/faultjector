#ifndef INJECTOR_CORE_TEST_TEST_HELPERS_H_
#define INJECTOR_CORE_TEST_TEST_HELPERS_H_

#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <vector>

#include "message.h"
#include "timestamp.h"

// Shared helpers for the Perturb and FaultPipe tests. Payloads are strings
// derived from the sequence number ("m<seq>"), so a delivery shows which
// message it carries.
namespace fxtest {

using Msg = fxcore::Message<std::string>;
using Em = fxcore::Emission<std::string>;

inline std::string payload_of(const uint64_t seq) {
  return std::format("m{}", seq);
}

inline Msg make_msg(const fxcore::Timestamp t, const uint64_t seq) {
  return {t, seq, payload_of(seq)};
}

// A comparable, printable view of an Emission or a Message.
struct Delivery {
  fxcore::Timestamp time;
  uint64_t sequence;
  std::string payload;
  bool operator==(const Delivery&) const = default;
};

inline void PrintTo(const Delivery& d, std::ostream* os) {
  *os << std::format("{{t={}, seq={}, payload={}}}", d.time.time_since_epoch(),
                     d.sequence, d.payload);
}

// The message with sequence @p seq, delivered at @p t.
inline Delivery delivery(const fxcore::Timestamp t, const uint64_t seq) {
  return {t, seq, payload_of(seq)};
}

inline Delivery view(const Em& e) {
  return {e.due_time, e.sequence, e.payload};
}

inline Delivery view(const Msg& m) {
  return {m.received_time, m.sequence, m.payload};
}

template <class Range>
std::vector<Delivery> views(const Range& range) {
  std::vector<Delivery> result;
  for (const auto& item : range) result.push_back(view(item));
  return result;
}

}  // namespace fxtest

#endif  // INJECTOR_CORE_TEST_TEST_HELPERS_H_
