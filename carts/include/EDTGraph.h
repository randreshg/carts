//===- EDTGraph.h - EDTGraph-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTGRAPH_H
#define LLVM_EDTGRAPH_H

#include "ARTS.h"
#include "llvm/IR/Function.h"
#include <unordered_set>
/// ------------------------------------------------------------------- ///
///                             EDT GRAPH                               ///
/// The data structure used to represent the EDTs and its dependencies
/// in the program.
/// ------------------------------------------------------------------- ///
namespace arts {
using namespace arcana;

class EDTGraphNode {
public:
  EDTGraphNode(EDT &E);
  ~EDTGraphNode();
  void print(void);
  EDT *getEDT() { return &E; }

private:
  EDT &E;
};

class EDTGraphEdge {
public:
  EDTGraphEdge(EDTGraphNode *From, EDTGraphNode *To);
  ~EDTGraphEdge();

  bool isDataDep(void);
  bool isControlDep(void);
  bool isCreationDep(void);
  void setDataDep(bool Dep) { IsDataDep = Dep; }
  void setControlDep(bool Dep) { IsControlDep = Dep; }
  void setCreationDep(bool Dep) { IsCreationDep = Dep; }
  void print();

  EDTGraphNode *getFrom() { return From; }
  EDTGraphNode *getTo() { return To; }

private:
  bool IsDataDep = false;
  bool IsControlDep = false;
  bool IsCreationDep = false;
  EDTGraphNode *From = nullptr;
  EDTGraphNode *To = nullptr;
};

class EDTGraph {
public:
  EDTGraph(EDTCache &Cache);
  ~EDTGraph();

  EDTGraphNode *getEntryNode() const;
  EDTGraphNode *getNode(Function &F) const;
  EDTGraphNode *getNode(EDT *E) const;

private:
  void createNodes();
  std::unordered_set<EDTGraphNode *> getNodes();
  EDTGraphNode *insertNode(EDT *E);
  EDTGraphNode *insertNode(EDT *E, EDTGraphNode *ParentNode);
  EDTGraphNode *insertNode(EDT *E, Function &F);
  /// Edges
  EDTGraphEdge *getEdge(EDTGraphNode *From, EDTGraphNode *To);
  std::unordered_set<EDTGraphEdge *> getIncomingEdges(EDTGraphNode *Node);
  std::unordered_set<EDTGraphEdge *> getOutgoingEdges(EDTGraphNode *Node);
  std::unordered_set<EDTGraphEdge *> getEdges(EDTGraphNode *Node);
  EDTGraphEdge *fetchOrCreateEdge(EDTGraphNode *From, EDTGraphNode *To);
  void addCreationEdge(EDTGraphNode *From, EDTGraphNode *To);
  void addDataEdge(EDTGraphNode *From, EDTGraphNode *To);
  void addControlEdge(EDTGraphNode *From, EDTGraphNode *To);
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
