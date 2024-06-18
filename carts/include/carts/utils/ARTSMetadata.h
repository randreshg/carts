#ifndef LLVM_ARTS_METADATA_H
#define LLVM_ARTS_METADATA_H

#include <cstdint>

#include "carts/utils/ARTSIRBuilder.h"
#include "carts/utils/ARTSTypes.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"

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

  /// Static functions
  static EDTMetadata *getEDT(Function &Fn);
  static string getName(EDTType Ty);
  static void setMetadata(Function &Fn, EDTIRBuilder &Builder);
  static bool classof(const EDTMetadata *M) { return true; }

  /// Getters
  Function &getFunction() { return Fn; }
  EDTType getTy() const { return Ty; }

private:
  Function &Fn;
  SmallVector<EDTArgMetadata, 4> Args;
protected:
  EDTType Ty = EDTType::Unknown;
};

class ParallelEDTMetadata : public EDTMetadata {
public:
  ParallelEDTMetadata(Function &Fn) : EDTMetadata(Fn) {
    Ty = EDTType::Parallel;
  }
  ~ParallelEDTMetadata() {}
  static string getName();
  static bool classof(const EDTMetadata *M) {
    return M->getTy() == EDTType::Parallel;
  }

  uint32_t getNumberOfThreads() const { return NumberOfThreads; }

private:
  uint32_t NumberOfThreads = 1;
};

class TaskEDTMetadata : public EDTMetadata {
public:
  TaskEDTMetadata(Function &Fn) : EDTMetadata(Fn) {}
  ~TaskEDTMetadata() {}
  static string getName();
  static bool classof(const EDTMetadata *M) {
    return M->getTy() == EDTType::Task;
  }

  uint32_t getThreadNumber() const { return ThreadNumber; }

private:
  uint32_t ThreadNumber = 0;
};

class MainEDTMetadata : public EDTMetadata {
public:
  MainEDTMetadata(Function &Fn) : EDTMetadata(Fn) {}
  ~MainEDTMetadata() {}
  static string getName();
  static bool classof(const EDTMetadata *M) {
    return M->getTy() == EDTType::Main;
  }
};

} // namespace arts

#endif // LLVM_ARTS_METADATA_H