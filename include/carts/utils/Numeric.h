///==========================================================================///
/// File: Numeric.h
///
/// Dialect-neutral saturating integer helpers for layout and sizing math.
///==========================================================================///

#ifndef CARTS_UTILS_NUMERIC_H
#define CARTS_UTILS_NUMERIC_H

#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace mlir {
namespace carts {

inline int64_t ceilDivPositive(int64_t a, int64_t b) {
  return llvm::divideCeil(std::max<int64_t>(1, a),
                          std::max<int64_t>(1, b));
}

inline int64_t saturatingAddPositive(int64_t lhs, int64_t rhs) {
  lhs = std::max<int64_t>(0, lhs);
  rhs = std::max<int64_t>(0, rhs);
  if (lhs > std::numeric_limits<int64_t>::max() - rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs + rhs;
}

inline int64_t saturatingMulPositive(int64_t lhs, int64_t rhs) {
  if (lhs <= 0 || rhs <= 0)
    return 0;
  if (lhs > std::numeric_limits<int64_t>::max() / rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs * rhs;
}

} // namespace carts
} // namespace mlir

#endif // CARTS_UTILS_NUMERIC_H
