///==========================================================================///
/// File: LowerToMemref.cpp
///
/// Inverse of RaiseToTensor's mem2reg: lowers tensor SSA values back to
/// memref.alloca + memref.load/memref.store form.
///
/// Three-phase algorithm:
///   Phase A: Walk each function body, rewriting tensor.insert → memref.store,
///            tensor.extract → memref.load, and mapping tensor values to
///            their backing memrefs. Skips <single> cu_region bodies (those
///            are handled by ConvertToCodelet).
///   Phase B: Strip tensor iter_args from scf.for, cu_region, su_iterate
///            using replaceAllUsesWith (not dropAllUses) to safely handle
///            nested ops. Tensor iter_args that feed <single> cu_regions
///            are preserved.
///   Phase C: Erase remaining dead linalg carriers and tensor ops.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_LOWERTOMEMREF
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(lower_to_memref);

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

static MemRefType getMemRefType(RankedTensorType tensorTy) {
  return MemRefType::get(tensorTy.getShape(), tensorTy.getElementType());
}

/// Trace a tensor value through tensor.insert / tensor.cast chains to find
/// the backing memref.
static Value traceBackingMemref(Value tensor,
                                DenseMap<Value, Value> &backingMemref) {
  auto it = backingMemref.find(tensor);
  if (it != backingMemref.end())
    return it->second;
  if (auto insertOp = tensor.getDefiningOp<tensor::InsertOp>())
    return traceBackingMemref(insertOp.getDest(), backingMemref);
  if (auto castOp = tensor.getDefiningOp<tensor::CastOp>())
    return traceBackingMemref(castOp.getSource(), backingMemref);
  return Value();
}

/// Find or create a backing memref alloca for a tensor init value.
/// Traces through mu_memref_to_tensor / bufferization.to_tensor to find
/// existing allocas, or creates a new memref.alloca as fallback.
static Value getOrCreateBacking(Value initTensor, OpBuilder &builder) {
  // Trace through known conversion ops.
  if (auto muToTensor =
          initTensor.getDefiningOp<sde::SdeMuMemrefToTensorOp>())
    return muToTensor.getMemref();
  if (auto toTensor = initTensor.getDefiningOp<bufferization::ToTensorOp>())
    return toTensor.getBuffer();
  // Trace through tensor.insert chains.
  if (auto insertOp = initTensor.getDefiningOp<tensor::InsertOp>())
    return getOrCreateBacking(insertOp.getDest(), builder);
  if (auto castOp = initTensor.getDefiningOp<tensor::CastOp>())
    return getOrCreateBacking(castOp.getSource(), builder);

  // Fallback: create a temporary alloca.
  auto tensorTy = cast<RankedTensorType>(initTensor.getType());
  return memref::AllocaOp::create(builder, initTensor.getLoc(),
                                  getMemRefType(tensorTy));
}

//===----------------------------------------------------------------------===//
// Phase A: Walk block, rewrite tensor ops to memref, map backing memrefs
//===----------------------------------------------------------------------===//

static void walkBlock(Block &block, DenseMap<Value, Value> &backingMemref);

static void processRegionOp(sde::SdeCuRegionOp cuRegion,
                            DenseMap<Value, Value> &backingMemref) {
  // Do NOT walk into <single> bodies — ConvertToCodelet handles them.
  if (cuRegion.getKind() == sde::SdeCuKind::single)
    return;

  Block &body = cuRegion.getBody().front();

  // Map block args for tensor iter_args.
  for (auto [i, arg] : llvm::enumerate(cuRegion.getIterArgs())) {
    if (!isa<RankedTensorType>(arg.getType()))
      continue;
    Value backing = traceBackingMemref(arg, backingMemref);
    if (backing)
      backingMemref[body.getArgument(i)] = backing;
  }
  walkBlock(body, backingMemref);
  // Map results to backing memrefs.
  for (auto [i, arg] : llvm::enumerate(cuRegion.getIterArgs())) {
    if (!isa<RankedTensorType>(arg.getType()))
      continue;
    Value backing = traceBackingMemref(arg, backingMemref);
    if (backing && i < cuRegion.getNumResults())
      backingMemref[cuRegion.getResult(i)] = backing;
  }
}

