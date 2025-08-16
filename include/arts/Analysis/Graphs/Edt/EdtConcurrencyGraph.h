//===----------------------------------------------------------------------===//
// EdtConcurrencyGraph.h - Undirected graph of potentially concurrent EDTs
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_EDT_EDTCONCURRENCYGRAPH_H
#define ARTS_ANALYSIS_GRAPHS_EDT_EDTCONCURRENCYGRAPH_H

#include "mlir/IR/Operation.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {

class EdtGraph; // fwd decl
class EdtTaskNode; // fwd decl

class EdtAnalysis; // forward declaration

/// Undirected edge annotated with optional affinity metrics
struct EdtConcurrencyEdge {
  Operation *a = nullptr;
  Operation *b = nullptr;
  double dataOverlap = 0.0;
  double hazardScore = 0.0;
  bool mayConflict = false;
  double reuseProximity = 0.0;
  double localityScore = 0.0;
  double concurrencyRisk = 0.0;
};

/// Concurrency graph built from EdtGraph.
/// Responsibility: identify pairs of EDTs that are not ordered by dependencies
/// and may execute concurrently. Optional edge annotations consumed from
/// EdtAnalysis. No IR mutation.
class EdtConcurrencyGraph {
public:
  EdtConcurrencyGraph(EdtGraph *edtGraph, EdtAnalysis *analysis = nullptr)
      : edtGraph(edtGraph), analysis(analysis) {}

  void build();
  void clear();

  llvm::ArrayRef<Operation *> getTasks() const { return tasks; }
  llvm::ArrayRef<EdtConcurrencyEdge> getEdges() const { return edges; }

  void print(llvm::raw_ostream &os) const;
  void exportToDot(llvm::raw_ostream &os) const;
  void exportToJson(llvm::raw_ostream &os) const;

private:
  EdtGraph *edtGraph = nullptr;                // not owned
  EdtAnalysis *analysis = nullptr;             // not owned, optional
  llvm::SmallVector<Operation *, 16> tasks;    // stable list of task ops
  llvm::SmallVector<EdtConcurrencyEdge, 32> edges; // undirected unique edges
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_EDT_EDTCONCURRENCYGRAPH_H


