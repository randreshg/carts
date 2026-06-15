///==========================================================================///
/// File: SdeToArtsBoundaryStandaloneCu.cpp
/// Standalone CU region access realization.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryStandaloneCu.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryDepAnalysis.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {
namespace {

bool enqueueForwardedMemrefResults(Operation *user, Value value,
                                   SmallVectorImpl<Value> &worklist) {
  if (auto cast = dyn_cast<memref::CastOp>(user)) {
    if (cast.getSource() != value)
      return false;
    worklist.push_back(cast.getResult());
    return true;
  }
  if (auto subview = dyn_cast<memref::SubViewOp>(user)) {
    if (subview.getSource() != value)
      return false;
    worklist.push_back(subview.getResult());
    return true;
  }
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
    if (!llvm::is_contained(unrealized.getInputs(), value))
      return false;
    for (Value result : unrealized.getOutputs())
      if (isa<MemRefType>(result.getType()))
        worklist.push_back(result);
    return true;
  }
  return false;
}

bool isReadOnlyMemrefUseInside(Value source, Operation *scope) {
  SmallVector<Value, 8> worklist;
  DenseSet<Value> visited;
  worklist.push_back(source);

  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!current || !visited.insert(current).second)
      continue;

    for (Operation *user : current.getUsers()) {
      if (!scope->isAncestor(user))
        continue;
      if (auto access = arts::DbUtils::getMemoryAccessInfo(user)) {
        if (access->memref == current &&
            access->kind == arts::DbUtils::MemoryAccessKind::Write)
          return false;
        continue;
      }
      if (auto dim = dyn_cast<memref::DimOp>(user)) {
        if (dim.getSource() == current)
          continue;
      }
      if (enqueueForwardedMemrefResults(user, current, worklist))
        continue;
      return false;
    }
  }

  return true;
}

LogicalResult
collectCloneableReadOnlyGlobalMemrefs(sde::SdeCuRegionOp source,
                                      const DenseSet<Value> &allowedDbHandles,
                                      SetVector<Value> &captures) {
  bool failed = false;
  source.getBody().walk([&](Operation *op) {
    if (isa<arts::DbAccessWindowOp, sde::SdeYieldOp>(op))
      return;
    for (Value operand : op->getOperands()) {
      if (!isa<MemRefType>(operand.getType()) ||
          isDefinedInside(operand, source.getOperation()))
        continue;
      if (allowedDbHandles.contains(operand))
        continue;
      if (operand.getDefiningOp<memref::GetGlobalOp>()) {
        if (!isReadOnlyMemrefUseInside(operand, source.getOperation())) {
          op->emitError()
              << "writes or escapes a cloned read-only memref.global inside a "
                 "standalone CU; mutable global state must be represented as "
                 "an explicit SDE/ARTS dependency";
          failed = true;
          continue;
        }
        captures.insert(operand);
        continue;
      }
      if (resolveBoundaryDbAlloc(operand)) {
        op->emitError()
            << "uses a DB-backed external memref that was not remapped to an "
               "EDT dependency; SDE must provide a committed access window for "
               "this standalone CU access";
        failed = true;
      }
    }
  });
  return failure(failed);
}

// A residual CU may use a heap `memref.alloc` scratch buffer (e.g. softmax
// output) that is allocated outside but used and freed entirely within the CU.
// Such a buffer is EDT-private: clone its alloc into the EDT body so it is not
// captured as an outer pointer. Only buffers whose every use is inside the CU
// qualify; anything escaping the CU must travel as a real DB dependency.
LogicalResult collectCloneablePrivateScratchAllocs(sde::SdeCuRegionOp source,
                                                   SetVector<Value> &scratch) {
  SetVector<Value> candidates;
  source.getBody().walk([&](Operation *op) {
    if (isa<arts::DbAccessWindowOp, sde::SdeYieldOp>(op))
      return;
    for (Value operand : op->getOperands()) {
      if (!isa<MemRefType>(operand.getType()) ||
          isDefinedInside(operand, source.getOperation()))
        continue;
      if (operand.getDefiningOp<memref::AllocOp>())
        candidates.insert(operand);
    }
  });
  for (Value alloc : candidates) {
    bool allInside = true;
    for (Operation *user : alloc.getUsers())
      if (!source.getOperation()->isAncestor(user)) {
        allInside = false;
        break;
      }
    if (allInside)
      scratch.insert(alloc);
  }
  return success();
}

