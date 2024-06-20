#ifndef LLVM_ARTS_METADATA_H
#define LLVM_ARTS_METADATA_H

#include <cstdint>

#include "carts/utils/ARTSIRBuilder.h"
#include "carts/utils/ARTSTypes.h"
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
  EDTArgMetadata(Value *V);
  ~EDTArgMetadata();

  static string getName(EDTArgType Ty);
  static bool classof(const EDTArgMetadata *M) { return true; }
  EDTArgType getTy() const { return Ty; }
  Value *getV() const { return V; }

protected:
  EDTArgType Ty = EDTArgType::Unknown;
private:
  Value *V;
};

class EDTDepArgMetadata : public EDTArgMetadata {
public:
  EDTDepArgMetadata(Value *V);
  ~EDTDepArgMetadata();
  static string getName();
  static bool classof(const EDTArgMetadata *M) {
    return M->getTy() == EDTArgType::Dep;
  }

};

class EDTParamArgMetadata : public EDTArgMetadata {
public:
  EDTParamArgMetadata(Value *V);
  ~EDTParamArgMetadata();

  static string getName();

  static bool classof(const EDTArgMetadata *M) {
    return M->getTy() == EDTArgType::Param;
  }
};

/// ------------------------------------------------------------------- ///
///                             EDT METADATA                            ///
/// ------------------------------------------------------------------- ///
class EDTMetadata {
public:
  EDTMetadata(Function &Fn);
  ~EDTMetadata();

  /// Static functions
  static string getName(EDTType Ty);
  static EDTMetadata *getMetadata(Function &Fn);
  static void setMetadata(Function &Fn, EDTIRBuilder &Builder);
  static bool classof(const EDTMetadata *M) { return true; }
  void insertArg(EDTArgMetadata *Arg) { Args.push_back(Arg); }

  /// Getters
  Function &getFn() { return Fn; }
  EDTType getTy() const { return Ty; }
  /// Friends
  friend class EDT;

private:
  Function &Fn;
  SmallVector<EDTArgMetadata *, 4> Args;

protected:
  EDTType Ty = EDTType::Unknown;
};

class ParallelEDTMetadata : public EDTMetadata {
public:
  ParallelEDTMetadata(Function &Fn);
  ~ParallelEDTMetadata() {}
  static string getName();
  static bool classof(const EDTMetadata *M) {
    return M->getTy() == EDTType::Parallel;
  }

  uint32_t getNumThreads() const { return NumThreads; }

  friend class ParallelEDT;

private:
  uint32_t NumThreads = 0;
  uint32_t NumTasks = 0;
};

class TaskEDTMetadata : public EDTMetadata {
public:
  TaskEDTMetadata(Function &Fn);
  ~TaskEDTMetadata() {}
  static string getName();
  static bool classof(const EDTMetadata *M) {
    return M->getTy() == EDTType::Task;
  }

  uint32_t getThreadNum() const { return ThreadNum; }

  friend class TaskEDT;

private:
  uint32_t ThreadNum = 0;
};

class MainEDTMetadata : public EDTMetadata {
public:
  MainEDTMetadata(Function &Fn);
  ~MainEDTMetadata() {}
  static string getName();
  static bool classof(const EDTMetadata *M) {
    return M->getTy() == EDTType::Main;
  }

  friend class MainEDT;
};

} // namespace arts

#endif // LLVM_ARTS_METADATA_H