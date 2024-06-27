//===- EDTGraph.h - EDTGraph-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTGRAPH_H
#define LLVM_EDTGRAPH_H

#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include <unordered_set>

#include "carts/analysis/ARTSAnalysisPass.h"
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

private:
  void createNode(Function &Fn);
  void createNodes();
  void setDeps(EDTGraphNode *Node);
  void setCreationDeps();
  void setDataDeps();
  EDTGraphNode *getClobberingEDT(MemorySSA &MSSA, CallBase *Inst);
  void analyzeDeps();
  void analyzeReachability();
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
  void addCreationEdge(EDTGraphNode *From, EDTGraphNode *To);
  void addDataEdge(EDTGraphNode *From, EDTGraphNode *To, Value *V = nullptr);
  void addControlEdge(EDTGraphNode *From, EDTGraphNode *To);
  void removeEdge(EDTGraphEdge *Edge);
  void removeEdge(EDTGraphNode *From, EDTGraphNode *To);
  void addReachableEDT(EDTGraphNode *From);

  /// Attributes
  // EDTGraphCache &Cache;
  DenseMap<Function *, EDTGraphNode *> EDTs;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, EDTGraphEdge *>>
      IncomingEdges;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, EDTGraphEdge *>>
      OutgoingEdges;

  ////
  // FunctionsManager *FM;
public:
  void print();
};
} // namespace arts

#endif // LLVM_EDTGRAPH_H
