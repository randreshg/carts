///==========================================================================///
/// File: SdeToArtsBoundaryRawAccessVerify.cpp
/// Verify raw SU accesses are covered by committed SDE access-window deps.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryDepAnalysis.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryPasses.h"
#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/DenseMap.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

static LogicalResult verifyRawSuAccessCoveredByDep(
    sde::SdeSuIterateOp source, Operation *site, Value memref, ArtsMode mode,
    DenseMap<Operation *, SmallVector<unsigned, 2>> &depIndex,
    SmallVectorImpl<DirectDepSpec> &deps) {
  arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
  if (!alloc) {
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (root && isDefinedInside(root, source.getOperation()))
      return success();
    if (isStackScratchMemref(memref))
      return success();
    return site->emitError()
           << "accesses external memref without ARTS DB-backed storage during "
              "direct SDE-to-ARTS SU realization";
  }

  auto it = depIndex.find(alloc.getOperation());
  if (it != depIndex.end()) {
    for (unsigned depIdx : it->second) {
      if (depIdx >= deps.size())
        continue;
      std::optional<unsigned> match =
          findDirectDepIndexForAccess(deps, alloc, mode);
      if (match && *match == depIdx)
        return success();
    }
    return site->emitError()
           << "raw access strengthens a committed SDE access-window "
              "dependency; SDE must author the dependency mode before "
              "direct ARTS lowering";
  }

  return site->emitError()
         << "touches a DB without a committed SDE access-window dependency; "
            "SDE must author the dependency before direct ARTS lowering";
}

LogicalResult verifyRawSuAccessesCoveredByDeps(
    sde::SdeSuIterateOp source,
    DenseMap<Operation *, SmallVector<unsigned, 2>> &depIndex,
    SmallVectorImpl<DirectDepSpec> &deps) {
  if (deps.empty())
    return success();
  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";

  WalkResult result = computeBlock->walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return failed(verifyRawSuAccessCoveredByDep(source, op, load.getMemref(),
                                                  ArtsMode::in, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return failed(verifyRawSuAccessCoveredByDep(
                 source, op, store.getMemref(), ArtsMode::out, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    if (auto copy = dyn_cast<memref::CopyOp>(op)) {
      if (failed(verifyRawSuAccessCoveredByDep(source, op, copy.getSource(),
                                               ArtsMode::in, depIndex, deps)))
        return WalkResult::interrupt();
      if (failed(verifyRawSuAccessCoveredByDep(source, op, copy.getTarget(),
                                               ArtsMode::out, depIndex, deps)))
        return WalkResult::interrupt();
      return WalkResult::advance();
    }
    if (auto atomic = dyn_cast<sde::SdeCuAtomicOp>(op))
      return failed(verifyRawSuAccessCoveredByDep(
                 source, op, atomic.getAddr(), ArtsMode::inout, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    return WalkResult::advance();
  });
  return result.wasInterrupted() ? failure() : success();
}

static LogicalResult verifyRawAccessCovered(sde::SdeSuIterateOp source) {
  DenseMap<Operation *, SmallVector<unsigned, 2>> depIndex;
  SmallVector<DirectDepSpec, 4> deps;
  if (failed(collectSuAccessWindowDependencySpecs(source, depIndex, deps)))
    return failure();
  return verifyRawSuAccessesCoveredByDeps(source, depIndex, deps);
}

LogicalResult runVerifyRawAccessCovered(ModuleOp module) {
  SmallVector<sde::SdeSuIterateOp> iterates;
  module.walk([&](sde::SdeSuIterateOp op) { iterates.push_back(op); });
  for (sde::SdeSuIterateOp op : iterates)
    if (failed(verifyRawAccessCovered(op)))
      return failure();
  return success();
}

} // namespace mlir::carts::arts::boundary
