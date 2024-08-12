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

class EDTGraphCreationEdge;

class EDTGraphDataBlockEdge;
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

private:
  EDTGraphCreationEdge *IncomingCreationEdge = nullptr;
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
  SetVector<EDTGraphDataBlockEdge *> IncomingDataEdges;
  EDTGraphNode *Parent = nullptr;
  uint32_t Slot;
};

class CARTSGraph {
public:
  CARTSGraph();
  ~CARTSGraph();

  /// Analysis
  bool isReachable(EDTGraphNode *From, EDTGraphNode *To);
  bool isCreationReachable(EDTGraphNode *From, EDTGraphNode *To);
  bool isDataDependent(EDTGraphNode *From, EDTGraphNode *To);
  EDT *getParentSyncEDT(EDTGraphNode *Node, uint32_t Depth = 0);
  bool isChild(EDT *Parent, EDT *Child);
  bool isChild(EDTGraphNode *Parent, EDTGraphNode *Child);

  /// Getters
  EDTGraphNode *getNode(EDT *E) const;
  EDTGraphNode *getNode(Function &Fn) const;
  EDTGraphNode *getEntryNode();
  EDTGraphNode *getExitNode();
  /// Nodes
  void createNode(Function &Fn);
  unordered_set<EDTGraphNode *> getNodes();
  EDTGraphNode *insertNode(EDT *E);
  EDTGraphNode *insertNode(EDT *E, EDTGraphNode *ParentNode,
                           Function *F = nullptr);
  EDTGraphNode *insertNode(EDT *E, Function &Fn);

  /// Edges with EDTs
  EDTGraphCreationEdge *getEdge(EDT *From, EDT *To);
  EDTGraphCreationEdge *getEdge(EDTGraphNode *From, EDTGraphNode *To);
  unordered_set<EDTGraphCreationEdge *> getIncomingEdges(EDT *Node);
  unordered_set<EDTGraphCreationEdge *> getIncomingEdges(EDTGraphNode *Node);
  unordered_set<EDTGraphCreationEdge *> getOutgoingEdges(EDT *Node);
  unordered_set<EDTGraphCreationEdge *> getOutgoingEdges(EDTGraphNode *Node);


  unordered_set<EDTGraphCreationEdge *> getEdges(EDTGraphNode *Node);
  EDTGraphCreationEdge *fetchOrCreateEdge(EDTGraphNode *From, EDTGraphNode *To,
                                  bool IsDataEdge);
  void addEdge(EDTGraphNode *From, EDTGraphNode *To, EDTGraphCreationEdge *Edge);
  /// Add edges with EDT
  EDTGraphCreationEdge *addCreationEdge(EDT *From, EDT *To);
  EDTGraphCreationEdge *addDataEdge(EDT *From, EDT *To, EDTValue *Parameter);
  EDTGraphCreationEdge *addDataEdge(EDT *From, EDT *To, EDT *Guid);
  EDTGraphCreationEdge *addDataEdge(EDTGraphNode *From, EDTGraphNode *To,
                            EDTValue *Parameter);
  EDTGraphCreationEdge *addDataEdge(EDTGraphNode *From, EDTGraphNode *To, EDT *Guid);

  EDTGraphCreationEdge *addDataEdge(EDT *From, EDT *To, DataBlock *DB);
  EDTGraphCreationEdge *addControlEdge(EDT *From, EDT *To);
  /// Add edges with EDTGraphNode
  EDTGraphCreationEdge *addCreationEdge(EDTGraphNode *From, EDTGraphNode *To);
  EDTGraphCreationEdge *addDataEdge(EDTGraphNode *From, EDTGraphNode *To,
                            DataBlock *DB);

  EDTGraphCreationEdge *addControlEdge(EDTGraphNode *From, EDTGraphNode *To);

  void removeEdge(EDTGraphCreationEdge *Edge);
  void removeEdge(EDTGraphNode *From, EDTGraphNode *To);
  void addReachableEDT(EDTGraphNode *From);

  /// Attributes
  DenseMap<Function *, EDTGraphNode *> EDTs;

  /// EDTEdges
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, EDTGraphCreationEdge *>>
      IncomingEdges;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, EDTGraphCreationEdge *>>
      OutgoingEdges;

  /// EntryNode
  EDTGraphNode *EntryNode = nullptr;
  /// ExitNode
  EDTGraphNode *ExitNode = nullptr;

public:
  void print();
};
} // namespace arts

#endif // LLVM_EDTGRAPH_H
