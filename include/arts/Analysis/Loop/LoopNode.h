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
  /// Induction Variable Analysis
  //===--------------------------------------------------------------------===//

  /// Get the induction variable for this loop
  Value getInductionVar() const;

  /// Utility functions
  bool dependsOnInductionVar(Value v);
  bool dependsOnInductionVarNormalized(Value v);
  bool isValueLoopInvariant(Value v);
  bool hasExplicitLoopMetadata() const;
  bool isParallelizableByMetadata() const;
  static bool dependsOnLoopInit(Value value, Value base);
  static bool dependsOnLoopInitNormalized(Value value, Value base);

  /// Analyzed index expression relative to induction variable.
  /// Represents: index = iv * multiplier + offset
  struct IVExpr {
    bool dependsOnIV = false;
    std::optional<int64_t> offset;     /// e.g., -1 for i-1, +1 for i+1
    std::optional<int64_t> multiplier; /// e.g., 2 for 2*i
    bool isAnalyzable() const { return dependsOnIV && offset.has_value(); }
  };

  /// Analyze an index expression to extract IV relationship.
  /// Returns IVExpr with offset/multiplier if analyzable.
  IVExpr analyzeIndexExpr(Value index);
  std::optional<int64_t> getLowerBoundConstant() const;
  std::optional<int64_t> getUpperBoundConstant() const;
  std::optional<int64_t> getStepConstant() const;
  int getNestingDepth() const;
  void clearIVCache() { ivDependencyCache.clear(); }

  //===--------------------------------------------------------------------===//
  /// Raw Value Accessors
  //===--------------------------------------------------------------------===//

  /// Get the lower bound value for this loop.
  Value getLowerBound() const;

  /// Get the upper bound value for this loop.
  Value getUpperBound() const;

  /// Get the step value for this loop.
  Value getStep() const;

  //===--------------------------------------------------------------------===//
  /// Loop Classification Methods
  //===--------------------------------------------------------------------===//

  /// Check if this loop is innermost (contains no nested loops of same type).
  bool isInnermostLoop() const;

  /// Check if this loop contains EDT operations (worker loop in ARTS).
  bool hasEdt() const;

  /// Check if two loops have compatible bounds for fusion (lower, upper, step).
  static bool haveCompatibleBounds(LoopNode *a, LoopNode *b);

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
