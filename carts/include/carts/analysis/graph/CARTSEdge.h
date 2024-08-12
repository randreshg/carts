//===- EDTEdge.h - EDTEdge-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTEDGE_H
#define LLVM_EDTEDGE_H

#include "carts/analysis/graph/CARTSGraph.h"
#include "carts/utils/ARTS.h"
// #include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
// #include <unordered_set>

/// ------------------------------------------------------------------- ///
///                             EDT EDGE                               ///
/// The data structure used to represent dependencies between EDTs.
/// ------------------------------------------------------------------- ///
namespace arts {
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

  /// Getters
  SetVector<EDTValue *> &getParameters();
  SetVector<EDT *> &getGuids();

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

#endif // LLVM_EDTEDGE_H
