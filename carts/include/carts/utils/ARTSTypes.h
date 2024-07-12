//===- ARTSTypes.h - ARTS-Related types -------------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_TYPES_H
#define LLVM_ARTS_TYPES_H

#include "llvm/IR/Instruction.h"
#include <sys/types.h>

namespace arts {
using namespace llvm;
using namespace std;
using BlockSequence = SmallVector<BasicBlock *, 0>;

/// ------------------------------------------------------------------- ///
///                            ART TYPES                                ///
/// It contains the types used by the ARTS runtime library.
/// ------------------------------------------------------------------- ///
namespace types {
/// EDT types
enum EDTTypeKind {
  TK_UNKNOWN = 0,
  TK_TASK = (1 << 0),               // "0001"
  TK_SYNC = (1 << 1) | TK_TASK,     // "0011"
  TK_MAIN = (1 << 2) | TK_TASK,     // "0101"
  TK_PARALLEL = (1 << 3) | TK_SYNC, // "1011"

};

enum class EDTType { Task, Main, Sync, Parallel, Unknown };
enum class EDTArgType { Param, Dep, Unknown };
const Twine toString(const EDTType Ty);
const Twine toString(const EDTArgType Ty);
EDTType toEDTType(const StringRef Str);
EDTArgType toEDTArgType(const StringRef Str);
/// IDs for all arts runtime library (RTL) functions.
enum class RuntimeFunction {
#define ARTS_RTL(Enum, ...) Enum,
#include "carts/codegen/ARTSKinds.def"
};

#define ARTS_RTL(Enum, ...)                                                    \
  constexpr auto Enum = arts::types::RuntimeFunction::Enum;
#include "carts/codegen/ARTSKinds.def"

} // end namespace types
} // end namespace arts
#endif // LLVM_ARTS_TYPES_H