// A standalone residual CU may read a DB-backed array for which SDE authored no
// access window (e.g. a diagonal checksum over a coarse whole-array output).
// Author a whole-array coarse read dependency for each such DB so the access is
// remapped to a DbRef over the complete array. A coarse DB holds the entire
// array in one block, so a whole-array read is exact. A block-partitioned DB
// has multiple blocks; a coarse DbRef would read only block 0 -> silent-wrong,
// so fail closed and require an SDE-committed window for that case.
LogicalResult
appendCoarseDbReadDependencies(sde::SdeCuRegionOp source,
                               SmallVectorImpl<DirectCuDepSpec> &deps) {
  DenseSet<Operation *> covered;
  for (DirectCuDepSpec &dep : deps)
    covered.insert(dep.alloc.getOperation());

  SmallVector<arts::DbAllocOp, 4> pendingAllocs;
  DenseMap<Operation *, bool> pendingHasWrite;
  bool failed = false;
  source.getBody().walk([&](Operation *op) {
    Value memref;
    bool isWrite = false;
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      memref = load.getMemref();
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      memref = store.getMemref();
      isWrite = true;
    } else {
      return;
    }
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    if (!alloc || covered.contains(alloc.getOperation()))
      return;
    std::optional<arts::PartitionMode> mode = alloc.getPartitionMode();
    if (!mode || *mode != arts::PartitionMode::coarse) {
      op->emitError()
          << "accesses a block-partitioned DB-backed array in a standalone CU "
             "without a committed SDE access window; a coarse access would "
             "only "
             "see one block (silent-wrong). SDE must author a multi-block "
             "access window for this access";
      failed = true;
      return;
    }
    auto [it, inserted] =
        pendingHasWrite.try_emplace(alloc.getOperation(), isWrite);
    if (inserted)
      pendingAllocs.push_back(alloc);
    else
      it->second |= isWrite;
  });
  if (failed)
    return failure();

  for (arts::DbAllocOp alloc : pendingAllocs) {
    DirectCuDepSpec dep;
    dep.alloc = alloc;
    dep.mode =
        pendingHasWrite[alloc.getOperation()] ? ArtsMode::inout : ArtsMode::in;
    dep.ownerDimCount = 0;
    deps.push_back(std::move(dep));
  }
  return success();
}

} // namespace

