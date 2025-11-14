///==========================================================================///
/// File: LoopAnalysis.h
///
/// LoopAnalysis manages loop operations and their associated metadata.
/// Owns LoopNode objects which combine graph structure with LoopMetadata.
///==========================================================================///

#ifndef CARTS_ANALYSIS_LOOP_LOOPANALYSIS_H
#define CARTS_ANALYSIS_LOOP_LOOPANALYSIS_H

#include "arts/Analysis/Loop/LoopNode.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// LoopInfo - Basic loop information
///===----------------------------------------------------------------------===///
struct LoopInfo {
  bool isAffine;
  affine::AffineForOp affine;
  scf::ParallelOp scfParallel;
  scf::ForOp scfFor;
  Value inductionVar;

  LoopInfo(bool isAffine, affine::AffineForOp affine,
           scf::ParallelOp scfParallel, scf::ForOp scfFor, Value inductionVar)
      : isAffine(isAffine), affine(affine), scfParallel(scfParallel),
        scfFor(scfFor), inductionVar(inductionVar) {}
};

///===----------------------------------------------------------------------===///
/// LoopAnalysis
/// Manages all loop operations in the module and provides LoopNode objects
/// that combine graph structure with rich loop metadata.
///===----------------------------------------------------------------------===///
class ArtsAnalysisManager;
class LoopAnalysis {
public:
  explicit LoopAnalysis(Operation *module,
                        ArtsAnalysisManager *analysisManager = nullptr);
  ~LoopAnalysis();

  /// Build analysis - creates LoopNodes for all loops in module
  void run();

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

  /// Collect LoopNodes whose induction variables affect the given operation
  void collectAffectingLoops(Operation *op,
                             SmallVectorImpl<LoopNode *> &affectingLoops);

  /// Get the analysis manager
  ArtsAnalysisManager *getAnalysisManager() const { return analysisManager; }

private:
  ModuleOp module;
  ArtsAnalysisManager *analysisManager = nullptr;

  /// THE KEY: Map from loop operation → LoopNode (owns metadata)
  /// This is the single source of truth for all loop information
  DenseMap<Operation *, std::unique_ptr<LoopNode>> loopNodes;

  /// Legacy map: Operation → LoopInfo (for backward compatibility)
  DenseMap<Operation *, LoopInfo *> loopInfoMap;

  /// Quick list of all loop operations
  SmallVector<Operation *, 4> loops;

  /// Maps values to loops they depend on
  DenseMap<Value, SmallVector<Operation *, 4>> loopValsMap;

  /// Helper: Analyze induction variables
  void analyzeLoopIV();

  /// Helper: Check if an operation is a recognized loop
  bool isLoopOperation(Operation *op) const;
};

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_LOOP_LOOPANALYSIS_H
