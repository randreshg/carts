//===- EDTGraph.h - EDTGraph-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTGRAPH_H
#define LLVM_EDTGRAPH_H

#include "ARTS.h"
/// ------------------------------------------------------------------- ///
///                             EDT GRAPH                               ///
/// The data structure used to represent the EDTs and its dependencies
/// in the program.
/// ------------------------------------------------------------------- ///
namespace arts {
using namespace arcana;

class EDTGraphEdge {
public:
  EDTGraphEdge();
  ~EDTGraphEdge();

  bool isDataDep(void);
  bool isControlDep(void);
  void print();

private:
  bool IsDataDep = false;
  bool IsControlDep = false;
};

class EDTGraphNode {
public:
  EDTGraphNode(EDT &E);
  ~EDTGraphNode();
  void print(void);

private:
  EDT &E;
};

class EDTGraph {
public:
  EDTGraph(EDTCache &Cache);

  EDTGraphNode *getEntryNode() const;
  EDTGraphNode *getNode(EDT *E) const;
  void print();

private:
  void insertNode(EDT *E);
  void addEdge(EDTGraphNode *Src, EDTGraphNode *Dst, EDTGraphEdge *E);
  void removeEdge(EDTGraphNode *Src, EDTGraphNode *Dst);
  void insertDeps();

  /// Attributes
  EDTCache &Cache;
  std::unordered_map<Function *, EDTGraphNode *> EDTs;
  std::unordered_map<EDTGraphNode *,
                     std::unordered_map<EDTGraphNode *, EDTGraphEdge *>>
      IncomingEdges;
  std::unordered_map<EDTGraphNode *,
                     std::unordered_map<EDTGraphNode *, EDTGraphEdge *>>
      OutgoingEdges;
};
} // namespace arts

#endif // LLVM_EDTGRAPH_H
