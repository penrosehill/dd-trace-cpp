#define CATCH_CONFIG_CPP17_UNCAUGHT_EXCEPTIONS
#define CATCH_CONFIG_CPP17_STRING_VIEW
#define CATCH_CONFIG_CPP17_VARIANT
#define CATCH_CONFIG_CPP17_OPTIONAL
#define CATCH_CONFIG_CPP17_BYTE

#include "catch.hpp"

#include <iosfwd>
#include <string>
#include <utility>

namespace std {

std::ostream& operator<<(std::ostream& stream, const std::pair<const std::string, std::string>& item);

}  // namspace std