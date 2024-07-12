#ifndef LLVM_ARTS_METADATA_H
#define LLVM_ARTS_METADATA_H

#include <cstdint>

#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "carts/utils/ARTSTypes.h"
#include "llvm/ADT/FloatingPointMode.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"

/// ------------------------------------------------------------------- ///
///                            ARTS METADATA                            ///
/// ------------------------------------------------------------------- ///
namespace arts {
using namespace arts::types;

#define CARTS_MD "carts.metadata"
#define EDT_MD "edt."
#define EDT_ARG_MD "edt.arg."

/// ------------------------------------------------------------------- ///
///                           EDT ARG METADATA                          ///
/// ------------------------------------------------------------------- ///
class EDTArgMetadata {
public:
  EDTArgMetadata(EDTArgType Ty, EDTValue *Val);
  ~EDTArgMetadata();

  static string getName(EDTArgType Ty);
  static bool classof(const EDTArgMetadata *M) { return true; }
  EDTArgType getTy() const { return Ty; }
  EDTValue *getVal() const { return Val; }

protected:
  EDTArgType Ty = EDTArgType::Unknown;
  EDTValue *Val;
};

/// ------------------------------------------------------------------- ///
///                             EDT METADATA                            ///
/// ------------------------------------------------------------------- ///
class EDTMetadata {
public:
  EDTMetadata(EDTType Ty, EDTFunction *Fn);
  ~EDTMetadata();

  /// Static interface
  static string getName(EDTType Ty);
  // static EDTMetadata *getMetadata(EDTFunction *Fn);
  // static void setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder);

  /// Functions
  // void insertArg(EDTArgMetadata *Arg) { Args.push_back(Arg); }

  /// Getters
  // EDTFunction *getFn() { return Fn; }
  // EDTType getTy() const { return Ty; }
  // Twine getName() { return Fn->getName(); }

// protected:
//   EDTType Ty = EDTType::Unknown;
//   EDTFunction *Fn = nullptr;
//   SmallVector<EDTArgMetadata *, 4> Args;
};

} // namespace arts

#endif // LLVM_ARTS_METADATA_H