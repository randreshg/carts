///==========================================================================///
/// File: BlockLoopStripMining.cpp
///
/// Strip-mine innermost loops that access block-partitioned datablocks.
/// This is a late DB-aware cleanup that must run after DbPartitioning.
/// It removes per-iteration div/rem + db_ref overhead by introducing a
/// block loop and local index loop, reusing db_ref per block.
///
/// Example (simplified):
///   Before:
///     scf.for %i = %lb to %ub step 1 {
///       %blk = arith.divui %i, %B
///       %off = arith.remui %i, %B
///       %view = arts.db_ref %ptr[%blk]
///       memref.store %val, %view[%off]
///     }
///   After:
///     scf.for %b = 0 to %numBlocks step 1 {
///       %blockStart = %lb + %b * %B
///       %innerUpper = min(%B, %ub - %blockStart)
///       scf.for %j = 0 to %innerUpper step 1 {
///         %i = %blockStart + %j
///         %view = arts.db_ref %ptr[%b]
///         memref.store %val, %view[%j]
///       }
///     }
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/utils/Debug.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/Utils.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/STLExtras.h"

#include <cstdint>
#include <optional>

ARTS_DEBUG_SETUP(block_stripmine);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct LoopBlockInfo {
  Value blockSizeVal;
  std::optional<int64_t> blockSizeConst;
  std::optional<int64_t> lowerBoundConst;
  Value lowerBoundDivHint;
  Value offsetVal;
  Value offsetDivHint;
  SmallVector<arith::DivUIOp> divOps;
  SmallVector<Value> remValues;
  SmallVector<Operation *> remPatternOps;
};

static bool getConstIndex(Value v, int64_t &out) {
  if (!v)
    return false;
  if (auto folded =
          ValueAnalysis::tryFoldConstantIndex(ValueAnalysis::stripNumericCasts(v))) {
    out = *folded;
    return true;
  }
  return false;
}

static std::optional<Value> extractInvariantOffset(Value lhs, Value iv) {
  lhs = ValueAnalysis::stripNumericCasts(lhs);
  if (lhs == iv)
    return Value();
  if (auto addOp = lhs.getDefiningOp<arith::AddIOp>()) {
    Value lhsOp = ValueAnalysis::stripNumericCasts(addOp.getLhs());
    Value rhsOp = ValueAnalysis::stripNumericCasts(addOp.getRhs());
    if (lhsOp == iv && !ValueAnalysis::dependsOn(rhsOp, iv))
      return rhsOp;
    if (rhsOp == iv && !ValueAnalysis::dependsOn(lhsOp, iv))
      return lhsOp;
  }
  return std::nullopt;
}

static bool recordRemPattern(Value lhs, Value rhs, Value remResult, Value iv,
                             LoopBlockInfo &info,
                             ArrayRef<Operation *> patternOps) {
  auto offsetOpt = extractInvariantOffset(lhs, iv);
  if (!offsetOpt)
    return false;
  Value offset = *offsetOpt;
  if (offset) {
    if (!info.offsetVal)
      info.offsetVal = offset;
    else if (!ValueAnalysis::sameValue(info.offsetVal, offset))
      return false;
  } else if (info.offsetVal) {
    return false;
  }

  Value normalizedRhs = ValueAnalysis::stripNumericCasts(rhs);
  if (ValueAnalysis::dependsOn(normalizedRhs, iv))
    return false;
  auto rhsConst = ValueAnalysis::tryFoldConstantIndex(normalizedRhs);
  if (rhsConst && *rhsConst <= 1)
    return false;
  if (!info.blockSizeVal) {
    info.blockSizeVal = normalizedRhs;
    info.blockSizeConst = rhsConst;
  } else if (info.blockSizeVal != normalizedRhs) {
    if (!info.blockSizeConst || !rhsConst || *info.blockSizeConst != *rhsConst)
      return false;
  }

  info.remValues.push_back(remResult);
  llvm::append_range(info.remPatternOps, patternOps);
  return true;
}