static void processRegionOp(sde::SdeSuIterateOp suIter,
                            DenseMap<Value, Value> &backingMemref) {
  unsigned numIVs = suIter.getLowerBounds().size();
  Block &body = suIter.getBody().front();
  for (auto [i, acc] : llvm::enumerate(suIter.getReductionAccumulators())) {
    if (!isa<RankedTensorType>(acc.getType()))
      continue;
    Value backing = traceBackingMemref(acc, backingMemref);
    unsigned blockArgIdx = numIVs + i;
    if (backing && blockArgIdx < body.getNumArguments())
      backingMemref[body.getArgument(blockArgIdx)] = backing;
  }
  walkBlock(body, backingMemref);
  for (auto [i, acc] : llvm::enumerate(suIter.getReductionAccumulators())) {
    if (!isa<RankedTensorType>(acc.getType()))
      continue;
    Value backing = traceBackingMemref(acc, backingMemref);
    if (backing && i < suIter.getNumResults())
      backingMemref[suIter.getResult(i)] = backing;
  }
}

static void processRegionOp(scf::ForOp forOp,
                            DenseMap<Value, Value> &backingMemref) {
  for (auto [i, init] : llvm::enumerate(forOp.getInitArgs())) {
    if (!isa<RankedTensorType>(init.getType()))
      continue;
    Value backing = traceBackingMemref(init, backingMemref);
    if (backing)
      backingMemref[forOp.getBody()->getArgument(1 + i)] = backing;
  }
  walkBlock(*forOp.getBody(), backingMemref);
  for (auto [i, init] : llvm::enumerate(forOp.getInitArgs())) {
    if (!isa<RankedTensorType>(init.getType()))
      continue;
    Value backing = traceBackingMemref(init, backingMemref);
    if (backing && i < forOp.getNumResults())
      backingMemref[forOp.getResult(i)] = backing;
  }
}

static void walkBlock(Block &block, DenseMap<Value, Value> &backingMemref) {
  SmallVector<Operation *> toErase;

  for (Operation &op : llvm::make_early_inc_range(block)) {
    if (op.hasTrait<OpTrait::IsTerminator>())
      continue;

    if (auto muAlloc = dyn_cast<sde::SdeMuAllocOp>(&op)) {
      auto tensorTy = cast<RankedTensorType>(muAlloc.getTensor().getType());
      OpBuilder b(muAlloc);
      Value alloca = memref::AllocaOp::create(b, muAlloc.getLoc(),
                                               getMemRefType(tensorTy),
                                               muAlloc.getDynamicSizes());
      backingMemref[muAlloc.getTensor()] = alloca;
      toErase.push_back(muAlloc);
      continue;
    }

    if (auto muToTensor = dyn_cast<sde::SdeMuMemrefToTensorOp>(&op)) {
      backingMemref[muToTensor.getTensor()] = muToTensor.getMemref();
      toErase.push_back(muToTensor);
      continue;
    }

    if (auto muToMemref = dyn_cast<sde::SdeMuTensorToMemrefOp>(&op)) {
      toErase.push_back(muToMemref);
      continue;
    }

    if (auto insertOp = dyn_cast<tensor::InsertOp>(&op)) {
      Value backing = traceBackingMemref(insertOp.getDest(), backingMemref);
      if (backing) {
        OpBuilder b(insertOp);
        memref::StoreOp::create(b, insertOp.getLoc(), insertOp.getScalar(),
                                backing, insertOp.getIndices());
        backingMemref[insertOp.getResult()] = backing;
        toErase.push_back(insertOp);
        continue;
      }
    }

    if (auto extractOp = dyn_cast<tensor::ExtractOp>(&op)) {
      Value backing = traceBackingMemref(extractOp.getTensor(), backingMemref);
      if (backing) {
        OpBuilder b(extractOp);
        Value loaded = memref::LoadOp::create(b, extractOp.getLoc(), backing,
                                              extractOp.getIndices());
        extractOp.getResult().replaceAllUsesWith(loaded);
        toErase.push_back(extractOp);
        continue;
      }
    }

    if (auto toTensor = dyn_cast<bufferization::ToTensorOp>(&op)) {
      backingMemref[toTensor.getResult()] = toTensor.getBuffer();
      toErase.push_back(toTensor);
      continue;
    }

    if (auto castOp = dyn_cast<tensor::CastOp>(&op)) {
      Value backing = traceBackingMemref(castOp.getSource(), backingMemref);
      if (backing) {
        backingMemref[castOp.getResult()] = backing;
        toErase.push_back(castOp);
        continue;
      }
    }

    // Region-holding ops: set up backing map and recurse.
    if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
      processRegionOp(forOp, backingMemref);
      continue;
    }
    if (auto cuRegion = dyn_cast<sde::SdeCuRegionOp>(&op)) {
      processRegionOp(cuRegion, backingMemref);
      continue;
    }
    if (auto suIter = dyn_cast<sde::SdeSuIterateOp>(&op)) {
      processRegionOp(suIter, backingMemref);
      continue;
    }

    // scf.execute_region is transparent — propagate backing map through.
    if (auto execRegion = dyn_cast<scf::ExecuteRegionOp>(&op)) {
      if (!execRegion.getRegion().empty())
        walkBlock(execRegion.getRegion().front(), backingMemref);
      continue;
    }

    // Other region-holding ops.
    if (op.getNumRegions() > 0) {
      for (Region &region : op.getRegions()) {
        if (!region.empty()) {
          DenseMap<Value, Value> innerBacking(backingMemref);
          walkBlock(region.front(), innerBacking);
        }
      }
      continue;
    }
  }

  for (Operation *op : llvm::reverse(toErase)) {
    if (op->use_empty())
      op->erase();
  }
}

