///==========================================================================///
/// File: ArtsId.h
///
/// Defines the ArtsId type - a semantic wrapper around std::optional<int64_t>
/// for ARTS identifiers. Provides a cleaner API and room for future extensions.
///==========================================================================///

#ifndef ARTS_UTILS_ARTSID_H
#define ARTS_UTILS_ARTSID_H

#include <cstdint>
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// ArtsId - Semantic type for ARTS identifiers
///
/// Wraps std::optional<int64_t> with a cleaner API tailored for ID management.
/// Benefits over raw std::optional<int64_t>:
/// - More semantic type name (ArtsId vs std::optional<int64_t>)
/// - Convenient set/clear methods
/// - Extensible for future ARTS-specific ID logic
/// - Type safety: prevents mixing with other optional integers
///===----------------------------------------------------------------------===///
class ArtsId {
public:
  /// Default constructor - creates an unset ID
  ArtsId() = default;

  /// Construct from an ID value
  explicit ArtsId(int64_t val) : value_(val) {}

  /// Set the ID to a specific value
  void set(int64_t val) { value_ = val; }

  /// Clear the ID (mark as unset)
  void clear() { value_.reset(); }

  /// Check if the ID has a value
  bool has_value() const { return value_.has_value(); }

  /// Get the ID value (throws if not set)
  int64_t value() const { return value_.value(); }

  /// Get the ID value with a default fallback
  int64_t value_or(int64_t defaultVal) const {
    return value_.value_or(defaultVal);
  }

  /// Get direct access to underlying optional (for compatibility)
  const std::optional<int64_t> &optional() const { return value_; }
  std::optional<int64_t> &optional() { return value_; }

  /// Conversion to bool (same as has_value())
  explicit operator bool() const { return has_value(); }

  /// Comparison operators
  bool operator==(const ArtsId &other) const { return value_ == other.value_; }
  bool operator!=(const ArtsId &other) const { return value_ != other.value_; }
  bool operator==(int64_t val) const { return value_ == val; }
  bool operator!=(int64_t val) const { return value_ != val; }

  /// Allow comparison with std::nullopt
  bool operator==(std::nullopt_t) const { return !has_value(); }
  bool operator!=(std::nullopt_t) const { return has_value(); }

private:
  std::optional<int64_t> value_;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_UTILS_ARTSID_H
