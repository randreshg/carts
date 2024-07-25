//===- EDTGraph.h - EDTGraph-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTEDGE_H
#define LLVM_EDTEDGE_H

#include "carts/analysis/graph/EDTGraph.h"
#include "carts/utils/ARTS.h"
#include "llvm/ADT/SetVector.h"
#include <unordered_set>

/// ------------------------------------------------------------------- ///
///                             EDT EDGE                               ///
/// The data structure used to represent dependencies between EDTs.
/// ------------------------------------------------------------------- ///
namespace arts {

class EDTGraphEdge {
public:
  EDTGraphEdge(EDTGraphNode *From, EDTGraphNode *To);
  virtual ~EDTGraphEdge();
  void print();

  EDTGraphNode *getFrom() { return From; }
  EDTGraphNode *getTo() { return To; }
  void setCreationDep(bool HasCreationDep) {
    this->HasCreationDep = HasCreationDep;
  }
  bool hasCreationDep() { return HasCreationDep; }
  virtual bool isDataEdge() const = 0;
  virtual bool isControlEdge() const = 0;

  static bool classof(const EDTGraphEdge *E) { return true; }

private:
  EDTGraphNode *From = nullptr;
  EDTGraphNode *To = nullptr;
  bool HasCreationDep = false;
};

class EDTGraphControlEdge : public EDTGraphEdge {
public:
  EDTGraphControlEdge(EDTGraphNode *From, EDTGraphNode *To);
  virtual ~EDTGraphControlEdge();
  bool isDataEdge() const override { return false; }
  bool isControlEdge() const override { return true; }

  static bool classof(const EDTGraphEdge *E) { return E->isControlEdge(); }

private:
  // bool IsBackEdge = false;
  // bool IsForwardEdge = false;
  // bool IsLoop = false;
};

class EDTGraphDataBlockEdge;
class EDTGraphDataEdge : public EDTGraphEdge {
public:
  EDTGraphDataEdge(EDTGraphNode *From, EDTGraphNode *To);
  virtual ~EDTGraphDataEdge();
  /// Add interface
  bool addDataBlock(EDTDataBlock *DB);
  bool addEDTValue(EDTValue *V);
  bool addEDTGuid(EDT *Guid);

  /// Remove interface
  bool removeDataBlock(EDTDataBlock *DB);
  bool removeEDTValue(EDTValue *V);
  bool removeEDTGuid(EDT *EDTGuid);

  /// Getters
  SetVector<EDTDataBlock *> getDataBlocks() { return DataBlocks; }
  SetVector<EDTValue *> getValues() { return Values; }
  SetVector<EDT *> getEDTGuids() { return EDTGuids; }

  /// Helpers
  bool isDataEdge() const override { return true; }
  bool isControlEdge() const override { return false; }

  static bool classof(const EDTGraphEdge *E) { return E->isDataEdge(); }

private:
  SetVector<EDTDataBlock *> DataBlocks;
  SetVector<EDTValue *> Values;
  SetVector<EDT *> EDTGuids;
};

class EDTGraphDataBlockEdge {
public:
  EDTGraphDataBlockEdge(EDTGraphDataEdge *Parent, EDTDataBlock *DB);
  ~EDTGraphDataBlockEdge();
  EDTGraphDataEdge *getParent() { return Parent; }
  EDTDataBlock *getDataBlock() { return DB; }

private:
  EDTGraphDataEdge *Parent;
  EDTDataBlock *DB;
};

} // namespace arts

#endif // LLVM_EDTEDGE_H
