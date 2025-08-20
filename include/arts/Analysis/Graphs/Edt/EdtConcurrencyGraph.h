///==========================================================================
/// File: EdtConcurrencyGraph.h
///
/// This file defines the concurrency graph for EDT analysis.
/// Identifies pairs of EDTs that may execute concurrently.
///==========================================================================

#ifndef ARTS_ANALYSIS_GRAPHS_EDT_EDTCONCURRENCYGRAPH_H
#define ARTS_ANALYSIS_GRAPHS_EDT_EDTCONCURRENCYGRAPH_H

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

/// Includes
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"

namespace mlir {
namespace arts {

/// Edge between two EDTs that may execute concurrently.
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

/// Concurrency graph for EDT analysis.
/// Identifies pairs of EDTs that are not ordered by dependencies and may
/// execute concurrently.
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
  EdtGraph *edtGraph = nullptr;
  EdtAnalysis *analysis = nullptr;
  llvm::SmallVector<Operation *, 16> tasks;
  llvm::SmallVector<EdtConcurrencyEdge, 32> edges;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_EDT_EDTCONCURRENCYGRAPH_H