//===----------------------------------------------------------------------===//
// Phase B: Strip tensor iter_args using replaceAllUsesWith
//
// For each region-holding op with tensor iter_args:
//   1. Replace tensor block arg uses with mu_memref_to_tensor from backing
//   2. Add mu_tensor_to_memref before yields (store back to backing)
//   3. Replace tensor results with mu_memref_to_tensor after the op
//   4. Rebuild the op without tensor iter_args
//===----------------------------------------------------------------------===//

/// Strip tensor iter_args from scf.for ops.
static void stripScfForTensorArgs(ModuleOp module) {
  SmallVector<scf::ForOp> forOps;
  module.walk([&](scf::ForOp op) {
    for (Value init : op.getInitArgs()) {
      if (isa<RankedTensorType>(init.getType())) {
        forOps.push_back(op);
        break;
      }
    }
  });

  for (scf::ForOp forOp : forOps) {
    SmallVector<unsigned> tensorArgIndices;
    SmallVector<unsigned> keptIndices;
    for (unsigned i = 0, e = forOp.getInitArgs().size(); i < e; ++i) {
      if (isa<RankedTensorType>(forOp.getInitArgs()[i].getType()))
        tensorArgIndices.push_back(i);
      else
        keptIndices.push_back(i);
    }
    if (tensorArgIndices.empty())
      continue;

    MLIRContext *ctx = forOp.getContext();
    Location loc = forOp.getLoc();
    Block &body = *forOp.getBody();
    OpBuilder outerBuilder(forOp);

    // Find backing allocas for tensor init values.
    SmallVector<Value> backingAllocas;
    for (unsigned idx : tensorArgIndices)
      backingAllocas.push_back(
          getOrCreateBacking(forOp.getInitArgs()[idx], outerBuilder));

    // Replace tensor block arg uses with mu_memref_to_tensor from alloca.
    for (auto [i, idx] : llvm::enumerate(tensorArgIndices)) {
      BlockArgument blockArg = body.getArgument(1 + idx);
      auto tensorTy = cast<RankedTensorType>(blockArg.getType());
      OpBuilder bodyStart(ctx);
      bodyStart.setInsertionPointToStart(&body);
      Value tensorFromAlloca = sde::SdeMuMemrefToTensorOp::create(
          bodyStart, loc, tensorTy, backingAllocas[i]);
      blockArg.replaceAllUsesWith(tensorFromAlloca);
    }

    // Before yield: store yielded tensors back to alloca.
    auto yield = cast<scf::YieldOp>(body.getTerminator());
    for (auto [i, idx] : llvm::enumerate(tensorArgIndices)) {
      if (idx < yield.getResults().size()) {
        OpBuilder preYield(yield);
        sde::SdeMuTensorToMemrefOp::create(preYield, loc,
                                           yield.getResults()[idx],
                                           backingAllocas[i]);
      }
    }

    // After for: replace tensor results with reads from alloca.
    OpBuilder afterBuilder(ctx);
    afterBuilder.setInsertionPointAfter(forOp);
    for (auto [i, idx] : llvm::enumerate(tensorArgIndices)) {
      if (idx >= forOp.getNumResults())
        continue;
      auto tensorTy =
          cast<RankedTensorType>(forOp.getResult(idx).getType());
      Value tensorAfter = sde::SdeMuMemrefToTensorOp::create(
          afterBuilder, loc, tensorTy, backingAllocas[i]);
      forOp.getResult(idx).replaceAllUsesWith(tensorAfter);
    }

    // Rebuild scf.for without tensor iter_args.
    SmallVector<Value> keptInitArgs;
    for (unsigned idx : keptIndices)
      keptInitArgs.push_back(forOp.getInitArgs()[idx]);

    auto newFor = scf::ForOp::create(outerBuilder, loc, forOp.getLowerBound(),
                                     forOp.getUpperBound(), forOp.getStep(),
                                     keptInitArgs);
    Block &newBody = *newFor.getBody();

    // Map IV.
    body.getArgument(0).replaceAllUsesWith(newBody.getArgument(0));
    // Map kept iter_arg block args.
    for (auto [newIdx, oldIdx] : llvm::enumerate(keptIndices))
      body.getArgument(1 + oldIdx).replaceAllUsesWith(
          newBody.getArgument(1 + newIdx));

    // Move ops from old body to new. Erase default yield first.
    if (!newBody.empty() && newBody.back().hasTrait<OpTrait::IsTerminator>())
      newBody.back().erase();
    newBody.getOperations().splice(newBody.end(), body.getOperations());

    // Fix yield: keep only non-tensor values.
    auto newYield = cast<scf::YieldOp>(newBody.getTerminator());
    SmallVector<Value> newYieldValues;
    for (unsigned idx : keptIndices)
      newYieldValues.push_back(newYield.getResults()[idx]);
    OpBuilder yieldBuilder(newYield);
    scf::YieldOp::create(yieldBuilder, loc, newYieldValues);
    newYield.erase();

    // Replace kept results.
    for (auto [newIdx, oldIdx] : llvm::enumerate(keptIndices))
      forOp.getResult(oldIdx).replaceAllUsesWith(newFor.getResult(newIdx));

    forOp.erase();
  }
}

