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
  int64_t blockSize = -1;
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

static bool isLoopIv(Value v, Value iv) {
  if (!v || !iv)
    return false;
  Value stripped = ValueUtils::stripNumericCasts(v);
  return stripped == iv;
}

static std::optional<LoopBlockInfo> analyzeLoop(scf::ForOp loop) {
  LoopBlockInfo info;
  Value iv = loop.getInductionVar();

  int64_t lb = 0;
  if (!getConstIndex(loop.getLowerBound(), lb))
    return std::nullopt;

  /// Require constant step = 1 and constant bounds for now.
  int64_t ub = 0, step = 0;
  if (!getConstIndex(loop.getUpperBound(), ub))
    return std::nullopt;
  if (!getConstIndex(loop.getStep(), step) || step != 1)
    return std::nullopt;
  if (ub <= lb)
    return std::nullopt;

  bool invalid = false;
  loop.getBody()->walk([&](Operation *op) {
    if (auto div = dyn_cast<arith::DivUIOp>(op)) {
      if (!isLoopIv(div.getLhs(), iv))
        return;
      int64_t rhs = 0;
      if (!getConstIndex(div.getRhs(), rhs))
        return;
      if (rhs <= 1)
        return;
      if (info.blockSize == -1)
        info.blockSize = rhs;
      else if (info.blockSize != rhs)
        invalid = true;
      info.divOps.push_back(div);
      return;
    }
    if (auto rem = dyn_cast<arith::RemUIOp>(op)) {
      if (!isLoopIv(rem.getLhs(), iv))
        return;
      int64_t rhs = 0;
      if (!getConstIndex(rem.getRhs(), rhs))
        return;
      if (rhs <= 1)
        return;
      if (info.blockSize == -1)
        info.blockSize = rhs;
      else if (info.blockSize != rhs)
        invalid = true;
      info.remOps.push_back(rem);
      return;
    }
  });

  if (invalid || info.blockSize <= 1)
    return std::nullopt;
  if (info.divOps.empty() || info.remOps.empty())
    return std::nullopt;

  if (info.blockSize > 1 && (lb % info.blockSize) != 0)
    return std::nullopt;

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
  if (!getConstIndex(loop.getUpperBound(), ub))
    return false;
  if (!getConstIndex(loop.getStep(), step) || step != 1)
    return false;

  int64_t blockSize = info.blockSize;
  if (blockSize <= 1)
    return false;

  Operation *origTerminator =
      loop.getBody()->empty() ? nullptr : &loop.getBody()->back();
  auto origYield = dyn_cast_or_null<scf::YieldOp>(origTerminator);
  if (!origYield)
    return false;

  int64_t tripCount = ub - lb;
  if (tripCount <= blockSize)
    return false;

  int64_t numBlocks = (tripCount + blockSize - 1) / blockSize;
  if (numBlocks <= 1)
    return false;

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value lbVal = builder.create<arith::ConstantIndexOp>(loc, lb);
  Value ubVal = builder.create<arith::ConstantIndexOp>(loc, ub);
  Value bsVal = builder.create<arith::ConstantIndexOp>(loc, blockSize);
  Value numBlocksVal = builder.create<arith::ConstantIndexOp>(loc, numBlocks);

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
  int64_t divOffsetConst = lb / blockSize;
  Value blockIdx = outer.getInductionVar();
  if (divOffsetConst != 0) {
    Value offsetVal =
        builder.create<arith::ConstantIndexOp>(loc, divOffsetConst);
    blockIdx = builder.create<arith::AddIOp>(loc, blockIdx, offsetVal);
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

    SmallVector<scf::ForOp> loops;
    func.walk([&](scf::ForOp loop) { loops.push_back(loop); });

    /// Process inner loops first to avoid invalidating parents.
    for (auto it = loops.rbegin(); it != loops.rend(); ++it) {
      scf::ForOp loop = *it;
      if (!loop || loop->getParentOfType<EdtOp>())
        continue;
      if (!isInnermostLoop(loop))
        continue;

      auto info = analyzeLoop(loop);
      if (!info)
        continue;

      if (stripMineLoop(loop, *info))
        ARTS_DEBUG("Applied block strip-mining to loop");
    }
  }
};

} /// namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createBlockLoopStripMiningPass() {
  return std::make_unique<BlockLoopStripMiningPass>();
}
} /// namespace arts
} /// namespace mlir
