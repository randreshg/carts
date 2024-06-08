#ifndef LLVM_ARTS_METADATA_H
#define LLVM_ARTS_METADATA_H

#include <cstdint>

#include "llvm/IR/Instruction.h"

#include "carts/analysis/ARTS.h"

/// ------------------------------------------------------------------- ///
///                            ARTS METADATA                            ///
/// ------------------------------------------------------------------- ///
namespace arts {
namespace metadata {

class EDTMetadata {
public:
  EDTMetadata(Function &F);
  ~EDTMetadata();

private:
  MDNode *Node;
};

class ParallelEDTMetadata : public EDTMetadata {
public:
  ParallelEDTMetadata(Function &F) : EDTMetadata(F) {}
  ~ParallelEDTMetadata() {}

  uint32_t getNumberOfThreads() const;
  void setNumberOfThreads(uint32_t NumThreads);
};

class TaskEDTMetadata : public EDTMetadata {
public:
  TaskEDTMetadata(Function &F) : EDTMetadata(F) {}
  ~TaskEDTMetadata() {}

  // uint32_t getNumberOfThreads() const;
};

class MainEDTMetadata : public EDTMetadata {
public:
  MainEDTMetadata(Function &F) : EDTMetadata(F) {}
  ~MainEDTMetadata() {}

  // uint32_t getNumberOfThreads() const;
};
} // namespace metadata
} // namespace arts

#endif // LLVM_ARTS_METADATA_H