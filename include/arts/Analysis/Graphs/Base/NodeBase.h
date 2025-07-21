//===----------------------------------------------------------------------===//
// NodeBase.h - Abstract base class for graph nodes
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_NODEBASE_H
#define ARTS_ANALYSIS_GRAPHS_NODEBASE_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"
#include "mlir/IR/Operation.h"

namespace mlir {
namespace arts {

class EdgeBase;

/// Abstract base class for all nodes (e.g., allocs, deps, tasks).
/// Provides common interface for hierarchy, printing, and edges.
class NodeBase {
public:
  virtual ~NodeBase() = default;

  /// Get the hierarchical ID (e.g., "A.1" for deps).
  virtual StringRef getHierId() const = 0;

  /// Print node details.
  virtual void print(llvm::raw_ostream &os) const = 0;

  /// Get the underlying MLIR operation.
  virtual Operation *getOp() const = 0;

  /// Add incoming/outgoing edges.
  virtual void addInEdge(EdgeBase *edge) = 0;
  virtual void addOutEdge(EdgeBase *edge) = 0;

  /// Get incoming/outgoing edges.
  virtual const DenseSet<EdgeBase *> &getInEdges() const = 0;
  virtual const DenseSet<EdgeBase *> &getOutEdges() const = 0;

protected:
  // Protected for derived classes.
  DenseSet<EdgeBase *> inEdges;
  DenseSet<EdgeBase *> outEdges;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_NODEBASE_H