///==========================================================================///
/// File: LoopNode.h
///
/// LoopNode represents a loop operation with integrated metadata.
/// Inherits from both NodeBase (for graph structure) and LoopMetadata
/// (for loop properties and analysis results).
///
/// Owned by LoopAnalysis, accessed via getLoopNode(Operation*).
///==========================================================================///

#ifndef ARTS_ANALYSIS_LOOPNODE_H
#define ARTS_ANALYSIS_LOOPNODE_H

#include "arts/Analysis/Graphs/Base/NodeBase.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "mlir/IR/Operation.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace mlir {
namespace arts {

///==========================================================================///
/// LoopNode - Represents a loop operation with metadata
/// Combines graph node capabilities (NodeBase) with loop metadata
///==========================================================================///
class LoopNode : public NodeBase, public LoopMetadata {
public:
  /// Constructor: automatically imports metadata from operation
  explicit LoopNode(Operation *loopOp);

  /// Get the underlying loop operation (affine.for, scf.for, scf.parallel)
  Operation *getLoopOp() const { return loopOp; }

  /// NodeBase interface
  StringRef getHierId() const override { return hierId; }
  void setHierId(std::string id) { hierId = id; }
  void print(llvm::raw_ostream &os) const override;
  Operation *getOp() const override { return loopOp; }

  void addInEdge(EdgeBase *edge) override { /* unused for loop nodes */ }
  void addOutEdge(EdgeBase *edge) override { /* unused for loop nodes */ }
  const DenseSet<EdgeBase *> &getInEdges() const override { return edges; }
  const DenseSet<EdgeBase *> &getOutEdges() const override { return edges; }

  NodeKind getKind() const override { return NodeKind::Loop; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::Loop;
  }

  /// All loop metadata fields inherited from LoopMetadata:
  ///   - potentiallyParallel
  ///   - hasReductions
  ///   - readCount, writeCount
  ///   - tripCount, nestingLevel
  ///   - hasInterIterationDeps
  ///   - suggestedPartitioning, suggestedChunkSize
  ///   - dataMovementPattern
  ///   etc. (directly accessible as members)

private:
  Operation *loopOp;  ///< The loop operation this node represents
  std::string hierId; ///< Hierarchical identifier for debugging
  DenseSet<EdgeBase *> edges; ///< Unused (kept for interface compliance)
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_LOOPNODE_H
