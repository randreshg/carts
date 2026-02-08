///==========================================================================///
/// File: LoopAnalysis.h
///
/// LoopAnalysis manages loop operations and their associated metadata.
/// Owns LoopNode objects which combine graph structure with LoopMetadata.
///==========================================================================///

#ifndef CARTS_ANALYSIS_LOOP_LOOPANALYSIS_H
#define CARTS_ANALYSIS_LOOP_LOOPANALYSIS_H

#include "arts/Analysis/ArtsAnalysis.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// LoopAnalysis
/// Manages all loop operations in the module and provides LoopNode objects
/// that combine graph structure with rich loop metadata.
///===----------------------------------------------------------------------===///
class ArtsAnalysisManager;
class LoopAnalysis : public ArtsAnalysis {
public:
  explicit LoopAnalysis(ArtsAnalysisManager &analysisManager);
  ~LoopAnalysis() = default;

  /// Build analysis - creates LoopNodes for all loops in module
  void run();
  void invalidate() override;

  ///===--------------------------------------------------------------------===///
  /// LoopNode Access
  ///===--------------------------------------------------------------------===///

  /// Get or create a LoopNode for any loop operation
  /// Returns nullptr if operation is not a recognized loop
  LoopNode *getOrCreateLoopNode(Operation *loopOp);

  /// Get existing LoopNode (returns nullptr if not found)
  LoopNode *getLoopNode(Operation *loopOp) const;

  /// Collect enclosing LoopNodes for a given operation
  void collectEnclosingLoops(Operation *op,
                             SmallVectorImpl<LoopNode *> &enclosingLoops);

  /// Collect all LoopNodes within an operation subtree
  void collectLoopsInOperation(Operation *op,
                               SmallVectorImpl<LoopNode *> &loops);

  /// Collect scf::ForOp LoopNodes within an operation subtree
  void collectForLoopsInOperation(Operation *op,
                                  SmallVectorImpl<LoopNode *> &loops);

  /// Collect all LoopNodes within an operation subtree, filtered by loop type
  template <typename LoopOpType>
  void collectLoopsInOperation(Operation *op,
                               SmallVectorImpl<LoopNode *> &loops);

  /// Resolve static trip count for a loop operation when possible.
  /// Uses LoopNode metadata first, then operation attributes/constant bounds.
  std::optional<int64_t> getStaticTripCount(Operation *loopOp) const;

private:
  ModuleOp module;

  DenseMap<Operation *, std::unique_ptr<LoopNode>> loopNodes;

  /// Helper: Check if an operation is a recognized loop
  bool isLoopOperation(Operation *op) const;
};

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_LOOP_LOOPANALYSIS_H