static bool matchNormalizedSignedRemainder(arith::SelectOp selectOp, Value iv,
                                           LoopBlockInfo &info) {
  auto rem = selectOp.getFalseValue().getDefiningOp<arith::RemSIOp>();
  if (!rem)
    return false;
  auto cmp = selectOp.getCondition().getDefiningOp<arith::CmpIOp>();
  auto add = selectOp.getTrueValue().getDefiningOp<arith::AddIOp>();
  if (!cmp || !add)
    return false;
  if (cmp.getPredicate() != arith::CmpIPredicate::slt)
    return false;

  Value remResult = rem.getResult();
  if (!ValueAnalysis::sameValue(ValueAnalysis::stripNumericCasts(cmp.getLhs()),
                             remResult) ||
      !ValueAnalysis::isZeroConstant(ValueAnalysis::stripNumericCasts(cmp.getRhs())))
    return false;

  Value addLhs = ValueAnalysis::stripNumericCasts(add.getLhs());
  Value addRhs = ValueAnalysis::stripNumericCasts(add.getRhs());
  Value rhs = ValueAnalysis::stripNumericCasts(rem.getRhs());
  if (!ValueAnalysis::sameValue(addLhs, remResult) ||
      !ValueAnalysis::sameValue(addRhs, rhs))
    return false;

  SmallVector<Operation *> patternOps = {rem.getOperation(), cmp.getOperation(),
                                         add.getOperation(),
                                         selectOp.getOperation()};
  return recordRemPattern(rem.getLhs(), rem.getRhs(), selectOp.getResult(), iv,
                          info, patternOps);
}

static bool isAlignedOffset(Value offset, Value blockSize,
                            const std::optional<int64_t> &blockSizeConst,
                            Value *mulOther = nullptr) {
  if (!offset)
    return true;
  Value off = ValueAnalysis::stripClampOne(offset);
  Value bs = ValueAnalysis::stripClampOne(blockSize);
  if (auto offConst = ValueAnalysis::tryFoldConstantIndex(off)) {
    if (*offConst == 0)
      return true;
    if (blockSizeConst && *blockSizeConst > 0)
      return (*offConst % *blockSizeConst) == 0;
    return false;
  }
  if (auto mul = off.getDefiningOp<arith::MulIOp>()) {
    Value lhs = ValueAnalysis::stripClampOne(mul.getLhs());
    Value rhs = ValueAnalysis::stripClampOne(mul.getRhs());
    if (ValueAnalysis::sameValue(lhs, bs)) {
      if (mulOther)
        *mulOther = rhs;
      return true;
    }
    if (ValueAnalysis::sameValue(rhs, bs)) {
      if (mulOther)
        *mulOther = lhs;
      return true;
    }
  }
  return false;
}

static bool isAlignedValue(Value value, Value blockSize,
                           const std::optional<int64_t> &blockSizeConst,
                           Value *divHint = nullptr) {
  if (!value)
    return true;

  value = ValueAnalysis::stripClampOne(value);
  if (isAlignedOffset(value, blockSize, blockSizeConst, divHint))
    return true;

  if (auto add = value.getDefiningOp<arith::AddIOp>()) {
    Value lhs = ValueAnalysis::stripClampOne(add.getLhs());
    Value rhs = ValueAnalysis::stripClampOne(add.getRhs());
    return isAlignedValue(lhs, blockSize, blockSizeConst) &&
           isAlignedValue(rhs, blockSize, blockSizeConst);
  }

  return false;
}

static bool dominatesOrInAncestor(Value v, Operation *op,
                                  DominanceInfo &domInfo) {
  if (!v || !op)
    return false;
  if (Operation *def = v.getDefiningOp()) {
    Region *defRegion = def->getParentRegion();
    Region *opRegion = op->getParentRegion();
    if (defRegion && opRegion && defRegion != opRegion &&
        defRegion->isAncestor(opRegion))
      return true;
  } else {
    /// Block arguments from ancestor regions are considered in-scope.
    if (auto arg = v.dyn_cast<BlockArgument>()) {
      if (Region *argRegion = arg.getOwner()->getParent())
        if (Region *opRegion = op->getParentRegion())
          if (argRegion->isAncestor(opRegion))
            return true;
    }
  }
  return domInfo.dominates(v, op);
}

