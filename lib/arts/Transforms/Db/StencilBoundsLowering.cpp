///==========================================================================///
/// File: StencilBoundsLowering.cpp
///
/// Runtime stencil-bounds guard lowering for DbAcquireOps.
///==========================================================================///

#include "arts/transforms/db/StencilBoundsLowering.h"
#include "arts/ArtsDialect.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "arts/utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(stencil_bounds_lowering);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Check if op is in the else branch of `if (iv == 0)`, guaranteeing iv > 0.
static bool isLowerBoundGuaranteedByControlFlow(Operation *op, Value loopIV) {
  if (!loopIV)
    return false;

  auto matchesIV = [&loopIV](Value v) {
    if (v == loopIV)
      return true;
    if (auto cast = v.getDefiningOp<arith::IndexCastOp>())
      return cast.getIn() == loopIV;
    return false;
  };

  for (Operation *p = op->getParentOp(); p; p = p->getParentOp()) {
    auto ifOp = dyn_cast<scf::IfOp>(p);
    if (!ifOp || ifOp.getElseRegion().empty())
      continue;

    if (!ifOp.getElseRegion().isAncestor(op->getParentRegion()))
      continue;

    auto cmp = ifOp.getCondition().getDefiningOp<arith::CmpIOp>();
    if (!cmp || cmp.getPredicate() != arith::CmpIPredicate::eq)
      continue;

    Value lhs = cmp.getLhs(), rhs = cmp.getRhs();
    if ((matchesIV(lhs) && ValueUtils::isZeroConstant(rhs)) ||
        (matchesIV(rhs) && ValueUtils::isZeroConstant(lhs))) {
      ARTS_DEBUG("  Lower bound guaranteed by control flow (else of iv==0)");
      return true;
    }
  }
  return false;
}

/// Recreate DbAcquireOp with bounds_valid predicate.
static void applyBoundsValid(DbAcquireOp acquireOp,
                             ArrayRef<int64_t> boundsCheckFlags, Value loopIV) {
  if (acquireOp.hasMultiplePartitionEntries()) {
    ARTS_DEBUG("  Skipping bounds generation for multi-entry stencil acquire");
    return;
  }

  Location loc = acquireOp.getLoc();
  OpBuilder builder(acquireOp);

  auto indices = acquireOp.getIndices();
  SmallVector<Value> sourceSizes =
      DbUtils::getSizesFromDb(acquireOp.getSourcePtr());

  bool lowerBoundGuarded =
      isLowerBoundGuaranteedByControlFlow(acquireOp, loopIV);

  Value zero = arts::createZeroIndex(builder, loc);
  Value boundsValid;

  for (size_t i = 0; i < boundsCheckFlags.size() && i < indices.size(); ++i) {
    if (boundsCheckFlags[i] == 0 || i >= sourceSizes.size())
      continue;

    Value idx = indices[i];
    Value size = sourceSizes[i];
    Value dimValid;

    if (lowerBoundGuarded) {
      dimValid = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::slt,
                                               idx, size);
    } else {
      Value geZero = builder.create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::sge, idx, zero);
      Value ltSize = builder.create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::slt, idx, size);
      dimValid = builder.create<arith::AndIOp>(loc, geZero, ltSize);
    }

    boundsValid =
        boundsValid ? builder.create<arith::AndIOp>(loc, boundsValid, dimValid)
                    : dimValid;
  }

  if (!boundsValid)
    return;

  SmallVector<Value> indicesVec(indices.begin(), indices.end());
  /// Sanitize DB-space indices for guarded dims so out-of-bounds halo acquires
  /// never carry negative/overflowed indices into downstream lowering.
  /// bounds_valid still controls whether the acquire is logically active.
  for (size_t i = 0; i < boundsCheckFlags.size() && i < indicesVec.size();
       ++i) {
    if (boundsCheckFlags[i] == 0 || i >= sourceSizes.size())
      continue;

    Value idx = indicesVec[i];
    Value size = sourceSizes[i];
    Value geZero = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::sge,
                                                 idx, zero);
    Value nonNegative = builder.create<arith::SelectOp>(loc, geZero, idx, zero);

    Value one = arts::createOneIndex(builder, loc);
    Value hasExtent = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::sgt, size, zero);
    Value lastIdxRaw = builder.create<arith::SubIOp>(loc, size, one);
    Value lastIdx =
        builder.create<arith::SelectOp>(loc, hasExtent, lastIdxRaw, zero);
    Value clamped = builder.create<arith::MinUIOp>(loc, nonNegative, lastIdx);
    indicesVec[i] = clamped;
  }

  SmallVector<Value> offsetsVec(acquireOp.getOffsets().begin(),
                                acquireOp.getOffsets().end());
  SmallVector<Value> sizesVec(acquireOp.getSizes().begin(),
                              acquireOp.getSizes().end());
  SmallVector<Value> partIndices(acquireOp.getPartitionIndices().begin(),
                                 acquireOp.getPartitionIndices().end());
  SmallVector<Value> partOffsets(acquireOp.getPartitionOffsets().begin(),
                                 acquireOp.getPartitionOffsets().end());
  SmallVector<Value> partSizes(acquireOp.getPartitionSizes().begin(),
                               acquireOp.getPartitionSizes().end());

  auto partitionMode = getPartitionMode(acquireOp.getOperation());
  auto newAcquire = builder.create<DbAcquireOp>(
      loc, acquireOp.getMode(), acquireOp.getSourceGuid(),
      acquireOp.getSourcePtr(), partitionMode, indicesVec, offsetsVec, sizesVec,
      partIndices, partOffsets, partSizes, boundsValid);
  transferAttributes(acquireOp.getOperation(), newAcquire.getOperation(),
                     /*excludes=*/{});
  newAcquire.copyPartitionSegmentsFrom(acquireOp);

  acquireOp.getGuid().replaceAllUsesWith(newAcquire.getGuid());
  acquireOp.getPtr().replaceAllUsesWith(newAcquire.getPtr());
  acquireOp.erase();

  ARTS_DEBUG("Added bounds_valid to DbAcquireOp: " << newAcquire);
}

} // namespace

