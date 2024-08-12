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

class EDTGraphCreationEdge {
public:
  EDTGraphCreationEdge(EDTGraphNode *From, EDTGraphNode *To);
  virtual ~EDTGraphCreationEdge();
  void print();

  EDTGraphNode *getFrom() { return From; }
  EDTGraphNode *getTo() { return To; }

  /// Add interface
  bool addParameter(EDTValue *V);
  bool addGuid(EDT *Guid);

  /// Has interface
  bool hasParameter(EDTValue *V);
  bool hasGuid(EDT *Guid);

  /// Remove interface
  bool removeParameter(EDTValue *V);
  bool removeGuid(EDT *Guid);

  /// Getters
  SetVector<EDTValue *> &getParameters() { return Parameters; }
  SetVector<EDT *> &getGuids() { return Guids; }

private:
  EDTGraphNode *From = nullptr;
  EDTGraphNode *To = nullptr;

  /// Input parameters
  SetVector<EDTValue *> Parameters;
  SetVector<EDT *> Guids;
};

class EDTGraphDataBlockEdge {
public:
  EDTGraphDataBlockEdge(EDTGraphNode *From, EDTGraphSlotNode *To,
                        DataBlock *DB);
  virtual ~EDTGraphDataBlockEdge();
  void print();

  EDTGraphNode *getFrom() { return From; }
  EDTGraphSlotNode *getTo() { return To; }
  DataBlock *getDataBlock() { return DB; }

private:
  EDTGraphNode *From = nullptr;
  EDTGraphSlotNode *To = nullptr;
  DataBlock *DB = nullptr;
};

} // namespace arts

#endif // LLVM_EDTEDGE_H
