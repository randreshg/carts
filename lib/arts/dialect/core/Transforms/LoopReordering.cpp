///==========================================================================///
/// LoopReordering.cpp - Cache-optimal loop interchange for matmul patterns
///
/// Transforms j-k loops to k-j order for stride-1 B[k,j] access.
/// Handles patterns with explicit init ops (e.g., 2mm, 3mm).
///
/// Example (2mm):
///
///   for j:                         for j: C[i,j] = 0      /// init distributed
///     C[i,j] = 0          =>       for k:
///     for k: C += A*B                for j: C += A*B      /// B[k,j] stride-1
///
/// See KernelTransforms.cpp for iter_args/update-form kernel rewrites
/// (e.g., gemm).
/// Must run BEFORE CreateDbs to preserve SSA relationships.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/dialect/core/Analysis/AnalysisDependencies.h"
#include "arts/dialect/core/Analysis/AnalysisManager.h"
#include "arts/dialect/core/Analysis/db/DbPatternMatchers.h"
#define GEN_PASS_DEF_LOOPREORDERING
#include "arts/dialect/core/Transforms/loop/LoopNormalizer.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include <functional>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(loop_reordering);

using namespace mlir;
using namespace mlir::arts;

static const AnalysisKind kLoopReordering_reads[] = {
    AnalysisKind::LoopAnalysis};
[[maybe_unused]] static const AnalysisDependencyInfo kLoopReordering_deps = {
    kLoopReordering_reads, {}};

namespace {

struct LoopReorderingPass
    : public impl::LoopReorderingBase<LoopReorderingPass> {
  LoopReorderingPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    ARTS_INFO_HEADER(LoopReorderingPass);

    int reorderCount = 0;

    module.walk([&](ForOp artsFor) {
      if (tryAutoDetectAndReorder(artsFor))
        reorderCount++;
    });

    ARTS_DEBUG("LoopReorderingPass: reordered " << reorderCount
                                                << " loop nests");
  }

  /// Try to auto-detect matmul-like patterns and apply loop interchange.
  /// Returns true if a transformation was applied.
  bool tryAutoDetectAndReorder(ForOp artsFor) {
    if (auto depPattern = getEffectiveDepPattern(artsFor.getOperation());
        depPattern && *depPattern != ArtsDepPattern::unknown)
      return false;

    MatmulInitReductionLoopMatch loopMatch;
    if (!detectMatmulInitReductionLoopNest(artsFor, &AM->getLoopAnalysis(),
                                           loopMatch))
      return false;

    ARTS_DEBUG("Auto-detected matmul pattern in arts.for: "
               << loopMatch.initOps.size() << " init ops, j-k nest");

    /// Apply the distribution + interchange
    return interchangeInnerLoopsWithDistribution(
        artsFor, loopMatch.jLoop, loopMatch.kLoop, loopMatch.initOps);
  }

private:
  mlir::arts::AnalysisManager *AM;

  /// Collect pure prefix ops from firstInner that are used by secondInner's
  /// body. These must be rematerialized into the rebuilt loop nest before the
  /// original firstInner is erased. If secondInner depends on a side-effecting
  /// prefix op, the interchange is not structurally safe.
  bool collectRematerializablePrefixOps(ArrayRef<Operation *> prefixOps,
                                        scf::ForOp secondInner,
                                        SmallVectorImpl<Operation *> &out) {
    DenseSet<Operation *> prefixSet(prefixOps.begin(), prefixOps.end());
    DenseSet<Operation *> needed;
    bool supported = true;

    std::function<void(Value)> visitValue = [&](Value value) {
      if (!supported)
        return;

      Operation *defOp = value.getDefiningOp();
      if (!defOp || !prefixSet.contains(defOp))
        return;
      if (!needed.insert(defOp).second)
        return;
      if (!isMemoryEffectFree(defOp)) {
        supported = false;
        return;
      }

      for (Value operand : defOp->getOperands())
        visitValue(operand);
    };

    secondInner.walk([&](Operation *op) {
      for (Value operand : op->getOperands())
        visitValue(operand);
    });

    if (!supported)
      return false;

    for (Operation *op : prefixOps) {
      if (needed.contains(op))
        out.push_back(op);
    }
    return true;
  }