void mlir::arts::lowerStencilAcquireBounds(ModuleOp module,
                                           LoopAnalysis &loopAnalysis) {
  SmallVector<std::tuple<DbAcquireOp, SmallVector<int64_t>, Value>> toModify;

  module.walk([&](DbAcquireOp acquireOp) {
    if (acquireOp.getBoundsValid())
      return;

    auto indices = acquireOp.getIndices();
    if (indices.empty())
      return;

    SmallVector<LoopNode *> enclosingLoops;
    loopAnalysis.collectEnclosingLoops(acquireOp, enclosingLoops);
    if (enclosingLoops.empty())
      return;

    LoopNode *loopNode = enclosingLoops.back();

    SmallVector<int64_t> boundsCheckFlags;
    bool needsBoundsCheck = false;

    for (Value idx : indices) {
      int64_t flag = 0;
      LoopNode::IVExpr expr = loopNode->analyzeIndexExpr(idx);
      if (expr.isAnalyzable() && expr.offset.has_value() && *expr.offset != 0) {
        flag = 1;
        needsBoundsCheck = true;
        ARTS_DEBUG("  Index with offset " << *expr.offset
                                          << " needs bounds check");
      } else if (expr.dependsOnIV && !expr.isAnalyzable()) {
        flag = 1;
        needsBoundsCheck = true;
        ARTS_DEBUG("  Complex IV-dependent index needs bounds check");
      }
      boundsCheckFlags.push_back(flag);
    }

    if (needsBoundsCheck) {
      Value loopIV = loopNode->getInductionVar();
      toModify.push_back({acquireOp, std::move(boundsCheckFlags), loopIV});
    }
  });

  for (auto &[acquireOp, flags, loopIV] : toModify)
    applyBoundsValid(acquireOp, flags, loopIV);
}
