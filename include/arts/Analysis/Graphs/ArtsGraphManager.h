///==========================================================================
/// File: ArtsGraphManager.h
/// Coordinator for DbGraph and EdtGraph management.
///==========================================================================

#ifndef ARTS_ANALYSIS_GRAPHS_ARTSGRAPHMANAGER_H
#define ARTS_ANALYSIS_GRAPHS_ARTSGRAPHMANAGER_H

#include "arts/Analysis/Graphs/Base/GraphBase.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/ArtsDialect.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

namespace mlir {
namespace arts {

class ArtsGraphManager {
public:
  ArtsGraphManager(std::unique_ptr<DbGraph> dbGraph,
                   std::unique_ptr<EdtGraph> edtGraph);

  /// Build both graphs
  void build();

  /// Invalidate both graphs
  void invalidate();

  /// Print combined summary
  void print(llvm::raw_ostream &os) const;

  /// Unified node queries
  NodeBase *getNode(Operation *op) const;

  /// Combined queries
  bool isEdtDependentOnDb(EdtOp edt, DbAllocOp alloc);
  SmallVector<NodeBase *, 4> getDbNodesForEdt(EdtOp edt) const;

  /// Placeholder for concurrency graph generation
  void buildConcurrencyView(llvm::raw_ostream &os) const;

private:
  std::unique_ptr<DbGraph> dbGraph;
  std::unique_ptr<EdtGraph> edtGraph;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_ARTSGRAPHMANAGER_H