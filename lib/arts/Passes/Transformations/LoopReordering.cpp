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
/// See LoopTransforms.cpp for iter_args patterns (e.g., gemm).
/// Must run BEFORE CreateDbs to preserve SSA relationships.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(loop_reordering);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Copy loop attributes (arts.id and arts.loop) from source to target
/// operation.
static void copyLoopAttributes(Operation *source, Operation *target) {
  if (!source || !target)
    return;

  /// Copy arts.id attribute
  if (auto idAttr = source->getAttr(AttrNames::Operation::ArtsId))
    target->setAttr(AttrNames::Operation::ArtsId, idAttr);

  /// Copy arts.loop attribute
  if (auto loopAttr = source->getAttr(AttrNames::LoopMetadata::Name))
    target->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
}

/// Clear hasReductions for loops that no longer contain reduction operations
/// (e.g., init loops created by loop distribution).
static Attribute sanitizeLoopMetadataForInitLoop(Attribute attr,
                                                 MLIRContext *ctx) {
  if (!attr || !ctx)
    return attr;
  auto loopAttr = dyn_cast<LoopMetadataAttr>(attr);
  if (!loopAttr)
    return attr;

  /// Init loops don't have reductions - clear the flag
  return LoopMetadataAttr::get(
      ctx,
      /*potentiallyParallel=*/loopAttr.getPotentiallyParallel(),
      /*hasReductions=*/BoolAttr::get(ctx, false),
      /*reductionKinds=*/ArrayAttr(),
      /*tripCount=*/loopAttr.getTripCount(),
      /*nestingLevel=*/loopAttr.getNestingLevel(),
      /*hasInterIterationDeps=*/BoolAttr::get(ctx, false),
      /*memrefsWithLoopCarriedDeps=*/loopAttr.getMemrefsWithLoopCarriedDeps(),
      /*parallelClassification=*/loopAttr.getParallelClassification(),
      /*locationKey=*/loopAttr.getLocationKey());
}

struct LoopReorderingPass
    : public arts::LoopReorderingBase<LoopReorderingPass> {
  LoopReorderingPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    auto &manager = AM->getMetadataManager();

    ARTS_INFO_HEADER(LoopReorderingPass);

    int reorderCount = 0;

    module.walk([&](ForOp artsFor) {
      /// First try metadata-based reordering
      bool metadataApplied = false;
      if (manager.ensureLoopMetadata(artsFor)) {
        auto *loopMeta = manager.getLoopMetadata(artsFor);
        if (loopMeta && !loopMeta->reorderNestTo.empty()) {
          ARTS_DEBUG("Found loop with reorder_nest_to: "
                     << loopMeta->reorderNestTo.size() << " loops");

          if (applyReordering(artsFor, loopMeta->reorderNestTo, manager)) {
            reorderCount++;
            ARTS_INFO("Applied loop reordering to arts.for at "
                      << loopMeta->locationMetadata.getKey());
            metadataApplied = true;
          }
        }
      }

      /// If metadata didn't apply, try auto-detection for matmul patterns
      if (!metadataApplied) {
        if (tryAutoDetectAndReorder(artsFor)) {
          reorderCount++;
        }
      }
    });

    ARTS_DEBUG("LoopReorderingPass: reordered " << reorderCount
                                                << " loop nests");
  }

  /// Try to auto-detect matmul-like patterns and apply loop interchange.
  /// Returns true if a transformation was applied.
  bool tryAutoDetectAndReorder(ForOp artsFor) {
    /// Look for the pattern: arts.for(i) { scf.for(j) { init; scf.for(k) {
    /// reduce } } }

    /// Find the first scf.for directly inside arts.for
    scf::ForOp jLoop = nullptr;
    for (Operation &op : artsFor.getBody()->without_terminator()) {
      if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
        jLoop = forOp;
        break;
      }
    }

    if (!jLoop)
      return false;

    /// Check if j loop has the pattern: init_ops followed by inner scf.for (k)
    SmallVector<Operation *, 8> initOps;
    scf::ForOp kLoop = nullptr;

    for (Operation &op : jLoop.getBody()->without_terminator()) {
      if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
        kLoop = forOp;
        break; /// Found the k loop
      }
      initOps.push_back(&op);
    }

    /// Need: init ops present, and a k loop
    if (initOps.empty() || !kLoop)
      return false;

    /// Additional heuristic: check if this looks like a reduction (k loop body
    /// has load-compute-store pattern with accumulation)
    if (!looksLikeReduction(kLoop))
      return false;

    ARTS_DEBUG("Auto-detected matmul pattern in arts.for: "
               << initOps.size() << " init ops, j-k nest");

    /// Apply the distribution + interchange
    return interchangeInnerLoopsWithDistribution(artsFor, jLoop, kLoop,
                                                 initOps);
  }

  /// Check if a loop body looks like a reduction (load-compute-store pattern)
  bool looksLikeReduction(scf::ForOp loop) {
    bool hasLoad = false;
    bool hasStore = false;
    bool hasArith = false;

    for (Operation &op : loop.getBody()->without_terminator()) {
      if (isa<memref::LoadOp, affine::AffineLoadOp>(op))
        hasLoad = true;
      if (isa<memref::StoreOp, affine::AffineStoreOp>(op))
        hasStore = true;
      if (isa<arith::AddFOp, arith::MulFOp, arith::AddIOp, arith::MulIOp>(op))
        hasArith = true;
    }

    /// A reduction typically has loads, arithmetic, and stores
    return hasLoad && hasStore && hasArith;
  }

