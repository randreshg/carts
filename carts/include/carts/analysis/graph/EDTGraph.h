//===- EDTGraph.h - EDTGraph-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTGRAPH_H
#define LLVM_EDTGRAPH_H

#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include <unordered_set>

#include "carts/utils/ARTS.h"

/// ------------------------------------------------------------------- ///
///                              EDT GRAPH                              ///
/// The data structure used to represent the EDTs and its dependencies
/// in the program.
/// ------------------------------------------------------------------- ///
namespace arts {
using namespace std;

class EDTGraphEdge;
class EDTGraphNode {
public:
  EDTGraphNode(EDT &E);
  ~EDTGraphNode();
  void print(void);
  EDT *getEDT() { return &E; }

private:
  EDT &E;
};

class EDTGraph {
public:
  EDTGraph();
  ~EDTGraph();

  EDTGraphNode *getEntryNode() const;
  EDTGraphNode *getNode(Function &Fn) const;
  EDTGraphNode *getNode(EDT *E) const;

  /// Analysis
  bool isReachable(EDTGraphNode *From, EDTGraphNode *To);
  bool isCreationReachable(EDTGraphNode *From, EDTGraphNode *To);
  bool isDataDependent(EDTGraphNode *From, EDTGraphNode *To);
  EDT *getParentSyncEDT(EDTGraphNode *Node, uint32_t Depth = 0);

  /// Nodes
  void createNode(Function &Fn);

  unordered_set<EDTGraphNode *> getNodes();
  EDTGraphNode *insertNode(EDT *E);
  EDTGraphNode *insertNode(EDT *E, EDTGraphNode *ParentNode,
                           Function *F = nullptr);
  EDTGraphNode *insertNode(EDT *E, Function &Fn);
  /// Edges
  EDTGraphEdge *getEdge(EDTGraphNode *From, EDTGraphNode *To);
  unordered_set<EDTGraphEdge *> getIncomingEdges(EDTGraphNode *Node);
  unordered_set<EDTGraphEdge *> getOutgoingEdges(EDTGraphNode *Node);
  unordered_set<EDTGraphEdge *> getEdges(EDTGraphNode *Node);
  EDTGraphEdge *fetchOrCreateEdge(EDTGraphNode *From, EDTGraphNode *To,
                                  bool IsDataEdge);
  void addEdge(EDTGraphNode *From, EDTGraphNode *To, EDTGraphEdge *Edge);
  /// Add edges with EDT
  EDTGraphEdge *addCreationEdge(EDT *From, EDT *To);
  EDTGraphEdge *addDataEdge(EDT *From, EDT *To, EDTDataBlock *DB);
  EDTGraphEdge *addDataEdge(EDT *From, EDT *To, EDTValue *Parameter);
  EDTGraphEdge *addControlEdge(EDT *From, EDT *To);
  /// Add edges with EDTGraphNode
  EDTGraphEdge *addCreationEdge(EDTGraphNode *From, EDTGraphNode *To);
  EDTGraphEdge *addDataEdge(EDTGraphNode *From, EDTGraphNode *To,
                            EDTDataBlock *DB);
  EDTGraphEdge *addDataEdge(EDTGraphNode *From, EDTGraphNode *To,
                            EDTValue *Parameter);
  EDTGraphEdge *addControlEdge(EDTGraphNode *From, EDTGraphNode *To);

  void removeEdge(EDTGraphEdge *Edge);
  void removeEdge(EDTGraphNode *From, EDTGraphNode *To);
  void addReachableEDT(EDTGraphNode *From);

  /// Attributes
  DenseMap<Function *, EDTGraphNode *> EDTs;

  /// EDTEdges
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, EDTGraphEdge *>>
      IncomingEdges;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, EDTGraphEdge *>>
      OutgoingEdges;

public:
  void print();
};
} // namespace arts

#endif // LLVM_EDTGRAPH_H