/// Strip tensor iter_args from cu_region ops (parallel/master only).
/// Tensor iter_args that feed nested <single> cu_regions are preserved.
static void stripCuRegionTensorArgs(ModuleOp module) {
  SmallVector<sde::SdeCuRegionOp> cuRegions;
  module.walk([&](sde::SdeCuRegionOp op) {
    if (op.getKind() == sde::SdeCuKind::single)
      return; // Handled by ConvertToCodelet.
    for (Value arg : op.getIterArgs()) {
      if (isa<RankedTensorType>(arg.getType())) {
        cuRegions.push_back(op);
        break;
      }
    }
  });

  for (sde::SdeCuRegionOp cuRegion : cuRegions) {
    Block &body = cuRegion.getBody().front();
    SmallVector<unsigned> tensorArgIndices; // to strip
    SmallVector<unsigned> keptIndices;      // non-tensor
    for (unsigned i = 0, e = cuRegion.getIterArgs().size(); i < e; ++i) {
      if (isa<RankedTensorType>(cuRegion.getIterArgs()[i].getType()))
        tensorArgIndices.push_back(i);
      else
        keptIndices.push_back(i);
    }
    if (tensorArgIndices.empty())
      continue;

    MLIRContext *ctx = cuRegion.getContext();
    Location loc = cuRegion.getLoc();
    OpBuilder outerBuilder(cuRegion);

    // Find backing allocas for tensor init values.
    SmallVector<Value> backingAllocas;
    for (unsigned idx : tensorArgIndices)
      backingAllocas.push_back(
          getOrCreateBacking(cuRegion.getIterArgs()[idx], outerBuilder));

    // Replace tensor block arg uses with mu_memref_to_tensor from alloca.
    for (auto [i, idx] : llvm::enumerate(tensorArgIndices)) {
      BlockArgument blockArg = body.getArgument(idx);
      auto tensorTy = cast<RankedTensorType>(blockArg.getType());
      OpBuilder bodyStart(ctx);
      bodyStart.setInsertionPointToStart(&body);
      Value tensorFromAlloca = sde::SdeMuMemrefToTensorOp::create(
          bodyStart, loc, tensorTy, backingAllocas[i]);
      blockArg.replaceAllUsesWith(tensorFromAlloca);
    }

    // Before yield: store yielded tensors back to alloca.
    auto yield = cast<sde::SdeYieldOp>(body.getTerminator());
    for (auto [i, idx] : llvm::enumerate(tensorArgIndices)) {
      if (idx < yield.getValues().size()) {
        OpBuilder preYield(yield);
        sde::SdeMuTensorToMemrefOp::create(preYield, loc,
                                           yield.getValues()[idx],
                                           backingAllocas[i]);
      }
    }

    // After cu_region: replace tensor results with reads from alloca.
    OpBuilder afterBuilder(ctx);
    afterBuilder.setInsertionPointAfter(cuRegion);
    for (auto [i, idx] : llvm::enumerate(tensorArgIndices)) {
      if (idx >= cuRegion.getNumResults())
        continue;
      auto tensorTy =
          cast<RankedTensorType>(cuRegion.getResult(idx).getType());
      Value tensorAfter = sde::SdeMuMemrefToTensorOp::create(
          afterBuilder, loc, tensorTy, backingAllocas[i]);
      cuRegion.getResult(idx).replaceAllUsesWith(tensorAfter);
    }

    // Rebuild cu_region without stripped tensor iter_args.
    SmallVector<Value> keptIterArgs;
    SmallVector<Type> keptResultTypes;
    for (unsigned idx : keptIndices) {
      keptIterArgs.push_back(cuRegion.getIterArgs()[idx]);
      keptResultTypes.push_back(cuRegion.getResult(idx).getType());
    }

    OpBuilder rebuilder(cuRegion);
    auto newRegion = sde::SdeCuRegionOp::create(
        rebuilder, loc, TypeRange{keptResultTypes}, cuRegion.getKindAttr(),
        cuRegion.getConcurrencyScopeAttr(),
        cuRegion.getNowaitAttr() ? UnitAttr::get(ctx) : nullptr,
        ValueRange{keptIterArgs});

    newRegion.getBody().takeBody(cuRegion.getBody());
    Block &newBody = newRegion.getBody().front();

    // Erase stripped tensor block args (reverse order to keep indices valid).
    for (auto it = tensorArgIndices.rbegin(); it != tensorArgIndices.rend();
         ++it)
      newBody.eraseArgument(*it);

    // Fix yield: keep only non-stripped values.
    if (auto oldYield =
            dyn_cast_or_null<sde::SdeYieldOp>(newBody.getTerminator())) {
      SmallVector<Value> newYieldValues;
      for (unsigned idx : keptIndices) {
        if (idx < oldYield.getValues().size())
          newYieldValues.push_back(oldYield.getValues()[idx]);
      }
      OpBuilder yieldBuilder(oldYield);
      sde::SdeYieldOp::create(yieldBuilder, loc, newYieldValues);
      oldYield.erase();
    }

    // Replace kept results.
    for (auto [newIdx, oldIdx] : llvm::enumerate(keptIndices))
      cuRegion.getResult(oldIdx).replaceAllUsesWith(
          newRegion.getResult(newIdx));

    cuRegion.erase();
  }
}

