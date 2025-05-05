///==========================================================================
/// File: ArtsTypes.h
///==========================================================================

#ifndef CARTS_UTILS_ARTSTYPES_H
#define CARTS_UTILS_ARTSTYPES_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/InstrTypes.h"
#include <sys/types.h>

namespace mlir {
namespace arts {
namespace types {
/// EDT types
enum class EdtType { Parallel, Single, Sync, Task };
enum class DatablockAccessType { ReadOnly, WriteOnly, ReadWrite, Unknown };

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

inline llvm::StringRef toString(const DatablockAccessType Ty) {
  switch (Ty) {
  case DatablockAccessType::ReadOnly:
    return "in";
  case DatablockAccessType::WriteOnly:
    return "inout";
  case DatablockAccessType::ReadWrite:
    return "inout";
  case DatablockAccessType::Unknown:
    return "unknown";
  }
}
// const llvm::Twine toString(const EDTType Ty);
// const llvm::Twine toString(const EDTArgType Ty);
// EDTType toEDTType(const llvm::StringRef Str);
// EDTArgType toEDTArgType(const llvm::StringRef Str);

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
#endif // CARTS_UTILS_ARTSTYPES_H