static std::optional<LoopBlockInfo> analyzeLoop(scf::ForOp loop,
                                                DominanceInfo &domInfo) {
  LoopBlockInfo info;
  Value iv = loop.getInductionVar();
  Value loopLowerBound = loop.getLowerBound();

  /// Require constant step = 1; allow dynamic upper bound.
  int64_t ub = 0, step = 0;
  if (!getConstIndex(loop.getStep(), step) || step != 1)
    return std::nullopt;
  info.lowerBoundConst = ValueAnalysis::tryFoldConstantIndex(loopLowerBound);
  if (getConstIndex(loop.getUpperBound(), ub) && info.lowerBoundConst &&
      ub <= *info.lowerBoundConst)
    return std::nullopt;

  bool invalid = false;
  loop.getBody()->walk([&](Operation *op) {
    if (auto div = dyn_cast<arith::DivUIOp>(op)) {
      auto offsetOpt = extractInvariantOffset(div.getLhs(), iv);
      if (!offsetOpt)
        return;
      Value offset = *offsetOpt;
      if (offset) {
        if (!info.offsetVal)
          info.offsetVal = offset;
        else if (!ValueAnalysis::sameValue(info.offsetVal, offset))
          invalid = true;
      } else if (info.offsetVal) {
        invalid = true;
      }
      Value rhs = ValueAnalysis::stripNumericCasts(div.getRhs());
      if (ValueAnalysis::dependsOn(rhs, iv))
        return;
      auto rhsConst = ValueAnalysis::tryFoldConstantIndex(rhs);
      if (rhsConst && *rhsConst <= 1)
        return;
      if (!info.blockSizeVal) {
        info.blockSizeVal = rhs;
        info.blockSizeConst = rhsConst;
      } else if (info.blockSizeVal != rhs) {
        if (!info.blockSizeConst || !rhsConst ||
            *info.blockSizeConst != *rhsConst)
          invalid = true;
      }
      info.divOps.push_back(div);
      return;
    }
    if (auto rem = dyn_cast<arith::RemUIOp>(op)) {
      if (!recordRemPattern(rem.getLhs(), rem.getRhs(), rem.getResult(), iv,
                            info, {rem.getOperation()}))
        invalid = true;
      return;
    }
    if (auto select = dyn_cast<arith::SelectOp>(op)) {
      if (matchNormalizedSignedRemainder(select, iv, info))
        return;
      return;
    }
    if (isa<arith::RemSIOp, arith::CmpIOp>(op)) {
      return;
    }
  });

  if (invalid || !info.blockSizeVal)
    return std::nullopt;
  if (info.divOps.empty() || info.remValues.empty())
    return std::nullopt;

  if (info.blockSizeConst && *info.blockSizeConst <= 1)
    return std::nullopt;
  if (!info.blockSizeConst &&
      !dominatesOrInAncestor(info.blockSizeVal, loop, domInfo))
    return std::nullopt;
  if (!info.lowerBoundConst &&
      !dominatesOrInAncestor(loopLowerBound, loop, domInfo))
    return std::nullopt;
  if (info.offsetVal) {
    if (!dominatesOrInAncestor(info.offsetVal, loop, domInfo))
      return std::nullopt;
    if (!isAlignedOffset(info.offsetVal, info.blockSizeVal, info.blockSizeConst,
                         &info.offsetDivHint))
      return std::nullopt;
  }
  if (!info.lowerBoundConst &&
      !isAlignedValue(loopLowerBound, info.blockSizeVal, info.blockSizeConst,
                      &info.lowerBoundDivHint))
    return std::nullopt;
  if (info.lowerBoundConst && info.blockSizeConst &&
      (*info.lowerBoundConst % *info.blockSizeConst) != 0)
    return std::nullopt;
  if (info.offsetVal) {
    auto offsetConst = ValueAnalysis::tryFoldConstantIndex(info.offsetVal);
    if (info.lowerBoundConst && offsetConst && info.blockSizeConst) {
      int64_t effectiveLower = *info.lowerBoundConst + *offsetConst;
      if ((effectiveLower % *info.blockSizeConst) != 0)
        return std::nullopt;
    } else if (!isAlignedValue(info.offsetVal, info.blockSizeVal,
                               info.blockSizeConst)) {
      return std::nullopt;
    }
  }

  /// Require at least one db_ref that depends on a matching div op for block
  /// index.
  bool hasDbRef = false;
  loop.getBody()->walk([&](DbRefOp ref) {
    for (Value idx : ref.getIndices()) {
      Value base = ValueAnalysis::stripNumericCasts(idx);
      for (auto div : info.divOps) {
        if (ValueAnalysis::dependsOn(base, div.getResult())) {
          hasDbRef = true;
          return;
        }
      }
    }
  });

  if (!hasDbRef)
    return std::nullopt;

  return info;
}