private:
  ArtsAnalysisManager *AM;

  /// Apply the reordering specified by targetOrder (arts.ids in target order).
  /// Returns true if reordering was applied successfully.
  bool applyReordering(ForOp outerLoop, ArrayRef<int64_t> targetOrder,
                       ArtsMetadataManager &manager) {
    /// Collect all loops in the nest by arts.id
    DenseMap<int64_t, Operation *> loopById;
    SmallVector<Operation *, 4> loopsInCurrentOrder;

    /// First, add the outer arts.for
    if (auto idAttr = outerLoop->getAttrOfType<IntegerAttr>(
            AttrNames::Operation::ArtsId)) {
      loopById[idAttr.getInt()] = outerLoop.getOperation();
      loopsInCurrentOrder.push_back(outerLoop.getOperation());
    }

    /// Walk inner scf.for loops and collect them
    outerLoop->walk([&](scf::ForOp innerFor) {
      if (auto idAttr = innerFor->getAttrOfType<IntegerAttr>(
              AttrNames::Operation::ArtsId)) {
        loopById[idAttr.getInt()] = innerFor.getOperation();
        loopsInCurrentOrder.push_back(innerFor.getOperation());
      }
    });

    /// Build target order list
    SmallVector<Operation *, 4> targetLoops;
    for (int64_t id : targetOrder) {
      auto *loop = loopById.lookup(id);
      if (!loop) {
        ARTS_DEBUG("Loop reordering: missing loop with arts.id " << id);
        return false;
      }
      targetLoops.push_back(loop);
    }

    if (targetLoops.size() != loopsInCurrentOrder.size()) {
      ARTS_DEBUG("Loop reordering: size mismatch ("
                 << targetLoops.size() << " vs " << loopsInCurrentOrder.size()
                 << ")");
      return false;
    }

    /// Check if already in target order
    bool alreadyOrdered = true;
    for (size_t i = 0; i < targetLoops.size(); ++i) {
      if (targetLoops[i] != loopsInCurrentOrder[i]) {
        alreadyOrdered = false;
        break;
      }
    }
    if (alreadyOrdered) {
      ARTS_DEBUG("Loop reordering: already in target order");
      return false;
    }

    /// Determine which inner loops need swapping
    /// For arts.for with inner scf.for loops, we need to swap the inner loops
    /// The outer arts.for stays in place, we only reorder the inner scf.for
    /// loops

    /// For a 3-deep nest like: arts.for(i) -> scf.for(j) -> scf.for(k) -> body
    /// Target order [i, k, j] means we need: arts.for(i) -> scf.for(k) ->
    /// scf.for(j) -> body

    /// Simple case: 2 inner loops to swap
    if (targetLoops.size() == 3 && loopsInCurrentOrder.size() == 3) {
      /// loopsInCurrentOrder[0] = arts.for (outer, stays in place)
      /// loopsInCurrentOrder[1] = first inner scf.for
      /// loopsInCurrentOrder[2] = second inner scf.for (innermost)

      /// Check if we need to swap the inner two loops
      /// targetLoops[1] and targetLoops[2] are the desired order for inner
      /// loops
      scf::ForOp firstInner = dyn_cast<scf::ForOp>(loopsInCurrentOrder[1]);
      scf::ForOp secondInner = dyn_cast<scf::ForOp>(loopsInCurrentOrder[2]);

      if (!firstInner || !secondInner) {
        ARTS_DEBUG("Loop reordering: inner loops are not scf.for");
        return false;
      }

      /// Check if the target order wants them swapped
      if (targetLoops[1] == loopsInCurrentOrder[2] &&
          targetLoops[2] == loopsInCurrentOrder[1]) {
        /// Need to swap firstInner and secondInner
        return interchangeInnerLoops(outerLoop, firstInner, secondInner);
      }
    }

    /// For other cases, log and return false for now
    ARTS_DEBUG("Loop reordering: complex reordering not yet supported");
    return false;
  }

  /// Interchange two adjacent inner scf.for loops.
  /// Before: outerLoop { firstInner { secondInner { body } } }
  /// After:  outerLoop { secondInner { firstInner { body } } }
  bool interchangeInnerLoops(ForOp outerLoop, scf::ForOp firstInner,
                             scf::ForOp secondInner) {
    ARTS_DEBUG("Interchanging inner loops");

    /// Check if this is an imperfect nest (has init ops before inner loop)
    SmallVector<Operation *, 8> initOps;
    scf::ForOp innerLoopOp = nullptr;

    for (Operation &op : firstInner.getBody()->without_terminator()) {
      if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
        innerLoopOp = forOp;
        break; /// Found the inner loop, everything before is init
      }
      initOps.push_back(&op);
    }

    /// If there are init ops, use the distribution + interchange approach
    if (!initOps.empty() && innerLoopOp) {
      ARTS_DEBUG("Detected imperfect nest with "
                 << initOps.size() << " init ops - using distribution");
      return interchangeInnerLoopsWithDistribution(outerLoop, firstInner,
                                                   secondInner, initOps);
    }

    /// Perfect nest case - simple interchange
    return interchangeInnerLoopsPerfect(outerLoop, firstInner, secondInner);
  }

  /// Interchange two adjacent inner scf.for loops (perfect nest case).
  /// Before: outerLoop { firstInner { secondInner { body } } }
  /// After:  outerLoop { secondInner { firstInner { body } } }
  bool interchangeInnerLoopsPerfect(ForOp outerLoop, scf::ForOp firstInner,
                                    scf::ForOp secondInner) {
    ARTS_DEBUG("Interchanging inner loops (perfect nest)");

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

    /// Create new outer loop (was secondInner)
    auto newOuterLoop = builder.create<scf::ForOp>(
        firstInner.getLoc(), lb2, ub2, step2, ValueRange{},
        [&](OpBuilder &nestedBuilder, Location loc, Value newOuterIV,
            ValueRange iterArgs) {
          /// Create new inner loop (was firstInner)
          nestedBuilder.create<scf::ForOp>(
              loc, lb1, ub1, step1, ValueRange{},
              [&](OpBuilder &innerBuilder, Location innerLoc, Value newInnerIV,
                  ValueRange innerIterArgs) {
                /// Clone the body of secondInner, replacing IVs
                IRMapping mapping;
                mapping.map(iv1, newInnerIV); /// firstInner IV -> new inner IV
                mapping.map(iv2, newOuterIV); /// secondInner IV -> new outer IV

                /// Clone all operations from secondInner's body except yield
                for (Operation &op :
                     secondInner.getBody()->without_terminator()) {
                  innerBuilder.clone(op, mapping);
                }

                innerBuilder.create<scf::YieldOp>(innerLoc);
              });
          nestedBuilder.create<scf::YieldOp>(loc);
        });

    /// Copy attributes from original loops
    copyLoopAttributes(secondInner.getOperation(), newOuterLoop.getOperation());

    auto &innerLoops = newOuterLoop.getBody()->getOperations();
    for (Operation &op : innerLoops) {
      if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
        copyLoopAttributes(firstInner.getOperation(), forOp.getOperation());
        break;
      }
    }

    /// Replace firstInner with newOuterLoop
    firstInner->replaceAllUsesWith(newOuterLoop->getResults());
    firstInner->erase();

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

    /// STEP 1: Create the init loop (for j: init_ops)
    auto initLoop = builder.create<scf::ForOp>(
        firstInner.getLoc(), lb1, ub1, step1, ValueRange{},
        [&](OpBuilder &nestedBuilder, Location loc, Value initLoopIV,
            ValueRange iterArgs) {
          /// Clone init ops, mapping original j IV to new IV
          IRMapping mapping;
          mapping.map(iv1, initLoopIV);

          for (Operation *op : initOps) {
            nestedBuilder.clone(*op, mapping);
          }

          nestedBuilder.create<scf::YieldOp>(loc);
        });

    /// Copy j loop attributes to init loop (sanitize: init has no reductions)
    if (auto idAttr = firstInner->getAttr(AttrNames::Operation::ArtsId))
      initLoop->setAttr(AttrNames::Operation::ArtsId, idAttr);
    if (auto loopAttr = firstInner->getAttr(AttrNames::LoopMetadata::Name))
      initLoop->setAttr(
          AttrNames::LoopMetadata::Name,
          sanitizeLoopMetadataForInitLoop(loopAttr, firstInner->getContext()));

    /// STEP 2: Create the interchanged reduction loop (for k: for j: reduce)
    auto newOuterLoop = builder.create<scf::ForOp>(
        firstInner.getLoc(), lb2, ub2, step2, ValueRange{},
        [&](OpBuilder &nestedBuilder, Location loc, Value newOuterIV,
            ValueRange iterArgs) {
          /// Create new inner loop (was firstInner/j)
          nestedBuilder.create<scf::ForOp>(
              loc, lb1, ub1, step1, ValueRange{},
              [&](OpBuilder &innerBuilder, Location innerLoc, Value newInnerIV,
                  ValueRange innerIterArgs) {
                /// Clone the body of secondInner (k loop), replacing IVs
                IRMapping mapping;
                mapping.map(iv1, newInnerIV); /// j IV -> new inner IV
                mapping.map(iv2, newOuterIV); /// k IV -> new outer IV

                /// Clone all operations from secondInner's body except yield
                for (Operation &op :
                     secondInner.getBody()->without_terminator()) {
                  innerBuilder.clone(op, mapping);
                }

                innerBuilder.create<scf::YieldOp>(innerLoc);
              });
          nestedBuilder.create<scf::YieldOp>(loc);
        });

    /// Copy k loop attributes to new outer loop
    copyLoopAttributes(secondInner.getOperation(), newOuterLoop.getOperation());

    /// Copy j loop attributes to new inner loop
    auto &innerLoops = newOuterLoop.getBody()->getOperations();
    for (Operation &op : innerLoops) {
      if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
        copyLoopAttributes(firstInner.getOperation(), forOp.getOperation());
        break;
      }
    }

    ARTS_INFO("Loop distribution + interchange applied: "
              << "init loop + k-j reduction (was j-k)");

    /// Erase the original firstInner (which contained both init and k loop)
    firstInner->erase();

    return true;
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::arts::createLoopReorderingPass(ArtsAnalysisManager *AM) {
  return std::make_unique<LoopReorderingPass>(AM);
}
