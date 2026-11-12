#ifndef INJECTOR_CORE_INCLUDE_TIMESTAMP_H_
#define INJECTOR_CORE_INCLUDE_TIMESTAMP_H_
#include "core_types.h"

class Timestamp {
  nanoseconds val_;

 public:
  explicit Timestamp()
      : val_(0) {}
  explicit Timestamp(const nanoseconds& ns)
      : val_{ns} {}

  nanoseconds value() const { return val_; }
  inline Timestamp operator+(const Timestamp& ts) const {
    return Timestamp(val_ + ts.val_);
  }

  inline bool operator<(const Timestamp& t1) { return val_ < t1.value(); }
};

#endif  // INJECTOR_CORE_INCLUDE_TIMESTAMP_H_