static bool stripMineLoop(scf::ForOp loop, const LoopBlockInfo &info) {
  Location loc = loop.getLoc();
  OpBuilder builder(loop);
  int64_t ub = 0, step = 0;
  if (!getConstIndex(loop.getStep(), step) || step != 1)
    return false;

  if (info.blockSizeConst && *info.blockSizeConst <= 1)
    return false;

  Operation *origTerminator =
      loop.getBody()->empty() ? nullptr : &loop.getBody()->back();
  auto origYield = dyn_cast_or_null<scf::YieldOp>(origTerminator);
  if (!origYield)
    return false;

  Value zero = arts::createZeroIndex(builder, loc);
  Value one = arts::createOneIndex(builder, loc);
  Value lbVal = ValueAnalysis::ensureIndexType(loop.getLowerBound(), builder, loc);
  Value ubVal = loop.getUpperBound();
  Value bsVal =
      info.blockSizeConst
          ? arts::createConstantIndex(builder, loc, *info.blockSizeConst)
          : ValueAnalysis::ensureIndexType(info.blockSizeVal, builder, loc);
  Value ubGeLb = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                               ubVal, lbVal);
  Value tripCountRaw = builder.create<arith::SubIOp>(loc, ubVal, lbVal);
  Value tripCount =
      builder.create<arith::SelectOp>(loc, ubGeLb, tripCountRaw, zero);
  Value bsMinusOne = builder.create<arith::SubIOp>(loc, bsVal, one);
  Value tripPlus = builder.create<arith::AddIOp>(loc, tripCount, bsMinusOne);
  Value numBlocksVal = builder.create<arith::DivUIOp>(loc, tripPlus, bsVal);
  if (info.blockSizeConst && info.lowerBoundConst &&
      getConstIndex(loop.getUpperBound(), ub)) {
    int64_t tripCountConst = ub - *info.lowerBoundConst;
    if (tripCountConst <= *info.blockSizeConst)
      return false;
    int64_t numBlocksConst =
        (tripCountConst + *info.blockSizeConst - 1) / *info.blockSizeConst;
    if (numBlocksConst <= 1)
      return false;
  }

  /// Create outer block loop.
  auto outer = builder.create<scf::ForOp>(loc, zero, numBlocksVal, one,
                                          loop.getInitArgs());

  builder.setInsertionPointToStart(outer.getBody());
  Value blockStart = builder.create<arith::AddIOp>(
      loc, lbVal,
      builder.create<arith::MulIOp>(loc, outer.getInductionVar(), bsVal));

  /// innerUpper = min(bs, max(0, ub - blockStart))
  Value remaining = builder.create<arith::SubIOp>(loc, ubVal, blockStart);
  Value clampCond = builder.create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::uge, blockStart, ubVal);
  Value remainingNonNeg =
      builder.create<arith::SelectOp>(loc, clampCond, zero, remaining);
  Value innerUpper =
      builder.create<arith::MinUIOp>(loc, remainingNonNeg, bsVal);

  /// Create inner local loop, carrying iter_args from outer.
  auto inner = builder.create<scf::ForOp>(loc, zero, innerUpper, one,
                                          outer.getRegionIterArgs());

  /// Prepare mapping and clone body.
  builder.setInsertionPointToStart(inner.getBody());
  Value global =
      builder.create<arith::AddIOp>(loc, blockStart, inner.getInductionVar());

  IRMapping mapping;
  mapping.map(loop.getInductionVar(), global);

  /// Map iter args.
  auto oldArgs = loop.getRegionIterArgs();
  auto newArgs = inner.getRegionIterArgs();
  for (auto it : llvm::zip(oldArgs, newArgs)) {
    mapping.map(std::get<0>(it), std::get<1>(it));
  }

  /// Map div/rem results to outer/inner ivs.
  Value blockIdx = outer.getInductionVar();
  Value effectiveLowerDiv = nullptr;
  if (info.lowerBoundDivHint) {
    effectiveLowerDiv =
        ValueAnalysis::ensureIndexType(info.lowerBoundDivHint, builder, loc);
  } else if (info.lowerBoundConst && info.blockSizeConst) {
    int64_t lowerDivConst = *info.lowerBoundConst / *info.blockSizeConst;
    effectiveLowerDiv = arts::createConstantIndex(builder, loc, lowerDivConst);
  } else {
    effectiveLowerDiv = builder.create<arith::DivUIOp>(loc, lbVal, bsVal);
  }
  if (info.offsetVal) {
    Value offsetDiv = nullptr;
    if (info.offsetDivHint) {
      offsetDiv = ValueAnalysis::ensureIndexType(info.offsetDivHint, builder, loc);
    } else if (auto offConst =
                   ValueAnalysis::tryFoldConstantIndex(info.offsetVal)) {
      if (info.blockSizeConst) {
        int64_t offDivConst = *offConst / *info.blockSizeConst;
        offsetDiv = arts::createConstantIndex(builder, loc, offDivConst);
      }
    }
    if (!offsetDiv) {
      Value offsetIdx =
          ValueAnalysis::ensureIndexType(info.offsetVal, builder, loc);
      offsetDiv = builder.create<arith::DivUIOp>(loc, offsetIdx, bsVal);
    }
    effectiveLowerDiv =
        builder.create<arith::AddIOp>(loc, effectiveLowerDiv, offsetDiv);
  }
  auto effectiveLowerDivConst =
      ValueAnalysis::tryFoldConstantIndex(effectiveLowerDiv);
  if (!effectiveLowerDivConst || *effectiveLowerDivConst != 0)
    blockIdx = builder.create<arith::AddIOp>(loc, blockIdx, effectiveLowerDiv);
  for (auto div : info.divOps)
    mapping.map(div.getResult(), blockIdx);
  for (Value rem : info.remValues)
    mapping.map(rem, inner.getInductionVar());

  /// Clone body ops (excluding terminator) into inner loop.
  for (Operation &op : loop.getBody()->without_terminator()) {
    if (auto div = dyn_cast<arith::DivUIOp>(&op)) {
      if (llvm::is_contained(info.divOps, div))
        continue;
    }
    if (llvm::is_contained(info.remPatternOps, &op))
      continue;
    builder.clone(op, mapping);
  }

  /// Replace inner loop terminator with mapped yield.
  scf::YieldOp innerYield = nullptr;
  for (Operation &op : *inner.getBody()) {
    if (auto y = dyn_cast<scf::YieldOp>(&op)) {
      innerYield = y;
      break;
    }
  }
  if (innerYield)
    innerYield.erase();
  builder.setInsertionPointToEnd(inner.getBody());

  SmallVector<Value> newYieldOperands;
  newYieldOperands.reserve(origYield.getNumOperands());
  for (Value v : origYield.getOperands()) {
    newYieldOperands.push_back(mapping.lookupOrDefault(v));
  }
  builder.create<scf::YieldOp>(loc, newYieldOperands);

  /// Replace outer loop terminator with inner results.
  scf::YieldOp outerYield = nullptr;
  for (Operation &op : *outer.getBody()) {
    if (auto y = dyn_cast<scf::YieldOp>(&op)) {
      outerYield = y;
      break;
    }
  }
  if (outerYield)
    outerYield.erase();
  builder.setInsertionPointToEnd(outer.getBody());
  builder.create<scf::YieldOp>(loc, inner.getResults());

  /// Replace uses of original loop with new outer loop results.
  loop.replaceAllUsesWith(outer.getResults());
  loop.erase();

  return true;
}

struct BlockLoopStripMiningPass
    : public PassWrapper<BlockLoopStripMiningPass,
                         OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(BlockLoopStripMiningPass)

  void runOnOperation() override {
    func::FuncOp func = getOperation();
    DominanceInfo domInfo(func);

    SmallVector<scf::ForOp> loops;
    func.walk([&](scf::ForOp loop) { loops.push_back(loop); });

    /// Process inner loops first to avoid invalidating parents.
    for (auto it = loops.rbegin(); it != loops.rend(); ++it) {
      scf::ForOp loop = *it;
      if (!loop)
        continue;
      if (!isInnermostLoop(loop))
        continue;

      auto info = analyzeLoop(loop, domInfo);
      if (!info)
        continue;

      if (stripMineLoop(loop, *info))
        ARTS_DEBUG("Applied block strip-mining to loop");
    }
  }
};

} // namespace

namespace mlir {
namespace arts {
/// Create the block loop strip-mining pass used to reduce per-iteration
/// div/rem + db_ref overhead for block-partitioned access patterns.
std::unique_ptr<Pass> createBlockLoopStripMiningPass() {
  return std::make_unique<BlockLoopStripMiningPass>();
}
/// End block loop strip-mining pass creation.
} // namespace arts
} // namespace mlir
