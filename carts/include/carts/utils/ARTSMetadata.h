#ifndef LLVM_ARTS_METADATA_H
#define LLVM_ARTS_METADATA_H

#include <cstdint>

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instruction.h"
#include "carts/analysis/ARTS.h"

/// ------------------------------------------------------------------- ///
///                            ARTS METADATA                            ///
/// ------------------------------------------------------------------- ///
namespace arts {
namespace metadata {
using namespace arts::types;

/// ------------------------------------------------------------------- ///
///                           EDT ARG METADATA                          ///
/// ------------------------------------------------------------------- ///
class EDTArgMetadata {
public:
  EDTArgMetadata(Value &V);
  ~EDTArgMetadata();

  static StringRef getName(EDTArgType Ty);

  static bool classof(const EDTArgMetadata *M) { return true; }
};

class EDTDepArgMetadata: public EDTArgMetadata {
public:
  EDTDepArgMetadata(Value &V);
  ~EDTDepArgMetadata();

  static StringRef getName();

  static bool classof(const EDTArgMetadata *M) { return true; }
};

class EDTParamArgMetadata: public EDTArgMetadata {
public:
  EDTParamArgMetadata(Value &V);
  ~EDTParamArgMetadata();

  static StringRef getName();

  static bool classof(const EDTArgMetadata *M) { return true; }
};

/// ------------------------------------------------------------------- ///
///                             EDT METADATA                            ///
/// ------------------------------------------------------------------- ///
class ParallelEDTMetadata;
class TaskEDTMetadata;
class MainEDTMetadata;
class EDTMetadata {
public:
  EDTMetadata(Function &F);
  ~EDTMetadata();

  static EDTMetadata *getEDT(Function &F);
  static StringRef *getName(EDTType Ty);
  static void setMetadata(Function &F, EDTMetadata *M);

  static bool classof(const EDTMetadata *M) { return true; }

private:
  MDNode *Node;
  SmallVector<EDTArgMetadata, 4> Args;
};

class ParallelEDTMetadata : public EDTMetadata {
public:
  ParallelEDTMetadata(Function &F) : EDTMetadata(F) {}
  ~ParallelEDTMetadata() {}
  static StringRef getName();
  static bool classof(const EDTMetadata *M);

  uint32_t getNumberOfThreads() const { return 1; }
};

class TaskEDTMetadata : public EDTMetadata {
public:
  TaskEDTMetadata(Function &F) : EDTMetadata(F) {}
  ~TaskEDTMetadata() {}
  static StringRef getName();
  static bool classof(const EDTMetadata *M);

  uint32_t getThreadNumber() const { return 0; }
};

class MainEDTMetadata : public EDTMetadata {
public:
  MainEDTMetadata(Function &F) : EDTMetadata(F) {}
  ~MainEDTMetadata() {}
  static StringRef getName();
  static bool classof(const EDTMetadata *M);

};

} // namespace metadata
} // namespace arts

#endif // LLVM_ARTS_METADATA_H