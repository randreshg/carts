///==========================================================================///
/// File: MemrefAccessCollect.cpp
///
/// Collection of read/write memref access entries for SDE scheduling-unit
/// loops: builds affine indexing maps remapped onto the loop induction vars
/// (affine and non-affine memref ops, scf/affine `if`/`for` recursion), while
/// skipping pointer-of-pointer wrapper accesses, rank-zero scalars, and local
/// scratch effects. Backs the public analyzeSuLoopAccesses wrapper.
///==========================================================================///

#include "SuLoopAccessAnalysisDetail.h"

#include "carts/dialect/sde/Analysis/AffineIndexUtils.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineMemoryOpInterfaces.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Affine/Utils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {
namespace {

static std::optional<AffineMap>
remapAccessMapToLoopIvs(AffineMap map, OperandRange mapOperands,
                        ArrayRef<Value> ivs, MLIRContext *ctx) {
  if (!map)
    return std::nullopt;
  SmallVector<Value, 8> operands(mapOperands.begin(), mapOperands.end());
  affine::fullyComposeAffineMapAndOperands(&map, &operands);
  map = simplifyAffineMap(map);
  if (map.getNumSymbols() != 0 || map.getNumDims() != operands.size())
    return std::nullopt;

  SmallVector<AffineExpr, 8> dimReplacements(map.getNumDims());
  for (auto [dim, operand] : llvm::enumerate(operands)) {
    auto it = llvm::find(ivs, operand);
    if (it == ivs.end())
      return std::nullopt;
    dimReplacements[dim] = getAffineDimExpr(
        static_cast<unsigned>(std::distance(ivs.begin(), it)), ctx);
  }

  SmallVector<AffineExpr, 4> remappedResults;
  remappedResults.reserve(map.getNumResults());
  for (AffineExpr result : map.getResults())
    remappedResults.push_back(
        result.replaceDimsAndSymbols(dimReplacements, {}));
  return AffineMap::get(ivs.size(), /*symbolCount=*/0, remappedResults, ctx);
}

static std::optional<AffineMap>
tryBuildIndexingMapFromAffineAccess(Operation *memOp, ArrayRef<Value> ivs,
                                    MLIRContext *ctx) {
  if (auto read = dyn_cast<affine::AffineReadOpInterface>(memOp)) {
    return remapAccessMapToLoopIvs(read.getAffineMap(), read.getMapOperands(),
                                   ivs, ctx);
  }
  if (auto write = dyn_cast<affine::AffineWriteOpInterface>(memOp)) {
    return remapAccessMapToLoopIvs(write.getAffineMap(), write.getMapOperands(),
                                   ivs, ctx);
  }
  return std::nullopt;
}

static std::optional<AffineMap> tryBuildIndexingMap(OperandRange indices,
                                                    ArrayRef<Value> ivs,
                                                    MLIRContext *ctx) {
  SmallVector<AffineExpr> exprs;
  exprs.reserve(indices.size());
  for (Value idx : indices) {
    auto expr = sde::tryGetAffineExpr(idx, ivs, ctx);
    if (!expr)
      return std::nullopt;
    exprs.push_back(*expr);
  }
  return AffineMap::get(/*dimCount=*/ivs.size(), /*symbolCount=*/0, exprs, ctx);
}

static bool isLocalLibcAllocatorScratchForBody(Operation *op, Block &body) {
  Operation *region = body.getParentOp();
  if (!region)
    return false;

  SmallVector<Operation *, 1> regions{region};
  return detail::isLocalLibcAllocatorScratchCall(op, body, regions) ||
         detail::isLocalLibcFreeScratchCall(op, body, regions);
}

static bool
collectMemrefAccessesImpl(Operation *scope, Block &body, ArrayRef<Value> ivs,
                          SmallVectorImpl<MemrefAccessEntry> &reads,
                          SmallVectorImpl<MemrefAccessEntry> &writes,
                          MLIRContext *ctx, bool &sawAccess) {
  for (auto &op : body) {
    if (op.hasTrait<OpTrait::IsTerminator>())
      continue;

    // arts.db_access_window is a short-lived SDE-to-ARTS boundary carrier (not
    // a data access) with a MemWrite effect; skip it by name (no ARTS dep) so a
    // shared witness SU stays classifiable after the boundary annotates its
    // body.
    if (op.getName().getStringRef() == "arts.db_access_window")
      continue;

    if (isLocalScratchEffect(&op, scope))
      continue;

    if (isLocalLibcAllocatorScratchForBody(&op, body))
      continue;

    if (auto read = dyn_cast<affine::AffineReadOpInterface>(&op)) {
      auto memrefType = read.getMemRefType();
      if (memrefType && memrefType.getRank() == 0)
        continue;
      auto map = tryBuildIndexingMapFromAffineAccess(&op, ivs, ctx);
      if (!map)
        return false;
      reads.push_back(
          {read.getMemRef(), *map, read.getOperation(), /*isRead=*/true});
      sawAccess = true;
      continue;
    }

    if (auto write = dyn_cast<affine::AffineWriteOpInterface>(&op)) {
      if (isa<MemRefType>(write.getValueToStore().getType()))
        continue;
      auto memrefType = write.getMemRefType();
      if (memrefType && memrefType.getRank() == 0)
        continue;
      auto map = tryBuildIndexingMapFromAffineAccess(&op, ivs, ctx);
      if (!map)
        return false;
      writes.push_back({write.getMemRef(), *map, write.getOperation(),
                        /*isRead=*/false});
      sawAccess = true;
      continue;
    }

    if (auto loadOp = dyn_cast<memref::LoadOp>(&op)) {
      // Skip pointer-to-memref wrapper loads (e.g., memref.load %wrapper[] :
      // memref<memref<?xi32>>). These are pointer dereferences, not data
      // accesses. Including them would create structured inputs with
      // memref-of-memref types rather than element memrefs.
      if (isa<MemRefType>(loadOp.getResult().getType()))
        continue;
      auto memrefType = dyn_cast<MemRefType>(loadOp.getMemref().getType());
      if (memrefType && memrefType.getRank() == 0)
        continue;

      auto map = tryBuildIndexingMap(loadOp.getIndices(), ivs, ctx);
      if (!map)
        return false;
      reads.push_back(
          {loadOp.getMemref(), *map, loadOp.getOperation(), /*isRead=*/true});
      sawAccess = true;
      continue;
    }

    if (auto storeOp = dyn_cast<memref::StoreOp>(&op)) {
      // Skip stores to pointer-to-memref wrappers.
      if (isa<MemRefType>(storeOp.getValueToStore().getType()))
        continue;
      auto memrefType = dyn_cast<MemRefType>(storeOp.getMemref().getType());
      if (memrefType && memrefType.getRank() == 0)
        continue;

      auto map = tryBuildIndexingMap(storeOp.getIndices(), ivs, ctx);
      if (!map)
        return false;
      writes.push_back({storeOp.getMemref(), *map, storeOp.getOperation(),
                        /*isRead=*/false});
      sawAccess = true;
      continue;
    }

    if (auto ifOp = dyn_cast<scf::IfOp>(&op)) {
      bool thenSawAccess = false;
      if (!collectMemrefAccessesImpl(scope, ifOp.getThenRegion().front(), ivs,
                                     reads, writes, ctx, thenSawAccess))
        return false;
      sawAccess |= thenSawAccess;

      if (!ifOp.getElseRegion().empty()) {
        bool elseSawAccess = false;
        if (!collectMemrefAccessesImpl(scope, ifOp.getElseRegion().front(), ivs,
                                       reads, writes, ctx, elseSawAccess))
          return false;
        sawAccess |= elseSawAccess;
      }
      continue;
    }

    if (auto nestedFor = dyn_cast<scf::ForOp>(&op)) {
      bool nestedSawAccess = false;
      if (!collectMemrefAccessesImpl(scope, *nestedFor.getBody(), ivs, reads,
                                     writes, ctx, nestedSawAccess))
        return false;
      sawAccess |= nestedSawAccess;
      continue;
    }

    if (auto nestedFor = dyn_cast<affine::AffineForOp>(&op)) {
      bool nestedSawAccess = false;
      if (!collectMemrefAccessesImpl(scope, *nestedFor.getBody(), ivs, reads,
                                     writes, ctx, nestedSawAccess))
        return false;
      sawAccess |= nestedSawAccess;
      continue;
    }

    if (auto ifOp = dyn_cast<affine::AffineIfOp>(&op)) {
      bool thenSawAccess = false;
      if (!collectMemrefAccessesImpl(scope, *ifOp.getThenBlock(), ivs, reads,
                                     writes, ctx, thenSawAccess))
        return false;
      sawAccess |= thenSawAccess;
      if (Block *elseBlock = ifOp.getElseBlock()) {
        bool elseSawAccess = false;
        if (!collectMemrefAccessesImpl(scope, *elseBlock, ivs, reads, writes,
                                       ctx, elseSawAccess))
          return false;
        sawAccess |= elseSawAccess;
      }
      continue;
    }

    if (isKnownPureScalarLibmCall(&op))
      continue;

    if (isMemoryEffectFree(&op))
      continue;

    return false;
  }

  return true;
}

} // namespace

namespace detail {

bool collectMemrefAccesses(Operation *scope, Block &body, ArrayRef<Value> ivs,
                           SmallVectorImpl<MemrefAccessEntry> &reads,
                           SmallVectorImpl<MemrefAccessEntry> &writes,
                           MLIRContext *ctx) {
  bool sawAccess = false;
  if (!collectMemrefAccessesImpl(scope, body, ivs, reads, writes, ctx,
                                 sawAccess))
    return false;
  return sawAccess;
}

} // namespace detail

} // namespace mlir::carts::sde