/// Strip tensor accumulators from su_iterate ops.
static void stripSuIterateTensorArgs(ModuleOp module) {
  SmallVector<sde::SdeSuIterateOp> suIters;
  module.walk([&](sde::SdeSuIterateOp op) {
    for (Value acc : op.getReductionAccumulators()) {
      if (isa<RankedTensorType>(acc.getType())) {
        suIters.push_back(op);
        break;
      }
    }
  });

  for (sde::SdeSuIterateOp suIter : suIters) {
    ValueRange allAccumulators = suIter.getReductionAccumulators();
    SmallVector<unsigned> tensorAccIndices;
    SmallVector<unsigned> realAccIndices;
    for (auto [i, acc] : llvm::enumerate(allAccumulators)) {
      if (isa<RankedTensorType>(acc.getType()))
        tensorAccIndices.push_back(i);
      else
        realAccIndices.push_back(i);
    }
    if (tensorAccIndices.empty())
      continue;

    unsigned numIVs = suIter.getLowerBounds().size();
    MLIRContext *ctx = suIter.getContext();
    Location loc = suIter.getLoc();
    Block &body = suIter.getBody().front();
    OpBuilder outerBuilder(suIter);

    // Find backing allocas for tensor accumulator init values.
    SmallVector<Value> backingAllocas;
    for (unsigned accIdx : tensorAccIndices)
      backingAllocas.push_back(
          getOrCreateBacking(allAccumulators[accIdx], outerBuilder));

    // Replace tensor block arg uses with mu_memref_to_tensor from alloca.
    for (auto [i, accIdx] : llvm::enumerate(tensorAccIndices)) {
      unsigned blockArgIdx = numIVs + accIdx;
      if (blockArgIdx >= body.getNumArguments())
        continue;
      BlockArgument blockArg = body.getArgument(blockArgIdx);
      auto tensorTy = cast<RankedTensorType>(blockArg.getType());
      OpBuilder bodyStart(ctx);
      bodyStart.setInsertionPointToStart(&body);
      Value tensorFromAlloca = sde::SdeMuMemrefToTensorOp::create(
          bodyStart, loc, tensorTy, backingAllocas[i]);
      blockArg.replaceAllUsesWith(tensorFromAlloca);
    }

    // Before yield: store yielded tensors back to alloca.
    if (auto yield =
            dyn_cast_or_null<sde::SdeYieldOp>(body.getTerminator())) {
      for (auto [i, accIdx] : llvm::enumerate(tensorAccIndices)) {
        if (accIdx < yield.getValues().size()) {
          OpBuilder preYield(yield);
          sde::SdeMuTensorToMemrefOp::create(preYield, loc,
                                             yield.getValues()[accIdx],
                                             backingAllocas[i]);
        }
      }
    }

    // After su_iterate: replace tensor results with reads from alloca.
    OpBuilder afterBuilder(ctx);
    afterBuilder.setInsertionPointAfter(suIter);
    for (auto [i, accIdx] : llvm::enumerate(tensorAccIndices)) {
      if (accIdx >= suIter.getNumResults())
        continue;
      auto tensorTy =
          cast<RankedTensorType>(suIter.getResult(accIdx).getType());
      Value tensorAfter = sde::SdeMuMemrefToTensorOp::create(
          afterBuilder, loc, tensorTy, backingAllocas[i]);
      suIter.getResult(accIdx).replaceAllUsesWith(tensorAfter);
    }

    // Rebuild su_iterate with only real (memref) reduction accumulators.
    NamedAttrList attrs = sde::getRewrittenAttrs(suIter);

    SmallVector<Value> realAccValues;
    SmallVector<Type> newResultTypes;
    for (unsigned idx : realAccIndices) {
      realAccValues.push_back(allAccumulators[idx]);
      if (idx < suIter.getNumResults())
        newResultTypes.push_back(suIter.getResult(idx).getType());
    }

    ArrayAttr newReductionKinds;
    if (auto kindsAttr = suIter.getReductionKindsAttr()) {
      SmallVector<Attribute> filteredKinds;
      for (unsigned idx : realAccIndices) {
        if (idx < kindsAttr.size())
          filteredKinds.push_back(kindsAttr[idx]);
      }
      if (!filteredKinds.empty())
        newReductionKinds = ArrayAttr::get(ctx, filteredKinds);
    }

    OpBuilder rebuilder(suIter);
    auto newSuIter = sde::SdeSuIterateOp::create(
        rebuilder, loc, newResultTypes, suIter.getLowerBounds(),
        suIter.getUpperBounds(), suIter.getSteps(), suIter.getScheduleAttr(),
        suIter.getChunkSize(),
        suIter.getNowaitAttr() ? UnitAttr::get(ctx) : nullptr, realAccValues,
        newReductionKinds, suIter.getReductionStrategyAttr(),
        suIter.getStructuredClassificationAttr(),
        suIter.getAccessMinOffsetsAttr(), suIter.getAccessMaxOffsetsAttr(),
        suIter.getOwnerDimsAttr(), suIter.getSpatialDimsAttr(),
        suIter.getWriteFootprintAttr());

    for (auto attr : attrs)
      newSuIter->setAttr(attr.getName(), attr.getValue());

    newSuIter.getBody().takeBody(suIter.getBody());
    Block &newBody = newSuIter.getBody().front();

    // Erase tensor block args (reverse order to keep indices valid).
    for (auto it = tensorAccIndices.rbegin(); it != tensorAccIndices.rend();
         ++it) {
      unsigned argIdx = numIVs + *it;
      if (argIdx < newBody.getNumArguments())
        newBody.eraseArgument(argIdx);
    }

    // Fix yield: keep only real accumulator values.
    if (auto yield =
            dyn_cast_or_null<sde::SdeYieldOp>(newBody.getTerminator())) {
      SmallVector<Value> newYieldValues;
      for (unsigned idx : realAccIndices) {
        if (idx < yield.getValues().size())
          newYieldValues.push_back(yield.getValues()[idx]);
      }
      OpBuilder yieldBuilder(yield);
      sde::SdeYieldOp::create(yieldBuilder, loc, newYieldValues);
      yield.erase();
    }

    // Replace kept results.
    for (auto [newIdx, oldIdx] : llvm::enumerate(realAccIndices)) {
      if (oldIdx < suIter.getNumResults() && newIdx < newSuIter.getNumResults())
        suIter.getResult(oldIdx).replaceAllUsesWith(
            newSuIter.getResult(newIdx));
    }

    suIter.erase();
  }
}

