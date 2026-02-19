///==========================================================================///
/// File: AcquireRewritePlanning.cpp
///
/// Plans per-task DbAcquire rewrite inputs by distribution strategy.
///
/// Example (tiling_2d output owner hints):
///   Before:
///     %acq = arts.db_acquire[<inout>] ... offsets[%rowOff], sizes[%rowSize]
///
///   Planned rewrite:
///     - keep row partitioning in offsets/sizes
///     - append [colOff, colSize] to partition_offsets/partition_sizes
///       so DbPartitioning can materialize 2D owner layout for outputs.
///==========================================================================///

#include "arts/Transforms/Edt/AcquireRewritePlanning.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool acquireHasReadAccess(DbAcquireOp acquire) {
  if (!acquire)
    return false;

  SmallVector<Value, 16> worklist{acquire.getPtr()};
  llvm::SmallPtrSet<Value, 32> visited;

  auto enqueue = [&](Value value) {
    if (value)
      worklist.push_back(value);
  };

  while (!worklist.empty()) {
    Value value = worklist.pop_back_val();
    if (!value || !visited.insert(value).second)
      continue;

    for (Operation *user : value.getUsers()) {
      if (auto load = dyn_cast<memref::LoadOp>(user)) {
        if (load.getMemRef() == value)
          return true;
        continue;
      }

      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemRef() == value)
          continue;
      }

      if (auto dbRef = dyn_cast<DbRefOp>(user)) {
        if (dbRef.getSource() == value)
          enqueue(dbRef.getResult());
        continue;
      }

      if (auto cast = dyn_cast<memref::CastOp>(user)) {
        if (cast.getSource() == value)
          enqueue(cast.getResult());
        continue;
      }

      if (auto subview = dyn_cast<memref::SubViewOp>(user)) {
        if (subview.getSource() == value)
          enqueue(subview.getResult());
        continue;
      }

      if (auto reinterpret = dyn_cast<memref::ReinterpretCastOp>(user)) {
        if (reinterpret.getSource() == value)
          enqueue(reinterpret.getResult());
        continue;
      }

      if (auto view = dyn_cast<memref::ViewOp>(user)) {
        if (view.getSource() == value)
          enqueue(view.getResult());
        continue;
      }

      if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
        if (llvm::is_contained(unrealized.getInputs(), value))
          for (Value out : unrealized.getOutputs())
            enqueue(out);
        continue;
      }

      if (auto edt = dyn_cast<EdtOp>(user)) {
        Block *entry = edt.getBody().empty() ? nullptr : &edt.getBody().front();
        if (!entry)
          continue;
        for (auto depIt : llvm::enumerate(edt.getDependencies())) {
          if (depIt.value() != value)
            continue;
          unsigned argIdx = depIt.index();
          if (argIdx < entry->getNumArguments())
            enqueue(entry->getArgument(argIdx));
        }
        continue;
      }

      if (isa<DbReleaseOp, memref::DeallocOp, memref::DimOp>(user))
        continue;

      if (auto effects = dyn_cast<MemoryEffectOpInterface>(user)) {
        SmallVector<MemoryEffects::EffectInstance, 4> valueEffects;
        effects.getEffectsOnValue(value, valueEffects);
        if (llvm::any_of(valueEffects, [](const auto &effect) {
              return isa<MemoryEffects::Read>(effect.getEffect());
            }))
          return true;
        continue;
      }

      if (llvm::is_contained(user->getOperands(), value) &&
          !isa<func::CallOp>(user))
        return true;
    }
  }

  return false;
}

} // namespace

