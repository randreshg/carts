#ifndef LLVM_ARTS_METADATA_H
#define LLVM_ARTS_METADATA_H

#include <cstdint>

// #include "carts/analysis/ARTS.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"


/// ------------------------------------------------------------------- ///
///                            ARTS METADATA                            ///
/// ------------------------------------------------------------------- ///
namespace arts {
using namespace arts::types;

/// ------------------------------------------------------------------- ///
///                           EDT ARG METADATA                          ///
/// ------------------------------------------------------------------- ///
class EDTArgMetadata {
public:
  EDTArgMetadata(Value &V);
  ~EDTArgMetadata();

  static string getName(EDTArgType Ty);

  static bool classof(const EDTArgMetadata *M) { return true; }
};

class EDTDepArgMetadata : public EDTArgMetadata {
public:
  EDTDepArgMetadata(Value &V);
  ~EDTDepArgMetadata();

  static string getName();

  static bool classof(const EDTArgMetadata *M) { return true; }
};

class EDTParamArgMetadata : public EDTArgMetadata {
public:
  EDTParamArgMetadata(Value &V);
  ~EDTParamArgMetadata();

  static string getName();

  static bool classof(const EDTArgMetadata *M) { return true; }
};

/// ------------------------------------------------------------------- ///
///                             EDT METADATA                            ///
/// ------------------------------------------------------------------- ///
class EDTMetadata {
public:
  EDTMetadata(Function &Fn);
  ~EDTMetadata();

  static EDTMetadata *getEDT(Function &Fn);
  static string getName(EDTType Ty);
  static void setMetadata(Function &Fn, EDTIRBuilder &Builder);
  static bool classof(const EDTMetadata *M) { return true; }
  Function &getFunction() { return Fn; }

private:
  // MDNode *Node;
  Function &Fn;
  SmallVector<EDTArgMetadata, 4> Args;
};

class ParallelEDTMetadata : public EDTMetadata {
public:
  ParallelEDTMetadata(Function &Fn) : EDTMetadata(Fn) {}
  ~ParallelEDTMetadata() {}
  static string getName();
  // static bool classof(const EDTMetadata *M);

  uint32_t getNumberOfThreads() const { return 1; }
};

class TaskEDTMetadata : public EDTMetadata {
public:
  TaskEDTMetadata(Function &Fn) : EDTMetadata(Fn) {}
  ~TaskEDTMetadata() {}
  static string getName();
  // static bool classof(const EDTMetadata *M);

  uint32_t getThreadNumber() const { return 0; }
};

class MainEDTMetadata : public EDTMetadata {
public:
  MainEDTMetadata(Function &Fn) : EDTMetadata(Fn) {}
  ~MainEDTMetadata() {}
  static string getName();
  // static bool classof(const EDTMetadata *M);
};

} // namespace arts

#endif // LLVM_ARTS_METADATA_H