  /// Interchange inner scf.for loops with loop distribution for imperfect
  /// nests.
  ///
  /// BEFORE (imperfect nest):
  ///   arts.for(i) {
  ///     scf.for(j) {
  ///       init: E[i][j] = 0
  ///       scf.for(k) { reduce: E[i][j] += ... }
  ///     }
  ///   }
  ///
  /// AFTER (distributed + interchanged):
  ///   arts.for(i) {
  ///     scf.for(j) { E[i][j] = 0 }           /// init loop (distributed)
  ///     scf.for(k) {                          /// NEW outer
  ///       scf.for(j) { E[i][j] += ... }       /// NEW inner (interchanged)
  ///     }
  ///   }
  bool interchangeInnerLoopsWithDistribution(ForOp outerLoop,
                                             scf::ForOp firstInner,
                                             scf::ForOp secondInner,
                                             ArrayRef<Operation *> initOps) {
    ARTS_DEBUG("Interchanging inner loops with distribution");

    OpBuilder builder(firstInner);

    /// Get the bounds and step of both loops
    Value lb1 = firstInner.getLowerBound();
    Value ub1 = firstInner.getUpperBound();
    Value step1 = firstInner.getStep();
    Value iv1 = firstInner.getInductionVar();

    Value lb2 = secondInner.getLowerBound();
    Value ub2 = secondInner.getUpperBound();
    Value step2 = secondInner.getStep();
    Value iv2 = secondInner.getInductionVar();

    /// Check that secondInner is directly inside firstInner
    if (secondInner->getParentOp() != firstInner.getOperation()) {
      ARTS_DEBUG("Loop reordering: loops are not directly nested");
      return false;
    }

    SmallVector<Operation *, 8> rematerializedPrefixOps;
    if (!collectRematerializablePrefixOps(initOps, secondInner,
                                          rematerializedPrefixOps)) {
      ARTS_DEBUG("Loop reordering: reduction body captures side-effecting "
                 "prefix ops");
      return false;
    }

    bool needsInitLoop = llvm::any_of(
        initOps, [](Operation *op) { return !isMemoryEffectFree(op); });

    if (needsInitLoop) {
      /// STEP 1: Create the init loop (for j: init_ops)
      scf::ForOp::create(builder, firstInner.getLoc(), lb1, ub1, step1,
                         ValueRange{},
                         [&](OpBuilder &nestedBuilder, Location loc,
                             Value initLoopIV, ValueRange iterArgs) {
                           /// Clone init ops, mapping original j IV to new IV
                           IRMapping mapping;
                           mapping.map(iv1, initLoopIV);

                           for (Operation *op : initOps)
                             nestedBuilder.clone(*op, mapping);

                           scf::YieldOp::create(nestedBuilder, loc);
                         });
    }

    /// STEP 2: Create the interchanged reduction loop (for k: for j: reduce)
    auto newOuterLoop = scf::ForOp::create(
        builder, firstInner.getLoc(), lb2, ub2, step2, ValueRange{},
        [&](OpBuilder &nestedBuilder, Location loc, Value newOuterIV,
            ValueRange iterArgs) {
          /// Create new inner loop (was firstInner/j)
          scf::ForOp::create(
              nestedBuilder, loc, lb1, ub1, step1, ValueRange{},
              [&](OpBuilder &innerBuilder, Location innerLoc, Value newInnerIV,
                  ValueRange innerIterArgs) {
                /// Clone the body of secondInner (k loop), replacing IVs
                IRMapping mapping;
                mapping.map(iv1, newInnerIV); /// j IV -> new inner IV
                mapping.map(iv2, newOuterIV); /// k IV -> new outer IV

                for (Operation *op : rematerializedPrefixOps)
                  innerBuilder.clone(*op, mapping);

                /// Clone all operations from secondInner's body except yield
                for (Operation &op :
                     secondInner.getBody()->without_terminator()) {
                  innerBuilder.clone(op, mapping);
                }

                scf::YieldOp::create(innerBuilder, innerLoc);
              });
          scf::YieldOp::create(nestedBuilder, loc);
        });

    scf::ForOp newInnerLoop = nullptr;
    auto &innerLoops = newOuterLoop.getBody()->getOperations();
    for (Operation &op : innerLoops) {
      if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
        newInnerLoop = forOp;
        break;
      }
    }
    if (!newInnerLoop) {
      ARTS_DEBUG("Loop reordering: missing rebuilt reduction loop");
      return false;
    }

    copyPatternAttrs(secondInner.getOperation(), newOuterLoop.getOperation());
    copyPatternAttrs(firstInner.getOperation(), newInnerLoop.getOperation());

    ARTS_INFO("Loop distribution + interchange applied: "
              << "init loop + k-j reduction (was j-k)");

    /// Erase the original firstInner (which contained both init and k loop)
    firstInner->erase();

    return true;
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::arts::createLoopReorderingPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<LoopReorderingPass>(AM);
}
