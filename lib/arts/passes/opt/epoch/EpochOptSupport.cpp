///==========================================================================///
/// File: EpochOptSupport.cpp
///
/// Shared local helpers for the split EpochOpt implementation.
///==========================================================================///

#include "EpochOptInternal.h"

using namespace mlir;
using namespace mlir::arts;

namespace mlir::arts::epoch_opt {
namespace {
constexpr llvm::StringLiteral kAsyncLoopChainPrefix = "async_loop_";
constexpr llvm::StringLiteral kContinuationChainPrefix = "chain_";

bool isCpsExcludedDepPattern(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::jacobi_alternating_buffers:
  case ArtsDepPattern::wavefront_2d:
    return true;
  default:
    return false;
  }
}

bool loopContainsExcludedDepPattern(
    scf::ForOp forOp, function_ref<bool(ArtsDepPattern)> isExcluded) {
  if (!forOp)
    return false;

  auto matchesExcludedPattern = [&](Operation *op) {
    auto pattern = getEffectiveDepPattern(op);
    return pattern && isExcluded(*pattern);
  };

  if (matchesExcludedPattern(forOp.getOperation()))
    return true;

  bool found = false;
  forOp.walk([&](Operation *inner) {
    if (!matchesExcludedPattern(inner))
      return WalkResult::advance();
    found = true;
    return WalkResult::interrupt();
  });
  return found;
}

} // namespace

llvm::StringRef asyncLoopStrategyToString(EpochAsyncLoopStrategy strategy) {
  switch (strategy) {
  case EpochAsyncLoopStrategy::None:
    return "none";
  case EpochAsyncLoopStrategy::AdvanceEdt:
    return "advance_edt";
  case EpochAsyncLoopStrategy::CpsChain:
    return "cps_chain";
  }
  return "none";
}

bool loopContainsCpsChainExcludedDepPattern(scf::ForOp forOp) {
  return loopContainsExcludedDepPattern(forOp, isCpsExcludedDepPattern);
}

bool loopContainsCpsDriverExcludedDepPattern(scf::ForOp forOp) {
  return loopContainsExcludedDepPattern(forOp, isCpsExcludedDepPattern);
}

std::string makeAsyncLoopChainId(Operation *op) {
  return std::string(kAsyncLoopChainPrefix.data()) +
         std::to_string(reinterpret_cast<uintptr_t>(op));
}

std::string makeContinuationChainId(unsigned chainIdx) {
  return std::string(kContinuationChainPrefix.data()) +
         std::to_string(chainIdx);
}

bool isSlotOp(Operation *op, ArrayRef<EpochSlot> slots) {
  for (const EpochSlot &slot : slots) {
    if (op == slot.epoch)
      return true;
    if (slot.wrappingIf && op == slot.wrappingIf)
      return true;
  }
  return false;
}

void markClonedSlotEpochs(OpBuilder &attrBuilder, scf::IfOp ifOp) {
  for (Operation &op : ifOp.getThenRegion().front())
    if (auto epoch = dyn_cast<EpochOp>(op))
      epoch->setAttr(ContinuationForEpoch, attrBuilder.getUnitAttr());
  if (!ifOp.getElseRegion().empty()) {
    for (Operation &op : ifOp.getElseRegion().front())
      if (auto epoch = dyn_cast<EpochOp>(op))
        epoch->setAttr(ContinuationForEpoch, attrBuilder.getUnitAttr());
  }
}

Operation *cloneEpochSlot(OpBuilder &slotBuilder, EpochSlot slot,
                          IRMapping &mapping) {
  if (slot.wrappingIf) {
    Operation *clonedIfOp =
        slotBuilder.clone(*slot.wrappingIf.getOperation(), mapping);
    markClonedSlotEpochs(slotBuilder, cast<scf::IfOp>(clonedIfOp));
    return clonedIfOp;
  }

  Operation *clonedEpochOp =
      slotBuilder.clone(*slot.epoch.getOperation(), mapping);
  cast<EpochOp>(clonedEpochOp)
      ->setAttr(ContinuationForEpoch, slotBuilder.getUnitAttr());
  return clonedEpochOp;
}

