//===- ARTSEdge.h - ARTSEdge-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTSEDGE_H
#define LLVM_ARTSEDGE_H

#include "carts/utils/ARTS.h"
#include "llvm/ADT/SetVector.h"
#include <sys/types.h>

/// ------------------------------------------------------------------- ///
///                             EDT EDGE                               ///
/// The data structure used to represent dependencies between EDTs.
/// ------------------------------------------------------------------- ///
namespace arts {
class EDTGraphNode;
class EDTGraphSlotNode;

class CreationGraphEdge {
public:
  CreationGraphEdge(EDTGraphNode *From, EDTGraphNode *To);
  ~CreationGraphEdge();
  void print();
  EDTGraphNode *getFrom();
  EDTGraphNode *getTo();
  /// Insert interface
  bool insertParameter(EDTValue *V);
  bool insertGuid(EDT *Guid);
  /// Has interface
  bool hasParameter(EDTValue *V);
  bool hasGuid(EDT *Guid);
  /// Remove interface
  bool removeParameter(EDTValue *V);
  bool removeGuid(EDT *Guid);
  /// Get size
  uint32_t getParametersSize();
  uint32_t getGuidsSize();
  /// For each interface
  void forEachParameter(std::function<void(EDTValue *)> F);
  void forEachGuid(std::function<void(EDT *)> F);

private:
  EDTGraphNode *From = nullptr;
  EDTGraphNode *To = nullptr;

  /// Input parameters
  SetVector<EDTValue *> Parameters;
  SetVector<EDT *> Guids;
};

class DataBlockGraphEdge {
public:
  DataBlockGraphEdge(EDTGraphNode *From, EDTGraphSlotNode *To, DataBlock *DB);
  ~DataBlockGraphEdge();
  EDTGraphNode *getFrom();
  EDTGraphSlotNode *getTo();
  DataBlock *getDataBlock();

private:
  EDTGraphNode *From = nullptr;
  EDTGraphSlotNode *To = nullptr;
  DataBlock *DB = nullptr;
};

} // namespace arts

#endif // LLVM_ARTSEDGE_H
