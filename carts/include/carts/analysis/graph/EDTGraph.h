//===- EDTGraph.h - EDTGraph-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTGRAPH_H
#define LLVM_EDTGRAPH_H

#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include <unordered_set>

#include "carts/utils/ARTS.h"
#include "noelle/core/Noelle.hpp"

/// ------------------------------------------------------------------- ///
///                              EDT GRAPH                              ///
/// The data structure used to represent the EDTs and its dependencies
/// in the program.
/// ------------------------------------------------------------------- ///
namespace arts {
using namespace std;
using namespace arcana::noelle;

class EDTGraphCache : public EDTCache {
public:
  EDTGraphCache(Module &M, Noelle &NM, MemorySSA &MSSA)
      : EDTCache(M), NM(NM), MSSA(MSSA) {}
  ~EDTGraphCache() {}
  Noelle &getNoelle() { return NM; }
  MemorySSA &getMemorySSA() { return MSSA; }

private:
  Noelle &NM;
  MemorySSA &MSSA;
};

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
  EDTGraph(EDTGraphCache &Cache);
  ~EDTGraph();

  EDTGraphNode *getEntryNode() const;
  EDTGraphNode *getNode(Function &Fn) const;
  EDTGraphNode *getNode(EDT *E) const;

private:
  void createNode(Function &Fn);
  void createNodes();
  void setCreationDeps();
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
  EDTGraphCache &Cache;
  DenseMap<Function *, EDTGraphNode *> EDTs;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, EDTGraphEdge *>>
      IncomingEdges;
  DenseMap<EDTGraphNode *, DenseMap<EDTGraphNode *, EDTGraphEdge *>>
      OutgoingEdges;
  SmallSetVector<EDTGraphNode *, 4> InReachableEDTs;
  SmallSetVector<EDTGraphNode *, 4> OutReachableEDTs;

public:
  void print();
};
} // namespace arts

#endif // LLVM_EDTGRAPH_H