//===----------------------------------------------------------------------===//
// Phase C: Erase dead carriers
//===----------------------------------------------------------------------===//

static void eraseDeadCarriers(ModuleOp module) {
  // Erase materialize_in_destination sinks first.
  SmallVector<Operation *> matOps;
  module.walk([&](bufferization::MaterializeInDestinationOp op) {
    matOps.push_back(op);
  });
  for (Operation *op : llvm::reverse(matOps))
    op->erase();

  // Iteratively erase dead tensor/linalg/sde-mu ops.
  bool changed = true;
  while (changed) {
    changed = false;
    SmallVector<Operation *> dead;
    module.walk([&](Operation *op) {
      if (isa<linalg::GenericOp, bufferization::ToTensorOp, tensor::EmptyOp,
              tensor::CastOp, tensor::ExtractOp, tensor::InsertOp,
              sde::SdeMuAllocOp, sde::SdeMuMemrefToTensorOp,
              sde::SdeMuTensorToMemrefOp>(op)) {
        if (op->use_empty())
          dead.push_back(op);
      }
    });
    for (Operation *op : llvm::reverse(dead)) {
      op->erase();
      changed = true;
    }
  }
}

//===----------------------------------------------------------------------===//
// Pass implementation
//===----------------------------------------------------------------------===//

struct LowerToMemrefPass
    : public arts::impl::LowerToMemrefBase<LowerToMemrefPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    // Phase A: Walk each function, rewrite tensor ops to memref ops,
    //          build backing memref map.
    unsigned totalLowered = 0;
    module.walk([&](func::FuncOp func) {
      if (func.getBody().empty())
        return;
      DenseMap<Value, Value> backingMemref;
      walkBlock(func.getBody().front(), backingMemref);
      totalLowered += backingMemref.size();
    });

    // Phase B: Strip tensor iter_args.
    // Order: cu_regions first (innermost via post-order walk), then
    // su_iterates, then scf.for (outermost).
    stripCuRegionTensorArgs(module);
    stripSuIterateTensorArgs(module);
    stripScfForTensorArgs(module);

    // Phase C: Erase remaining dead carriers.
    eraseDeadCarriers(module);

    ARTS_INFO("LowerToMemref: lowered " << totalLowered
                                        << " tensor value(s) to memref");
  }
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass> createLowerToMemrefPass() {
  return std::make_unique<LowerToMemrefPass>();
}

} // namespace mlir::arts::sde