void cloneNonSlotArith(OpBuilder &builder, Block &body,
                       MutableArrayRef<EpochSlot> slots, IRMapping &mapping,
                       ArrayRef<Operation *> sequentialOps) {
  DenseSet<Operation *> seqOpSet(sequentialOps.begin(), sequentialOps.end());
  for (Operation &op : body.without_terminator()) {
    if (isSlotOp(&op, slots))
      continue;
    if (seqOpSet.contains(&op))
      continue;
    Operation *cloned = builder.clone(op, mapping);
    for (auto [oldRes, newRes] :
         llvm::zip(op.getResults(), cloned->getResults()))
      mapping.map(oldRes, newRes);
  }
}

void ensureYieldTerminator(Block &block, Location loc) {
  if (block.empty() || !block.back().hasTrait<OpTrait::IsTerminator>())
    OpBuilder::atBlockEnd(&block).create<YieldOp>(loc);
}

Operation *getLastNonTerminatorOp(Block &block) {
  if (block.empty())
    return nullptr;

  Operation *lastOp = &block.back();
  return lastOp->hasTrait<OpTrait::IsTerminator>() ? lastOp->getPrevNode()
                                                   : lastOp;
}

void emitAdvanceLogic(OpBuilder &builder, Location loc, Value iv,
                      Value tCurrent, Value ub, Value step, Block &body,
                      MutableArrayRef<EpochSlot> slots,
                      ArrayRef<Operation *> allSequentialOps,
                      ArrayRef<Value> loopBackParams) {
  Value tNext = builder.create<arith::AddIOp>(loc, tCurrent, step);
  IRMapping advanceMapping;
  advanceMapping.map(iv, tNext);
  cloneNonSlotArith(builder, body, slots, advanceMapping, allSequentialOps);

  auto i64Ty = IntegerType::get(builder.getContext(), 64);
  Value stepI64 = builder.create<arith::IndexCastOp>(loc, i64Ty, step);
  Value ubI64 = builder.create<arith::IndexCastOp>(loc, i64Ty, ub);
  SmallVector<Value> nextParams = {stepI64, ubI64};
  for (Value v : loopBackParams)
    nextParams.push_back(v);
  auto advance = builder.create<CPSAdvanceOp>(
      loc, nextParams, builder.getStringAttr(makeContinuationChainId(0)));
  advance->setAttr(CPSAdditiveParams, builder.getUnitAttr());
  advance->setAttr(CPSNumCarry,
                   builder.getI64IntegerAttr(loopBackParams.size()));

  Region &advanceRegion = advance.getEpochBody();
  if (advanceRegion.empty())
    advanceRegion.emplaceBlock();
  Block &advBlock = advanceRegion.front();
  ensureYieldTerminator(advBlock, loc);
  OpBuilder advBuilder = OpBuilder::atBlockTerminator(&advBlock);

  if (slots[0].wrappingIf) {
    auto indexTy = IndexType::get(builder.getContext());
    Value ivPlaceholder = advBlock.addArgument(indexTy, loc);
    advance->setAttr(CPSAdvanceHasIvArg, builder.getUnitAttr());

    IRMapping ifMapping;
    ifMapping.map(iv, ivPlaceholder);
    for (Operation &op : body.without_terminator()) {
      if (isSlotOp(&op, slots))
        continue;
      Operation *cloned = advBuilder.clone(op, ifMapping);
      for (auto [oldRes, newRes] :
           llvm::zip(op.getResults(), cloned->getResults()))
        ifMapping.map(oldRes, newRes);
    }

    cloneEpochSlot(advBuilder, slots[0], ifMapping);
    return;
  }

  Block &epochBody = slots[0].epoch.getBody().front();
  for (Operation &epochOp : epochBody.without_terminator())
    advBuilder.clone(epochOp, advanceMapping);
}

void markAsContinuation(EdtOp edt, OpBuilder &builder, unsigned chainIdx) {
  std::string chainId = makeContinuationChainId(chainIdx);
  edt->setAttr(ControlDep,
               builder.getIntegerAttr(builder.getI32Type(), 1));
  edt->setAttr(ContinuationForEpoch, builder.getUnitAttr());
  edt->setAttr(CPSChainId, builder.getStringAttr(chainId));
  edt->setAttr(CPSLoopContinuation, builder.getUnitAttr());
}

} // namespace mlir::arts::epoch_opt