LogicalResult realizeStandaloneCuAccesses(sde::SdeCuRegionOp source) {
  if (!source || source->getParentOfType<sde::SdeSuIterateOp>())
    return success();

  SmallVector<DirectCuDepSpec, 4> deps;
  SmallVector<arts::DbAccessWindowOp, 4> windows;
  if (failed(collectStandaloneCuDependencies(source, deps, windows)))
    return failure();
  if (windows.empty())
    return success();
  // SDE may leave a coarse whole-array read of a residual checksum unwindowed;
  // author a coarse read dep so it is remapped (fail closed for multi-block).
  if (failed(appendCoarseDbReadDependencies(source, deps)))
    return failure();

  if (source.getBody().empty())
    return source.emitOpError()
           << "has no body during standalone CU access realization";
  if (!source.getIterArgs().empty())
    return source.emitOpError()
           << "access-bearing standalone CU iter_args are not representable "
              "as an ARTS EDT; realize an explicit SDE dataflow first";
  for (Type resultType : source.getResultTypes())
    if (!isScalarParamType(resultType))
      return source.emitOpError()
             << "access-bearing standalone CU non-scalar results require an "
                "explicit SDE dataflow result before ARTS realization";
  Block &body = source.getBody().front();
  if (body.getNumArguments() != 0)
    return source.emitOpError()
           << "has region arguments during standalone CU EDT realization";
  OpBuilder builder(source.getContext());

  builder.setInsertionPoint(source);
  Location loc = source.getLoc();
  arts::ArtsLaunchPolicy standaloneLaunch;
  if (hasDistributedLaunchStorageFacts(deps)) {
    Value zero = createZeroIndex(builder, loc);
    standaloneLaunch = arts::resolveArtsOrdinalLaunchPolicy(
        source->getParentOfType<ModuleOp>(), zero, builder, loc);
  }
  Value standaloneRoute = standaloneLaunch.route
                              ? standaloneLaunch.route
                              : arts::createCurrentNodeRoute(builder, loc);

  for (DirectCuDepSpec &dep : deps) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    std::optional<arts::PartitionMode> partitionMode =
        std::optional<arts::PartitionMode>(arts::PartitionMode::block);
    if (dep.ownerDimCount == 0) {
      buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
      partitionMode = arts::PartitionMode::coarse;
    } else {
      offsets.reserve(dep.ownerDimCount);
      sizes.reserve(dep.ownerDimCount);
      for (unsigned idx = 0; idx < dep.ownerDimCount; ++idx) {
        offsets.push_back(createConstantIndex(builder, loc, dep.blockLo[idx]));
        sizes.push_back(createConstantIndex(
            builder, loc, dep.blockHi[idx] - dep.blockLo[idx]));
      }
    }

    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        partitionMode, SmallVector<Value>{}, offsets, sizes,
        SmallVector<Value>{}, SmallVector<Value>{}, SmallVector<Value>{},
        Value{}, SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    if (dep.haloShape) {
      acquire.setDepPatternAttr(ArtsDepPatternAttr::get(
          source.getContext(), ArtsDepPattern::stencil));
      acquire.setDistributionPatternAttr(EdtDistributionPatternAttr::get(
          source.getContext(), EdtDistributionPattern::stencil));
    }
    dep.acquiredPtr = acquire.getPtr();
  }

  SmallVector<CuResultSpec, 4> resultSpecs;
  resultSpecs.reserve(source.getNumResults());
  for (Type resultType : source.getResultTypes()) {
    Value one = createOneIndex(builder, loc);
    Type payloadType = arts::getElementMemRefType(resultType, 1);
    Type pointerType = MemRefType::get({ShapedType::kDynamic}, payloadType);
    auto resultDb = arts::DbAllocOp::create(
        builder, loc, ArtsMode::inout, standaloneRoute, DbAllocType::heap,
        DbMode::write, resultType, pointerType, SmallVector<Value>{one},
        SmallVector<Value>{one}, arts::PartitionMode::coarse);
    auto writeAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::out, resultDb.getGuid(), resultDb.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::coarse),
        SmallVector<Value>{}, SmallVector<Value>{createZeroIndex(builder, loc)},
        SmallVector<Value>{one}, SmallVector<Value>{}, SmallVector<Value>{},
        SmallVector<Value>{}, Value{}, SmallVector<Value>{},
        SmallVector<Value>{});
    writeAcquire.setPreserveAccessMode();
    resultSpecs.push_back({resultDb, writeAcquire.getPtr(), Value{}});
  }

  DenseMap<Operation *, DirectCuDepSpec *> depsByAlloc;
  for (DirectCuDepSpec &dep : deps)
    depsByAlloc[dep.alloc.getOperation()] = &dep;

  auto rewriteAccess = [&](Operation *op, Value memref,
                           MutableOperandRange indices) -> WalkResult {
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    auto it = depsByAlloc.find(alloc ? alloc.getOperation() : nullptr);
    if (it == depsByAlloc.end())
      return WalkResult::advance();

    DirectCuDepSpec &dep = *it->second;
    if (indices.size() < dep.ownerDimCount) {
      op->emitError() << "rank-expanded DB payload access has fewer indices "
                         "than committed owner dimensions";
      return WalkResult::interrupt();
    }

    builder.setInsertionPoint(op);
    SmallVector<Value, 4> localBlockIndices;
    localBlockIndices.reserve(dep.ownerDimCount);
    for (unsigned idx = 0; idx < dep.ownerDimCount; ++idx) {
      Value local = indices[idx].get();
      if (dep.blockLo[idx] != 0)
        local = arith::SubIOp::create(
            builder, op->getLoc(), local,
            createConstantIndex(builder, op->getLoc(), dep.blockLo[idx]));
      localBlockIndices.push_back(local);
    }
    SmallVector<Value, 4> dbRefIndices;
    if (dep.ownerDimCount == 0)
      dbRefIndices.push_back(createZeroIndex(builder, op->getLoc()));
    else
      dbRefIndices.assign(localBlockIndices.begin(), localBlockIndices.end());
    Value payload = arts::DbRefOp::create(builder, op->getLoc(),
                                          dep.acquiredPtr, dbRefIndices);
    if (auto load = dyn_cast<memref::LoadOp>(op))
      load->setOperand(0, payload);
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      store->setOperand(1, payload);
    else {
      op->emitError() << "unsupported direct DB payload access";
      return WalkResult::interrupt();
    }
    for (unsigned idx = 0; idx < dep.ownerDimCount; ++idx)
      indices[idx].set(createZeroIndex(builder, op->getLoc()));
    return WalkResult::advance();
  };

  WalkResult rewriteResult = body.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteAccess(op, load.getMemref(), load.getIndicesMutable());
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteAccess(op, store.getMemref(), store.getIndicesMutable());
    return WalkResult::advance();
  });
  if (rewriteResult.wasInterrupted())
    return failure();

  for (arts::DbAccessWindowOp window : windows)
    if (window && window->getBlock())
      window.erase();

  Operation *terminator = body.getTerminator();
  for (DirectCuDepSpec &dep : deps) {
    builder.setInsertionPoint(terminator ? terminator : &body.back());
    arts::DbReleaseOp::create(builder, loc, dep.acquiredPtr);
  }

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();
  DenseSet<Value> allowedDbHandles;
  for (DirectCuDepSpec &dep : deps) {
    allowedDbHandles.insert(dep.acquiredPtr);
    allowedDbHandles.insert(dep.alloc.getPtr());
  }
  for (CuResultSpec &result : resultSpecs)
    allowedDbHandles.insert(result.writePtr);
  SetVector<Value> cloneablePrivateScratchAllocs;
  if (failed(collectCloneablePrivateScratchAllocs(
          source, cloneablePrivateScratchAllocs)))
    return failure();
  for (Value scratch : cloneablePrivateScratchAllocs)
    allowedDbHandles.insert(scratch);
  SetVector<Value> cloneableReadOnlyGlobalMemrefs;
  if (failed(collectCloneableReadOnlyGlobalMemrefs(
          source, allowedDbHandles, cloneableReadOnlyGlobalMemrefs)))
    return failure();

  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size() + resultSpecs.size());
  for (DirectCuDepSpec &dep : deps)
    taskDeps.push_back(dep.acquiredPtr);
  for (CuResultSpec &result : resultSpecs)
    taskDeps.push_back(result.writePtr);

  SmallVector<Value, 8> taskParams(scalarCaptures.begin(),
                                   scalarCaptures.end());

  builder.setInsertionPoint(source);
  arts::ArtsLaunchPolicy launch = standaloneLaunch;
  Value route = standaloneRoute;
  auto task =
      arts::EdtOp::create(builder, loc, arts::EdtType::sync, launch.concurrency,
                          route, taskDeps, taskParams);

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value depArg = taskBlock.getArgument(idx);
    mapper.map(dep.acquiredPtr, depArg);
    mapper.map(dep.alloc.getPtr(), depArg);
  }
  unsigned resultDepBase = deps.size();
  for (auto [idx, result] : llvm::enumerate(resultSpecs))
    mapper.map(result.writePtr, taskBlock.getArgument(resultDepBase + idx));
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));

  auto yield = dyn_cast_or_null<sde::SdeYieldOp>(body.getTerminator());
  if (source.getNumResults() != 0 &&
      (!yield || yield.getValues().size() != source.getNumResults()))
    return source.emitOpError()
           << "has mismatched yield/result count during standalone CU EDT "
              "realization";

  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (Value capture : cloneableReadOnlyGlobalMemrefs) {
    Operation *def = capture.getDefiningOp();
    Operation *cloned = def->clone(mapper);
    bodyBuilder.insert(cloned);
    mapper.map(capture, cloned->getResult(0));
  }
  for (Value scratch : cloneablePrivateScratchAllocs) {
    Operation *def = scratch.getDefiningOp();
    Operation *cloned = def->clone(mapper);
    bodyBuilder.insert(cloned);
    mapper.map(scratch, cloned->getResult(0));
  }
  for (Operation &nested : body) {
    if (isa<arts::DbAccessWindowOp, sde::SdeYieldOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }
  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  for (auto [idx, result] : llvm::enumerate(resultSpecs)) {
    Value zero = createZeroIndex(bodyBuilder, loc);
    Value payload =
        arts::DbRefOp::create(bodyBuilder, loc,
                              taskBlock.getArgument(resultDepBase + idx),
                              SmallVector<Value>{zero})
            .getResult();
    Value yielded = remapOrSelf(mapper, yield.getValues()[idx]);
    memref::StoreOp::create(bodyBuilder, loc, yielded, payload,
                            SmallVector<Value>{zero});
    arts::DbReleaseOp::create(bodyBuilder, loc,
                              taskBlock.getArgument(resultDepBase + idx));
  }
  arts::YieldOp::create(bodyBuilder, loc);

  builder.setInsertionPointAfter(task);
  for (auto [idx, result] : llvm::enumerate(resultSpecs)) {
    Value zero = createZeroIndex(builder, loc);
    Value one = createOneIndex(builder, loc);
    auto readAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::in, result.alloc.getGuid(),
        result.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::coarse),
        SmallVector<Value>{}, SmallVector<Value>{zero}, SmallVector<Value>{one},
        SmallVector<Value>{}, SmallVector<Value>{}, SmallVector<Value>{},
        Value{}, SmallVector<Value>{}, SmallVector<Value>{});
    readAcquire.setPreserveAccessMode();
    Value payload = arts::DbRefOp::create(builder, loc, readAcquire.getPtr(),
                                          SmallVector<Value>{zero})
                        .getResult();
    Value loaded =
        memref::LoadOp::create(builder, loc, payload, SmallVector<Value>{zero});
    arts::DbReleaseOp::create(builder, loc, readAcquire.getPtr());
    result.replacement = loaded;
    source.getResult(idx).replaceAllUsesWith(loaded);
  }

  source.erase();
  // The original scratch allocs are now dead (their only uses were the erased
  // residual body); drop them so no orphan heap alloc remains.
  for (Value scratch : cloneablePrivateScratchAllocs)
    if (Operation *def = scratch.getDefiningOp())
      if (def->use_empty())
        def->erase();
  return success();
}

} // namespace mlir::carts::arts::boundary