AcquireRewritePlan
mlir::arts::planAcquireRewrite(AcquireRewritePlanningInput input) {
  auto makePlan = [&]() {
    return AcquireRewritePlan{
        AcquireRewriteInput{input.AC, input.loc, input.parentAcquire,
                            input.rootGuid, input.rootPtr, input.acquireOffset,
                            input.acquireSize, input.acquireHintSize,
                            /*extraOffsets=*/SmallVector<Value, 4>{},
                            /*extraSizes=*/SmallVector<Value, 4>{},
                            /*extraHintSizes=*/SmallVector<Value, 4>{},
                            input.step, input.stepIsUnit,
                            /*singleElement=*/false,
                            /*forceCoarse=*/
                            input.distributionKind ==
                                DistributionKind::BlockCyclic,
                            /*stencilExtent=*/Value()},
        /*useStencilRewriter=*/false};
  };

  if (!input.AC || !input.parentAcquire)
    return makePlan();

  Value zero = input.AC->createIndexConstant(0, input.loc);
  Value one = input.AC->createIndexConstant(1, input.loc);
  AcquireRewritePlan plan{makePlan()};

  bool isSingleElement = false;
  bool needsStencilHalo = false;
  Value stencilExtent;
  SmallVector<Value, 4> extraOffsets;
  SmallVector<Value, 4> extraSizes;
  SmallVector<Value, 4> extraHintSizes;

  if (Operation *rootAllocOp =
          DatablockUtils::getUnderlyingDbAlloc(input.rootPtr)) {
    if (auto dbAlloc = dyn_cast<DbAllocOp>(rootAllocOp)) {
      auto elemSizes = dbAlloc.getElementSizes();
      if (!elemSizes.empty()) {
        isSingleElement = llvm::all_of(elemSizes, [](Value value) {
          int64_t constant = 0;
          return ValueUtils::getConstantIndex(value, constant) && constant == 1;
        });

        auto accessPattern = getDbAccessPattern(dbAlloc.getOperation());
        const ArtsMode mode = input.parentAcquire.getMode();
        const bool modeNeedsStencilHalo =
            mode == ArtsMode::in || (mode == ArtsMode::inout &&
                                     acquireHasReadAccess(input.parentAcquire));
        const bool patternSaysStencil =
            accessPattern && *accessPattern == DbAccessPattern::stencil;
        const bool strategySaysStencil =
            input.distributionPattern &&
            *input.distributionPattern == EdtDistributionPattern::stencil;
        needsStencilHalo = !isSingleElement && modeNeedsStencilHalo &&
                           (patternSaysStencil || strategySaysStencil);
        if (needsStencilHalo)
          stencilExtent = input.AC->castToIndex(elemSizes.front(), input.loc);

        if (input.distributionKind == DistributionKind::Tiling2D &&
            input.parentAcquire.getMode() == ArtsMode::inout &&
            elemSizes.size() > 1 && !isSingleElement && input.tiling2DGrid &&
            input.tiling2DGrid->colWorkers) {
          Value totalCols = input.AC->castToIndex(elemSizes[1], input.loc);
          Value colWorkers =
              input.AC->castToIndex(input.tiling2DGrid->colWorkers, input.loc);
          Value colWorkerId =
              input.AC->castToIndex(input.tiling2DGrid->colWorkerId, input.loc);

          Value colWorkersMinusOne =
              input.AC->create<arith::SubIOp>(input.loc, colWorkers, one);
          Value colAdjusted = input.AC->create<arith::AddIOp>(
              input.loc, totalCols, colWorkersMinusOne);
          Value colChunk = input.AC->create<arith::DivUIOp>(
              input.loc, colAdjusted, colWorkers);
          Value colOffset =
              input.AC->create<arith::MulIOp>(input.loc, colWorkerId, colChunk);

          Value colNeedZero = input.AC->create<arith::CmpIOp>(
              input.loc, arith::CmpIPredicate::uge, colOffset, totalCols);
          Value colRemaining =
              input.AC->create<arith::SubIOp>(input.loc, totalCols, colOffset);
          Value colRemainingNonNeg = input.AC->create<arith::SelectOp>(
              input.loc, colNeedZero, zero, colRemaining);
          Value colCount = input.AC->create<arith::MinUIOp>(input.loc, colChunk,
                                                            colRemainingNonNeg);

          extraOffsets.push_back(colOffset);
          extraSizes.push_back(colCount);
          extraHintSizes.push_back(colCount);
        }
      }
    }
  }

  plan.useStencilRewriter = needsStencilHalo;
  plan.rewriteInput.singleElement = isSingleElement;
  plan.rewriteInput.stencilExtent = stencilExtent;
  plan.rewriteInput.extraOffsets = std::move(extraOffsets);
  plan.rewriteInput.extraSizes = std::move(extraSizes);
  plan.rewriteInput.extraHintSizes = std::move(extraHintSizes);
  return plan;
}
