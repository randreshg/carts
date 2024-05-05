//===- ARTSConstants.h - OpenMP related constants and helpers ------ C++
//-*-===//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file defines constants and helpers used when dealing with OpenMP.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "noelle/core/Noelle.hpp"
#include "noelle/core/Task.hpp"


namespace arts {
using BlockSequence = SmallVector<BasicBlock *, 0>;

class EDT : public arcana::noelle::Task {
public:
  enum class Type {
    MAIN,
    TASK,
    LOOP,
    PARALLEL,
    WRAPPER,
    OTHER,
  };

  EDT(Type Ty, FunctionType *TaskSignature, Module &M)
      : arcana::noelle::Task(TaskSignature, M), Ty(Ty) {}

  void setF(Function *F) { this->F = F; }
  Type getType() { return Ty; }

  Type Ty = Type::OTHER;
  uint64_t ID;
  Function *F = nullptr;
  bool Transformed = false;
  SetVector<EDT *> Preds;
  // DenseMap<EDT *, EdtDep *> Succs;
};


/// ----------------------------------------------------- ///
namespace types {
/// IDs for all arts runtime library (RTL) functions.
enum class RuntimeFunction {
#define ARTS_RTL(Enum, ...) Enum,
#include "ARTSKinds.def"
};

#define ARTS_RTL(Enum, ...)                                                    \
  constexpr auto Enum = arts::types::RuntimeFunction::Enum;
#include "ARTSKinds.def"

} // end namespace types

} // end namespace arts

#endif // LLVM_ARTS_H
