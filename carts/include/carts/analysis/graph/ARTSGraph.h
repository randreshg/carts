//===- ARTSGraph.h - ARTSGraph-Related structs --------------------*- C++
//-*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTSGRAPH_H
#define LLVM_ARTSGRAPH_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include <cstdint>
#include <sys/types.h>
#include <unordered_set>

#include "carts/analysis/graph/ARTSEdge.h"
#include "carts/utils/ARTS.h"

/// ------------------------------------------------------------------- ///
///                              ARTS GRAPH                              ///
/// The data structure used to represent the EDTs and its dependencies
/// in the program.
/// ------------------------------------------------------------------- ///
namespace arts {
using namespace std;

/// Classes
class EDTGraphNode {
public:
  EDTGraphNode(EDT &E);
  ~EDTGraphNode();
  void print(void);
  EDT *getEDT();
  EDTGraphSlotNode *getOrCreateSlotNode(uint32_t Slot);
  void forEachIncomingSlotNode(function<void(EDTGraphSlotNode *)> Fn);
  uint32_t getIncomingSlotNodesSize();

private:
  DenseMap<uint32_t, EDTGraphSlotNode *> IncomingSlotNodes;
  EDT &E;
};

class EDTGraphSlotNode {
public:
  EDTGraphSlotNode(EDTGraphNode *Parent, uint32_t Slot);
  ~EDTGraphSlotNode();
  void print(void);
  EDTGraphNode *getParent();
  uint32_t getSlot();
  bool insertIncomingDataEdge(DataBlockGraphEdge *Edge);
  SetVector<DataBlockGraphEdge *> &getIncomingDataEdges();

private:
  SetVector<DataBlockGraphEdge *> IncomingDataEdges;
  EDTGraphNode *Parent = nullptr;
  uint32_t Slot;
};

class ARTSGraph {
public:
  ARTSGraph();
  ~ARTSGraph();

  /// Analysis
  bool isCreationReachable(EDT *From, EDT *To);
  bool isDataReachable(EDT *From, EDT *To);
  bool isDataReachable(EDT *From, EDT *To, uint32_t Slot);
  EDT *getParentSyncEDT(EDT *Node, uint32_t Depth = 0);
  bool isChild(EDT *Parent, EDT *Child);

  /// Public interface
  EDTGraphNode *getEntryNode();
  EDTGraphNode *getExitNode();
  EDTGraphNode *getOrCreateNode(EDT *E);
  EDTGraphNode *getOrCreateNode(EDT *E, EDT *ParentEDT);
  EDTGraphSlotNode *getOrCreateSlotNode(EDT *Parent, uint32_t Slot);

  bool insertCreationEdge(EDT *From, EDT *To);
  bool insertCreationEdgeParameter(EDT *From, EDT *To, EDTValue *Parameter);
  bool insertCreationEdgeGuid(EDT *From, EDT *To, EDT *Done);
  bool insertDataBlockEdge(EDT *From, EDT *To, uint32_t Slot, DataBlock *DB);

  CreationGraphEdge *getEdge(EDT *From, EDT *To);
  DataBlockGraphEdge *getEdge(EDT *From, EDT *To, uint32_t Slot);

  void forEachEDTNode(function<void(EDTGraphNode *)> Fn);
  void forEachIncomingCreationEdge(EDTGraphNode *Node,
                                   function<void(CreationGraphEdge *)> Fn);
  void forEachOutgoingCreationEdge(EDTGraphNode *Node,
                                   function<void(CreationGraphEdge *)> Fn);
  void forEachIncomingDataBlockEdge(EDTGraphNode *Node,
                                    function<void(DataBlockGraphEdge *)> Fn);
  void forEachOutgoingDataBlockEdge(EDTGraphNode *Node,
                                    function<void(DataBlockGraphEdge *)> Fn);

private:
  /// Nodes
  EDTGraphNode *getNode(EDT *E) const;
  EDTGraphNode *getNode(Function &Fn) const;
  unordered_set<EDTGraphNode *> getNodes();

  /// Edges
  CreationGraphEdge *getEdge(EDTGraphNode *From, EDTGraphNode *To);
  DataBlockGraphEdge *getEdge(EDTGraphNode *From, EDTGraphSlotNode *To);
  unordered_set<CreationGraphEdge *>
  getIncomingCreationEdges(EDTGraphNode *Node);
  unordered_set<CreationGraphEdge *>
  getOutgoingCreationEdges(EDTGraphNode *Node);
  unordered_set<DataBlockGraphEdge *>
  getIncomingDataBlockEdges(EDTGraphNode *Node);
  unordered_set<DataBlockGraphEdge *>
  getIncomingDataBlockEdges(EDTGraphSlotNode *Node);
  unordered_set<DataBlockGraphEdge *>
  getOutgoingDataBlockEdges(EDTGraphNode *Node);
  unordered_set<CreationGraphEdge *> getCreationEdges(EDTGraphNode *Node);
  CreationGraphEdge *fetchOrCreateEdge(EDTGraphNode *From, EDTGraphNode *To);
  DataBlockGraphEdge *fetchOrCreateEdge(EDTGraphNode *From,
                                        EDTGraphSlotNode *To, DataBlock *DB);

  /// Insert edges
  void insertEdge(EDTGraphNode *From, EDTGraphNode *To,
                  CreationGraphEdge *Edge);
  void insertEdge(EDTGraphNode *From, EDTGraphSlotNode *To,
                  DataBlockGraphEdge *Edge);
  CreationGraphEdge *insertCreationEdge(EDTGraphNode *From, EDTGraphNode *To);
  DataBlockGraphEdge *insertDataBlockEdge(EDTGraphNode *From,
                                          EDTGraphSlotNode *To, DataBlock *DB);

  /// Remove edges
  // void removeEdge(CreationGraphEdge *Edge);
  // void removeEdge(DataBlockGraphEdge *Edge);
  // void removeEdge(EDTGraphNode *From, EDTGraphNode *To);
  // void removeEdge(EDTGraphNode *From, EDTGraphSlotNode *To);

  /// Attributes
  DenseMap<Function *, EDTGraphNode *> EDTs;

  /// EDTCreationEdges
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, CreationGraphEdge *>>
      IncomingCreationEdges;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, CreationGraphEdge *>>
      OutgoingCreationEdges;
  /// DataBlockGraphEdges
  DenseMap<EDTGraphSlotNode *, DenseMap<EDTGraphNode *, DataBlockGraphEdge *>>
      IncomingDataBlockEdges;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphSlotNode *, DataBlockGraphEdge *>>
      OutgoingDataBlockEdges;

  /// EntryNode
  EDTGraphNode *EntryNode = nullptr;
  /// ExitNode
  EDTGraphNode *ExitNode = nullptr;

public:
  void print();
};
} // namespace arts

#endif // LLVM_ARTSGRAPH_H
