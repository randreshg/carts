///==========================================================================
/// File: ArtsTypes.h
///==========================================================================

#ifndef CARTS_UTILS_ARTSTYPES_H
#define CARTS_UTILS_ARTSTYPES_H

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/InstrTypes.h"
#include <sys/types.h>

#include "arts/ArtsDialect.h"

namespace mlir {
namespace arts {

// Forward declarations
class DbAllocOp;

namespace types {

/// IDs for all arts runtime library (RTL) functions.
enum class RuntimeFunction {
#define ARTS_RTL(Enum, ...) Enum,
#include "arts/Codegen/ARTSKinds.def"
};

#define ARTS_RTL(Enum, ...)                                                    \
  constexpr auto Enum = arts::types::RuntimeFunction::Enum;
#include "arts/Codegen/ARTSKinds.def"

} // end namespace types
} // end namespace arts
} // end namespace mlir

/// Specialization of DenseMapInfo for RuntimeFunction enum
namespace llvm {
template <> struct DenseMapInfo<mlir::arts::types::RuntimeFunction> {
  static mlir::arts::types::RuntimeFunction getEmptyKey() {
    return static_cast<mlir::arts::types::RuntimeFunction>(~0U);
  }

  static mlir::arts::types::RuntimeFunction getTombstoneKey() {
    return static_cast<mlir::arts::types::RuntimeFunction>(~0U - 1);
  }

  static unsigned getHashValue(mlir::arts::types::RuntimeFunction val) {
    return static_cast<unsigned>(val);
  }

  static bool isEqual(mlir::arts::types::RuntimeFunction lhs,
                      mlir::arts::types::RuntimeFunction rhs) {
    return lhs == rhs;
  }
};

} // namespace llvm

#endif // CARTS_UTILS_ARTSTYPES_H