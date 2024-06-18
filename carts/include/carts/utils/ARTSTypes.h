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
enum class EDTType { Parallel, Task, Main, Unknown };
enum class EDTArgType { Param, Dep, Unknown };
const Twine toString(EDTType Ty);
const Twine toString(EDTArgType Ty);
EDTType toEDTType(StringRef Str);
EDTArgType toEDTArgType(StringRef Str);
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