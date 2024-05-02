//===- ARTSConstants.h - OpenMP related constants and helpers ------ C++ -*-===//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file defines constants and helpers used when dealing with OpenMP.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "noelle/core/Noelle.hpp"

// #include "llvm/ADT/StringRef.h"

namespace arts {

class EDT {
public:
  enum class Type {
    MAIN,
    TASK,
    PARALLEL,
    WRAPPER,
    OTHER,
  };

  EDT(Type Ty, uint64_t ID) : Ty(Ty), ID(ID){};
  EDT(Type Ty, uint64_t ID, Function *F) : Ty(Ty), ID(ID), F(F){};

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

#define ARTS_RTL(Enum, ...) constexpr auto Enum = arts::types::RuntimeFunction::Enum;
#include "ARTSKinds.def"

} // end namespace types


} // end namespace arts

#endif // LLVM_ARTS_H
