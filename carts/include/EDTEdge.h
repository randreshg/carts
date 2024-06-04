//===- EDTGraph.h - EDTGraph-Related structs --------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EDTEDGE_H
#define LLVM_EDTEDGE_H

#include "EDTGraph.h"
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
  virtual bool isControlEdge() const  = 0;

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

class EDTGraphDataEdgeVal;
class EDTGraphDataEdge : public EDTGraphEdge {
public:
  EDTGraphDataEdge(EDTGraphNode *From, EDTGraphNode *To);
  virtual ~EDTGraphDataEdge();
  void addValue(Value *V);
  void removeValue(Value *V);
  // std::unordered_set<EDTGraphDataEdgeVal *> getValues() { return Values; }
  std::unordered_set<Value *> getValues() { return Values; }
  bool isDataEdge() const override { return true; }
  bool isControlEdge() const override { return false; }

  static bool classof(const EDTGraphEdge *E) { return E->isDataEdge(); }

private:
  // std::unordered_set<EDTGraphDataEdgeVal *> Values;
  std::unordered_set<Value *> Values;
};

class EDTGraphDataEdgeVal {
public:
  EDTGraphDataEdgeVal(EDTGraphDataEdge *Parent, Value *V);
  ~EDTGraphDataEdgeVal();
  EDTGraphDataEdge *getParent() { return Parent; }
  Value *getValue() { return V; }

private:
  EDTGraphDataEdge *Parent;
  Value *V;
};

} // namespace arts

#endif // LLVM_EDTEDGE_H
