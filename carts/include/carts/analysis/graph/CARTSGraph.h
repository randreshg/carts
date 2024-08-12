//===- CARTSGraph.h - CARTSGraph-Related structs --------------------*- C++
//-*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTGRAPH_H
#define LLVM_EDTGRAPH_H

#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include <cstdint>
#include <unordered_set>

#include "carts/utils/ARTS.h"

/// ------------------------------------------------------------------- ///
///                              EDT GRAPH                              ///
/// The data structure used to represent the EDTs and its dependencies
/// in the program.
/// ------------------------------------------------------------------- ///
namespace arts {
using namespace std;

class CreationGraphEdge;

class DataBlockGraphEdge;
class EDTGraphNode;
class EDTGraphSlotNode;

class DataBlockGraphNode {
public:
  DataBlockGraphNode(DataBlock *DB);
  ~DataBlockGraphNode();
  void print(void);
  DataBlock *getDataBlock() { return DB; }

private:
  DataBlock *DB;
};

class EDTGraphNode {
public:
  EDTGraphNode(EDT &E);
  ~EDTGraphNode();
  void print(void);
  EDT *getEDT() { return &E; }
  void insertSlotNode(uint32_t Slot);
  EDTGraphSlotNode *getSlotNode(uint32_t Slot);

private:
  CreationGraphEdge *IncomingCreationEdge = nullptr;
  SetVector<EDTGraphSlotNode *> IncomingSlotNodes;
  EDT &E;
};

class EDTGraphSlotNode {
public:
  EDTGraphSlotNode(EDTGraphNode *Parent, uint32_t Slot);
  ~EDTGraphSlotNode();
  void print(void);
  EDTGraphNode *getParent() { return Parent; }
  uint32_t getSlot() { return Slot; }

private:
  SetVector<DataBlockGraphEdge *> IncomingDataEdges;
  EDTGraphNode *Parent = nullptr;
  uint32_t Slot;
};

class CARTSGraph {
public:
  CARTSGraph();
  ~CARTSGraph();

  /// Analysis
  bool isReachable(EDTGraphNode *From, EDTGraphNode *To);
  // bool isCreationReachable(EDTGraphNode *From, EDTGraphNode *To);
  // bool isDataDependent(EDTGraphNode *From, EDTGraphNode *To);
  EDT *getParentSyncEDT(EDTGraphNode *Node, uint32_t Depth = 0);
  bool isChild(EDT *Parent, EDT *Child);
  bool isChild(EDTGraphNode *Parent, EDTGraphNode *Child);

  /// Getters
  EDTGraphNode *getNode(EDT *E) const;
  EDTGraphNode *getNode(Function &Fn) const;
  unordered_set<EDTGraphNode *> getNodes();
  EDTGraphNode *getEntryNode();
  EDTGraphNode *getExitNode();
  /// Nodes
  void createNode(Function &Fn);

  EDTGraphNode *insertNode(EDT *E);
  EDTGraphNode *insertNode(EDT *E, EDTGraphNode *ParentNode,
                           Function *F = nullptr);
  EDTGraphNode *insertNode(EDT *E, Function &Fn);

  /// Edges with EDTs
  CreationGraphEdge *getEdge(EDT *From, EDT *To);
  DataBlockGraphEdge *getEdge(EDT *From, EDT *To, uint32_t Slot);
  CreationGraphEdge *getEdge(EDTGraphNode *From, EDTGraphNode *To);
  DataBlockGraphEdge *getEdge(EDTGraphNode *From, EDTGraphSlotNode *To);
  unordered_set<CreationGraphEdge *> getIncomingCreationEdges(EDT *Node);
  unordered_set<CreationGraphEdge *>
  getIncomingCreationEdges(EDTGraphNode *Node);
  unordered_set<CreationGraphEdge *> getOutgoingEdges(EDT *Node);
  unordered_set<CreationGraphEdge *> getOutgoingEdges(EDTGraphNode *Node);

  unordered_set<CreationGraphEdge *> getEdges(EDTGraphNode *Node);
  CreationGraphEdge *fetchOrCreateEdge(EDTGraphNode *From, EDTGraphNode *To,
                                       bool IsDataEdge);
  void addEdge(EDTGraphNode *From, EDTGraphNode *To, CreationGraphEdge *Edge);
  /// Add edges with EDT
  CreationGraphEdge *addCreationEdge(EDT *From, EDT *To);
  CreationGraphEdge *addDataEdge(EDT *From, EDT *To, EDTValue *Parameter);
  CreationGraphEdge *addDataEdge(EDT *From, EDT *To, EDT *Guid);
  CreationGraphEdge *addDataEdge(EDTGraphNode *From, EDTGraphNode *To,
                                 EDTValue *Parameter);
  CreationGraphEdge *addDataEdge(EDTGraphNode *From, EDTGraphNode *To,
                                 EDT *Guid);

  CreationGraphEdge *addDataEdge(EDT *From, EDT *To, DataBlock *DB);
  CreationGraphEdge *addControlEdge(EDT *From, EDT *To);
  /// Add edges with EDTGraphNode
  CreationGraphEdge *addCreationEdge(EDTGraphNode *From, EDTGraphNode *To);
  CreationGraphEdge *addDataEdge(EDTGraphNode *From, EDTGraphNode *To,
                                 DataBlock *DB);

  CreationGraphEdge *addControlEdge(EDTGraphNode *From, EDTGraphNode *To);

  void removeEdge(CreationGraphEdge *Edge);
  void removeEdge(EDTGraphNode *From, EDTGraphNode *To);
  void addReachableEDT(EDTGraphNode *From);

  /// Attributes
  DenseMap<Function *, EDTGraphNode *> EDTs;

  /// EDTCreationEdges
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, CreationGraphEdge *>>
      IncomingCreationEdges;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, CreationGraphEdge *>>
      OutgoingCreationEdges;
  /// DataBlockGraphEdges
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, DataBlockGraphEdge *>>
      IncomingDataBlockEdges;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, DataBlockGraphEdge *>>
      OutgoingDataBlockEdges;

  /// EntryNode
  EDTGraphNode *EntryNode = nullptr;
  /// ExitNode
  EDTGraphNode *ExitNode = nullptr;

public:
  void print();
};
} // namespace arts

#endif // LLVM_EDTGRAPH_H
