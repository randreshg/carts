///==========================================================================
/// File: ArtsTypes.h
///==========================================================================

#ifndef CARTS_UTILS_ARTSTYPES_H
#define CARTS_UTILS_ARTSTYPES_H

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/InstrTypes.h"
#include <sys/types.h>

namespace mlir {
namespace arts {
namespace types {
/// EDT types
enum class EdtType { Parallel, Single, Sync, Task };
enum class DbAccessType { ReadOnly, WriteOnly, ReadWrite, Unknown };

inline llvm::StringRef toString(const EdtType Ty) {
  switch (Ty) {
  case EdtType::Parallel:
    return "parallel";
  case EdtType::Single:
    return "single";
  case EdtType::Sync:
    return "sync";
  case EdtType::Task:
    return "task";
  }
}

inline llvm::StringRef toString(const DbAccessType Ty) {
  switch (Ty) {
  case DbAccessType::ReadOnly:
    return "in";
  case DbAccessType::WriteOnly:
    return "inout";
  case DbAccessType::ReadWrite:
    return "inout";
  case DbAccessType::Unknown:
    return "unknown";
  }
}

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