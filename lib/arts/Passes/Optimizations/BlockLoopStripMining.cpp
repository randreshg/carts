///==========================================================================///
/// File: BlockLoopStripMining.cpp
///
/// Strip-mine innermost loops that access block-partitioned datablocks.
/// This removes per-iteration div/rem + db_ref overhead by introducing a
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

#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ValueUtils.h"
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
  int64_t lbConst = 0;
  Value offsetVal;
  Value offsetDivHint;
  SmallVector<arith::DivUIOp> divOps;
  SmallVector<arith::RemUIOp> remOps;
};

static bool getConstIndex(Value v, int64_t &out) {
  if (!v)
    return false;
  if (auto folded =
          ValueUtils::tryFoldConstantIndex(ValueUtils::stripNumericCasts(v))) {
    out = *folded;
    return true;
  }
  return false;
}

static bool isInnermostLoop(scf::ForOp loop) {
  bool hasNested = false;
  loop.getBody()->walk([&](scf::ForOp nested) {
    if (nested != loop)
      hasNested = true;
  });
  return !hasNested;
}

static std::optional<Value> extractInvariantOffset(Value lhs, Value iv) {
  lhs = ValueUtils::stripNumericCasts(lhs);
  if (lhs == iv)
    return Value();
  if (auto addOp = lhs.getDefiningOp<arith::AddIOp>()) {
    Value lhsOp = ValueUtils::stripNumericCasts(addOp.getLhs());
    Value rhsOp = ValueUtils::stripNumericCasts(addOp.getRhs());
    if (lhsOp == iv && !ValueUtils::dependsOn(rhsOp, iv))
      return rhsOp;
    if (rhsOp == iv && !ValueUtils::dependsOn(lhsOp, iv))
      return lhsOp;
  }
  return std::nullopt;
}

