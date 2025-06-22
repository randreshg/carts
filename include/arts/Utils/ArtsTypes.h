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

// Forward declarations
class DbAllocOp;

namespace types {
/// EDT types
enum class EdtType { Parallel, Single, Sync, Task };
enum class DbDepType { ReadOnly, WriteOnly, ReadWrite, Unknown };
enum class DbAllocType { Stack, Heap, Global, Parameter, Dynamic, Unknown };

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

inline llvm::StringRef toString(const DbDepType Ty) {
  switch (Ty) {
  case DbDepType::ReadOnly:
    return "in";
  case DbDepType::WriteOnly:
    return "inout";
  case DbDepType::ReadWrite:
    return "inout";
  case DbDepType::Unknown:
    return "unknown";
  }
}

inline llvm::StringRef toString(const DbAllocType Ty) {
  switch (Ty) {
  case DbAllocType::Stack:
    return "stack";
  case DbAllocType::Heap:
    return "heap";
  case DbAllocType::Global:
    return "global";
  case DbAllocType::Parameter:
    return "parameter";
  case DbAllocType::Dynamic:
    return "dynamic";
  case DbAllocType::Unknown:
    return "unknown";
  }
}

inline DbAllocType getDbAllocType(llvm::StringRef str) {
  if (str == "stack")
    return DbAllocType::Stack;
  if (str == "heap")
    return DbAllocType::Heap;
  if (str == "global")
    return DbAllocType::Global;
  if (str == "parameter")
    return DbAllocType::Parameter;
  if (str == "dynamic")
    return DbAllocType::Dynamic;
  return DbAllocType::Unknown;
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

/// Specialization of DenseMapInfo for DbAllocType enum
template <> struct DenseMapInfo<mlir::arts::types::DbAllocType> {
  static mlir::arts::types::DbAllocType getEmptyKey() {
    return static_cast<mlir::arts::types::DbAllocType>(~0U);
  }

  static mlir::arts::types::DbAllocType getTombstoneKey() {
    return static_cast<mlir::arts::types::DbAllocType>(~0U - 1);
  }

  static unsigned getHashValue(mlir::arts::types::DbAllocType val) {
    return static_cast<unsigned>(val);
  }

  static bool isEqual(mlir::arts::types::DbAllocType lhs,
                      mlir::arts::types::DbAllocType rhs) {
    return lhs == rhs;
  }
};
} // namespace llvm

#endif // CARTS_UTILS_ARTSTYPES_H