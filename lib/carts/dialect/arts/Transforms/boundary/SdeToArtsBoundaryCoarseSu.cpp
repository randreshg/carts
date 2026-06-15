///==========================================================================///
/// File: SdeToArtsBoundaryCoarseSu.cpp
/// Coarse SDE SU iterate access realization.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCoarseSu.h"

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryDepAnalysis.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include <algorithm>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

LogicalResult
convertCoarseSuIterate(sde::SdeSuIterateOp source,
                       SmallVectorImpl<CoarseSuDependency> &deps) {
  if (deps.empty())
    return source.emitOpError()
           << "has no DB-backed accesses for coarse SDE-to-ARTS SU "
              "realization";
  if (sde::recoverCommittedPhysicalLayout(source)) {
    if (llvm::any_of(deps, [](CoarseSuDependency &dep) {
          auto partition = dep.alloc.getPartitionMode();
          return partition && *partition != arts::PartitionMode::coarse;
        }))
      return source.emitOpError()
             << "has committed physical partition facts but no access-window "
                "dependencies; refusing coarse ARTS realization";
  }
  if (source.getAccessMinOffsetsAttr() || source.getAccessMaxOffsetsAttr() ||
      source.getOwnerDimsAttr() || source.getSpatialDimsAttr() ||
      source.getWriteFootprintAttr())
    return source.emitOpError()
           << "has movement, halo, or physical scheduling facts without "
              "committed access windows; refusing coarse ARTS realization";

  unsigned loopRank = source.getUpperBounds().size();
  if (source.getLowerBounds().size() != loopRank ||
      source.getSteps().size() != loopRank ||
      source.getBody().front().getNumArguments() < loopRank)
    return source.emitOpError() << "has inconsistent loop bounds";

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size());
  for (CoarseSuDependency &dep : deps) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::coarse),
        SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
        SmallVector<Value>{}, SmallVector<Value>{}, Value{},
        SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    acquire.setPreserveDepEdge();
    taskDeps.push_back(acquire.getPtr());
  }

  SmallVector<Value, 8> taskParams;
  for (Value capture : scalarCaptures)
    taskParams.push_back(capture);
  auto appendParamIfMissing = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value) || llvm::is_contained(taskParams, value))
      return;
    taskParams.push_back(value);
  };
  for (CoarseSuDependency &dep : deps) {
    for (Value size : dep.alloc.getSizes())
      appendParamIfMissing(size);
    for (Value elementSize : dep.alloc.getElementSizes())
      appendParamIfMissing(elementSize);
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto task = arts::EdtOp::create(builder, loc, arts::EdtType::sync,
                                  arts::EdtConcurrency::intranode, route,
                                  taskDeps, taskParams);
  if (failed(attachUnpartitionedSdeFacts(source, task)))
    return failure();

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  SmallVector<Value, 4> payloads;
  payloads.reserve(deps.size());
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value payload = arts::realizeDbInnerPayload(bodyBuilder, loc,
                                                taskBlock.getArgument(idx));
    payloads.push_back(payload);
    mapper.map(dep.alloc.getPtr(), taskBlock.getArgument(idx));
  }

  source.getBody().walk([&](Operation *op) {
    Value memref;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      memref = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      memref = store.getMemref();
    else if (auto atomic = dyn_cast<sde::SdeCuAtomicOp>(op))
      memref = atomic.getAddr();
    else
      return;
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    if (!alloc)
      return;
    auto it = llvm::find_if(deps, [&](const CoarseSuDependency &dep) {
      return dep.alloc == alloc;
    });
    if (it == deps.end())
      return;
    unsigned depIdx = static_cast<unsigned>(std::distance(deps.begin(), it));
    mapper.map(memref, payloads[depIdx]);
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (root && root != memref)
      mapper.map(root, payloads[depIdx]);
  });
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));

  for (unsigned dim = 0; dim < loopRank; ++dim) {
    Value lower = remapOrSelf(mapper, source.getLowerBounds()[dim]);
    Value upper = remapOrSelf(mapper, source.getUpperBounds()[dim]);
    Value step = remapOrSelf(mapper, source.getSteps()[dim]);
    auto localLoop = scf::ForOp::create(bodyBuilder, loc, lower, upper, step);
    mapper.map(source.getBody().front().getArgument(dim),
               localLoop.getInductionVar());
    bodyBuilder.setInsertionPointToStart(localLoop.getBody());
  }

  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";
  for (Operation &nested : computeBlock->without_terminator()) {
    if (isa<arts::DbAccessWindowOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }

  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  bool needsCompletionBarrier = !source.getNowaitAttr();
  MLIRContext *ctx = source.getContext();
  source.erase();
  if (needsCompletionBarrier) {
    OpBuilder barrierBuilder(task);
    barrierBuilder.setInsertionPointAfter(task);
    auto reason = arts::ArtsBarrierReasonAttr::get(
        ctx, arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(barrierBuilder, loc, reason);
  }
  return success();
}

LogicalResult tryConvertCoarseSuIterate(sde::SdeSuIterateOp source) {
  SmallVector<CoarseSuDependency, 4> coarseDeps;
  if (failed(collectCoarseSuDependencies(source, coarseDeps)))
    return failure();
  return convertCoarseSuIterate(source, coarseDeps);
}

} // namespace mlir::carts::arts::boundary
