#pragma once

#include <variant>

#include "expected.h"

namespace datadog {
namespace tracing {

class Rate {
  double value_;
  explicit Rate(double value) : value_(value) {}

 public:
  Rate() : value_(0.0) {}

  double value() const { return value_; }
  operator double() const { return value(); }

  static Rate one() { return Rate(1.0); }
  static Rate zero() { return Rate(0.0); }

  static Expected<Rate> from(double);
};

}  // namespace tracing
}  // namespace datadog