static bool isAlignedOffset(Value offset, Value blockSize,
                            const std::optional<int64_t> &blockSizeConst,
                            Value *mulOther = nullptr) {
  if (!offset)
    return true;
  Value off = ValueUtils::stripClampOne(offset);
  Value bs = ValueUtils::stripClampOne(blockSize);
  if (auto offConst = ValueUtils::tryFoldConstantIndex(off)) {
    if (*offConst == 0)
      return true;
    if (blockSizeConst && *blockSizeConst > 0)
      return (*offConst % *blockSizeConst) == 0;
    return false;
  }
  if (auto mul = off.getDefiningOp<arith::MulIOp>()) {
    Value lhs = ValueUtils::stripClampOne(mul.getLhs());
    Value rhs = ValueUtils::stripClampOne(mul.getRhs());
    if (ValueUtils::sameValue(lhs, bs)) {
      if (mulOther)
        *mulOther = rhs;
      return true;
    }
    if (ValueUtils::sameValue(rhs, bs)) {
      if (mulOther)
        *mulOther = lhs;
      return true;
    }
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

  int64_t lb = 0;
  if (!getConstIndex(loop.getLowerBound(), lb))
    return std::nullopt;
  info.lbConst = lb;

  /// Require constant step = 1; allow dynamic upper bound.
  int64_t ub = 0, step = 0;
  if (!getConstIndex(loop.getStep(), step) || step != 1)
    return std::nullopt;
  if (getConstIndex(loop.getUpperBound(), ub) && ub <= lb)
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
        else if (!ValueUtils::sameValue(info.offsetVal, offset))
          invalid = true;
      } else if (info.offsetVal) {
        invalid = true;
      }
      Value rhs = ValueUtils::stripNumericCasts(div.getRhs());
      if (ValueUtils::dependsOn(rhs, iv))
        return;
      auto rhsConst = ValueUtils::tryFoldConstantIndex(rhs);
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
      auto offsetOpt = extractInvariantOffset(rem.getLhs(), iv);
      if (!offsetOpt)
        return;
      Value offset = *offsetOpt;
      if (offset) {
        if (!info.offsetVal)
          info.offsetVal = offset;
        else if (!ValueUtils::sameValue(info.offsetVal, offset))
          invalid = true;
      } else if (info.offsetVal) {
        invalid = true;
      }
      Value rhs = ValueUtils::stripNumericCasts(rem.getRhs());
      if (ValueUtils::dependsOn(rhs, iv))
        return;
      auto rhsConst = ValueUtils::tryFoldConstantIndex(rhs);
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
      info.remOps.push_back(rem);
      return;
    }
  });

  if (invalid || !info.blockSizeVal)
    return std::nullopt;
  if (info.divOps.empty() || info.remOps.empty())
    return std::nullopt;

  if (info.blockSizeConst && *info.blockSizeConst <= 1)
    return std::nullopt;
  if (lb != 0) {
    if (!info.blockSizeConst)
      return std::nullopt;
    if ((lb % *info.blockSizeConst) != 0)
      return std::nullopt;
  }
  if (!info.blockSizeConst &&
      !dominatesOrInAncestor(info.blockSizeVal, loop, domInfo))
    return std::nullopt;
  if (info.offsetVal) {
    if (!dominatesOrInAncestor(info.offsetVal, loop, domInfo))
      return std::nullopt;
    if (!isAlignedOffset(info.offsetVal, info.blockSizeVal, info.blockSizeConst,
                         &info.offsetDivHint))
      return std::nullopt;
  }

  /// Require at least one db_ref that depends on a matching div op for block
  /// index.
  bool hasDbRef = false;
  loop.getBody()->walk([&](DbRefOp ref) {
    for (Value idx : ref.getIndices()) {
      Value base = ValueUtils::stripNumericCasts(idx);
      for (auto div : info.divOps) {
        if (ValueUtils::dependsOn(base, div.getResult())) {
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
  int64_t lb = 0, ub = 0, step = 0;
  if (!getConstIndex(loop.getLowerBound(), lb))
    return false;
  if (!getConstIndex(loop.getStep(), step) || step != 1)
    return false;

  if (info.blockSizeConst && *info.blockSizeConst <= 1)
    return false;

  Operation *origTerminator =
      loop.getBody()->empty() ? nullptr : &loop.getBody()->back();
  auto origYield = dyn_cast_or_null<scf::YieldOp>(origTerminator);
  if (!origYield)
    return false;

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value lbVal = builder.create<arith::ConstantIndexOp>(loc, lb);
  Value ubVal = loop.getUpperBound();
  Value bsVal =
      info.blockSizeConst
          ? builder.create<arith::ConstantIndexOp>(loc, *info.blockSizeConst)
          : ValueUtils::ensureIndexType(info.blockSizeVal, builder, loc);
  Value ubGeLb = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                               ubVal, lbVal);
  Value tripCountRaw = builder.create<arith::SubIOp>(loc, ubVal, lbVal);
  Value tripCount =
      builder.create<arith::SelectOp>(loc, ubGeLb, tripCountRaw, zero);
  Value bsMinusOne = builder.create<arith::SubIOp>(loc, bsVal, one);
  Value tripPlus = builder.create<arith::AddIOp>(loc, tripCount, bsMinusOne);
  Value numBlocksVal = builder.create<arith::DivUIOp>(loc, tripPlus, bsVal);
  if (info.blockSizeConst && getConstIndex(loop.getUpperBound(), ub)) {
    int64_t tripCountConst = ub - lb;
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
  int64_t divOffsetConst = 0;
  if (info.blockSizeConst)
    divOffsetConst = lb / *info.blockSizeConst;
  Value blockIdx = outer.getInductionVar();
  if (divOffsetConst != 0) {
    Value offsetVal =
        builder.create<arith::ConstantIndexOp>(loc, divOffsetConst);
    blockIdx = builder.create<arith::AddIOp>(loc, blockIdx, offsetVal);
  }
  if (info.offsetVal) {
    Value offsetDiv = nullptr;
    if (info.offsetDivHint) {
      offsetDiv = ValueUtils::ensureIndexType(info.offsetDivHint, builder, loc);
    } else if (auto offConst =
                   ValueUtils::tryFoldConstantIndex(info.offsetVal)) {
      if (info.blockSizeConst) {
        int64_t offDivConst = *offConst / *info.blockSizeConst;
        offsetDiv = builder.create<arith::ConstantIndexOp>(loc, offDivConst);
      }
    }
    if (!offsetDiv) {
      Value offsetIdx =
          ValueUtils::ensureIndexType(info.offsetVal, builder, loc);
      offsetDiv = builder.create<arith::DivUIOp>(loc, offsetIdx, bsVal);
    }
    auto offsetDivConst = ValueUtils::tryFoldConstantIndex(offsetDiv);
    if (!offsetDivConst || *offsetDivConst != 0)
      blockIdx = builder.create<arith::AddIOp>(loc, blockIdx, offsetDiv);
  }
  for (auto div : info.divOps)
    mapping.map(div.getResult(), blockIdx);
  for (auto rem : info.remOps)
    mapping.map(rem.getResult(), inner.getInductionVar());

  /// Clone body ops (excluding terminator) into inner loop.
  for (Operation &op : loop.getBody()->without_terminator()) {
    if (auto div = dyn_cast<arith::DivUIOp>(&op)) {
      if (llvm::is_contained(info.divOps, div))
        continue;
    }
    if (auto rem = dyn_cast<arith::RemUIOp>(&op)) {
      if (llvm::is_contained(info.remOps, rem))
        continue;
    }
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
