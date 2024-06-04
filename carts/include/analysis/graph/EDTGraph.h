//===- EDTGraph.h - EDTGraph-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTGRAPH_H
#define LLVM_EDTGRAPH_H

#include "carts/analysis/ARTS.h"
#include "llvm/IR/Function.h"
#include <unordered_set>

/// ------------------------------------------------------------------- ///
///                             EDT GRAPH                               ///
/// The data structure used to represent the EDTs and its dependencies
/// in the program.
/// ------------------------------------------------------------------- ///
namespace arts {
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
  EDTGraph(EDTCache &Cache);
  ~EDTGraph();

  EDTGraphNode *getEntryNode() const;
  EDTGraphNode *getNode(Function &F) const;
  EDTGraphNode *getNode(EDT *E) const;

private:
  void createNode(Function &F);
  void createNodes();
  std::unordered_set<EDTGraphNode *> getNodes();
  EDTGraphNode *insertNode(EDT *E);
  EDTGraphNode *insertNode(EDT *E, EDTGraphNode *ParentNode, Function *F = nullptr);
  EDTGraphNode *insertNode(EDT *E, Function &F);
  /// Edges
  EDTGraphEdge *getEdge(EDTGraphNode *From, EDTGraphNode *To);
  std::unordered_set<EDTGraphEdge *> getIncomingEdges(EDTGraphNode *Node);
  std::unordered_set<EDTGraphEdge *> getOutgoingEdges(EDTGraphNode *Node);
  std::unordered_set<EDTGraphEdge *> getEdges(EDTGraphNode *Node);
  EDTGraphEdge *fetchOrCreateEdge(EDTGraphNode *From, EDTGraphNode *To,
                                  bool IsDataEdge);
  void addEdge(EDTGraphNode *From, EDTGraphNode *To, EDTGraphEdge *Edge);
  void addCreationEdge(EDTGraphNode *From, EDTGraphNode *To);
  void addDataEdge(EDTGraphNode *From, EDTGraphNode *To, Value *V = nullptr);
  void addControlEdge(EDTGraphNode *From, EDTGraphNode *To);
  void removeEdge(EDTGraphEdge *Edge);
  void removeEdge(EDTGraphNode *From, EDTGraphNode *To);

  /// Attributes
  EDTCache &Cache;
  std::unordered_map<Function *, EDTGraphNode *> EDTs;
  std::unordered_map<EDTGraphNode *,
                     std::unordered_map<EDTGraphNode *, EDTGraphEdge *>>
      IncomingEdges;
  std::unordered_map<EDTGraphNode *,
                     std::unordered_map<EDTGraphNode *, EDTGraphEdge *>>
      OutgoingEdges;

public:
  void print();
};
} // namespace arts

#endif // LLVM_EDTGRAPH_H
