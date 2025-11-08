///===----------------------------------------------------------------------===///
// DependenceAnalyzer.h - Dependence Analysis Helper
///===----------------------------------------------------------------------===///

#ifndef ARTS_ANALYSIS_METADATA_DEPENDENCEANALYZER_H
#define ARTS_ANALYSIS_METADATA_DEPENDENCEANALYZER_H

#include "arts/Analysis/Metadata/AccessAnalyzer.h"
#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
// DependenceAnalyzer - Helper for analyzing loop dependencies
///===----------------------------------------------------------------------===///
class DependenceAnalyzer {
public:
  explicit DependenceAnalyzer(MLIRContext *context,
                              AccessAnalyzer &accessAnalyzer)
      : context(context), accessAnalyzer(accessAnalyzer) {}

  /// Check if a loop has inter-iteration dependencies
  bool hasInterIterationDeps(Operation *loopOp) const {
    /// For affine loops, use MLIR's analysis
    if (auto forOp = dyn_cast<affine::AffineForOp>(loopOp))
      return !affine::isLoopParallel(forOp);

    /// For SCF loops, check for loop-carried dependencies via iter_args
    if (auto forOp = dyn_cast<scf::ForOp>(loopOp))
      return !forOp.getInitArgs().empty();

    /// scf.parallel is explicitly parallel
    if (isa<scf::ParallelOp>(loopOp))
      return false;

    /// Conservative default
    return true;
  }

  /// Check if a memref has loop-carried dependencies in a loop
  bool hasLoopCarriedDeps(Operation *loopOp, Value memref) const {
    /// Simple heuristic:
    ///  If same memref is both read and written, assume dependency
    bool hasRead = false, hasWrite = false;

    loopOp->walk([&](Operation *op) {
      Value accessedMemref;
      bool isRead = false;

      if (auto load = dyn_cast<affine::AffineLoadOp>(op)) {
        accessedMemref = load.getMemRef();
        isRead = true;
      } else if (auto store = dyn_cast<affine::AffineStoreOp>(op)) {
        accessedMemref = store.getMemRef();
        isRead = false;
      } else if (auto load = dyn_cast<memref::LoadOp>(op)) {
        accessedMemref = load.getMemRef();
        isRead = true;
      } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
        accessedMemref = store.getMemRef();
        isRead = false;
      }

      if (accessedMemref == memref) {
        if (isRead)
          hasRead = true;
        else
          hasWrite = true;
      }
    });

    return hasRead && hasWrite;
  }

private:
  MLIRContext *context [[maybe_unused]];
  AccessAnalyzer &accessAnalyzer [[maybe_unused]];
};

} // namespace arts
} // namespace mlir

#endif /// ARTS_ANALYSIS_METADATA_DEPENDENCEANALYZER_H
