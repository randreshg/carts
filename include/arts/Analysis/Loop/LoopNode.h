///==========================================================================///
/// File: LoopNode.h
///
/// LoopNode represents a loop operation with integrated metadata.
///==========================================================================///

#ifndef ARTS_ANALYSIS_LOOPNODE_H
#define ARTS_ANALYSIS_LOOPNODE_H

#include "arts/Analysis/Graphs/Base/NodeBase.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>
#include <string>

namespace mlir {
namespace arts {

///==========================================================================///
/// LoopNode - Represents a loop operation with metadata
/// Combines graph node capabilities (NodeBase) with loop metadata
///==========================================================================///
class LoopAnalysis;
class LoopNode : public NodeBase, public LoopMetadata {
public:
  /// Constructor: automatically imports metadata from operation
  explicit LoopNode(Operation *loopOp, LoopAnalysis *loopAnalysis = nullptr);

  /// Get the underlying loop operation (affine.for, scf.for, scf.parallel)
  Operation *getLoopOp() const { return loopOp; }

  /// NodeBase interface
  StringRef getHierId() const override { return hierId; }
  void setHierId(std::string id) { hierId = id; }
  void print(llvm::raw_ostream &os) const override;
  Operation *getOp() const override { return loopOp; }

  void addInEdge(EdgeBase *edge) override { /* unused for loop nodes */
  }
  void addOutEdge(EdgeBase *edge) override { /* unused for loop nodes */
  }
  const DenseSet<EdgeBase *> &getInEdges() const override { return edges; }
  const DenseSet<EdgeBase *> &getOutEdges() const override { return edges; }

  NodeKind getKind() const override { return NodeKind::Loop; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::Loop;
  }

  //===--------------------------------------------------------------------===//
  // Induction Variable Analysis
  //===--------------------------------------------------------------------===//

  /// Get the induction variable for this loop
  Value getInductionVar() const;

  /// Check if a value depends on this loop's induction variable.
  /// Walks the def-use chain to find IV dependencies.
  bool dependsOnInductionVar(Value v);
  /// Check IV dependency after stripping numeric casts.
  bool dependsOnInductionVarNormalized(Value v);
  /// Check if a value is invariant with respect to this loop's body.
  bool isValueLoopInvariant(Value v);
  /// Check if a value depends on base after normalizing loop IV to its init.
  static bool dependsOnLoopInit(Value value, Value base);
  /// Normalized variant that strips numeric casts on inputs.
  static bool dependsOnLoopInitNormalized(Value value, Value base);

  /// Analyzed index expression relative to induction variable.
  /// Represents: index = iv * multiplier + offset
  struct IVExpr {
    bool dependsOnIV = false;
    std::optional<int64_t> offset;     // e.g., -1 for i-1, +1 for i+1
    std::optional<int64_t> multiplier; // e.g., 2 for 2*i
    bool isAnalyzable() const { return dependsOnIV && offset.has_value(); }
  };

  /// Analyze an index expression to extract IV relationship.
  /// Returns IVExpr with offset/multiplier if analyzable.
  IVExpr analyzeIndexExpr(Value index);

  /// Get constant lower bound if available
  std::optional<int64_t> getLowerBoundConstant() const;

  /// Get constant upper bound if available
  std::optional<int64_t> getUpperBoundConstant() const;

  /// Clear the IV dependency cache (call when IR changes)
  void clearIVCache() { ivDependencyCache.clear(); }

private:
  Operation *loopOp;
  std::string hierId;
  DenseSet<EdgeBase *> edges;

  /// Cache for IV dependency analysis to avoid repeated traversals
  DenseMap<Value, bool> ivDependencyCache;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_LOOPNODE_H
