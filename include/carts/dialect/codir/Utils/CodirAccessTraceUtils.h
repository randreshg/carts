///==========================================================================///
/// File: CodirAccessTraceUtils.h
///
/// SDE-free owner-dim access-tracing helpers shared by the CODIR
/// StoragePlanning transform and the CODIR -> ARTS conversion. These were split
/// out of CodirConversionUtils.h so CODIR transforms that only need to trace a
/// memref access back to a dependency root do not take a transitive dependency
/// on the SDE dialect. Keep this header free of any `sde::` symbol.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_UTILS_CODIRACCESSTRACEUTILS_H
#define CARTS_DIALECT_CODIR_UTILS_CODIRACCESSTRACEUTILS_H
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <optional>
using namespace mlir;
using namespace mlir::carts;

namespace {

static inline bool indexSelectsOwnerSlice(Value index, Value ownerIv,
                                          llvm::SmallPtrSetImpl<Value> &seen) {
  if (!index || !ownerIv)
    return false;
  index = ::mlir::carts::ValueAnalysis::stripNumericCasts(index);
  ownerIv = ::mlir::carts::ValueAnalysis::stripNumericCasts(ownerIv);
  if (index == ownerIv)
    return true;
  if (!seen.insert(index).second)
    return false;

  Operation *def = index.getDefiningOp();
  if (auto rem = dyn_cast_or_null<arith::RemUIOp>(def))
    if (::mlir::carts::ValueAnalysis::isOneConstant(rem.getRhs()))
      return false;
  if (auto rem = dyn_cast_or_null<arith::RemSIOp>(def))
    if (::mlir::carts::ValueAnalysis::isOneConstant(rem.getRhs()))
      return false;
  if (auto div = dyn_cast_or_null<arith::DivUIOp>(def))
    if (::mlir::carts::ValueAnalysis::isOneConstant(div.getRhs()))
      return indexSelectsOwnerSlice(div.getLhs(), ownerIv, seen);
  if (auto div = dyn_cast_or_null<arith::DivSIOp>(def))
    if (::mlir::carts::ValueAnalysis::isOneConstant(div.getRhs()))
      return indexSelectsOwnerSlice(div.getLhs(), ownerIv, seen);

  if (::mlir::carts::ValueAnalysis::dependsOn(index, ownerIv))
    return true;

  auto blockArg = dyn_cast<BlockArgument>(index);
  if (blockArg) {
    auto loop =
        dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
    if (!loop || loop.getInductionVar() != index)
      return false;

    llvm::SmallPtrSet<Value, 8> lowerSeen;
    llvm::SmallPtrSet<Value, 8> upperSeen;
    for (Value value : seen) {
      lowerSeen.insert(value);
      upperSeen.insert(value);
    }
    return indexSelectsOwnerSlice(loop.getLowerBound(), ownerIv, lowerSeen) &&
           indexSelectsOwnerSlice(loop.getUpperBound(), ownerIv, upperSeen);
  }

  if (!isa_and_nonnull<
          arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp,
          arith::DivUIOp, arith::RemSIOp, arith::RemUIOp, arith::IndexCastOp,
          arith::IndexCastUIOp, arith::ExtSIOp, arith::ExtUIOp, arith::TruncIOp,
          arith::MinSIOp, arith::MinUIOp, arith::MaxSIOp, arith::MaxUIOp>(def))
    return false;

  for (Value operand : def->getOperands()) {
    llvm::SmallPtrSet<Value, 8> operandSeen;
    for (Value value : seen)
      operandSeen.insert(value);
    if (indexSelectsOwnerSlice(operand, ownerIv, operandSeen))
      return true;
  }
  return false;
}

static inline bool indexSelectsOwnerSlice(Value index, Value ownerIv) {
  llvm::SmallPtrSet<Value, 8> seen;
  return indexSelectsOwnerSlice(index, ownerIv, seen);
}

static inline bool indexSelectsOwnerSlice(OpFoldResult index, Value ownerIv) {
  if (auto value = dyn_cast<Value>(index))
    return indexSelectsOwnerSlice(value, ownerIv);
  return false;
}

enum class CodirAccessTraceStatus { NotRooted, Unsupported, Rooted };

struct CodirAccessOwnerDims {
  CodirAccessTraceStatus status = CodirAccessTraceStatus::NotRooted;
  SmallVector<unsigned> ownerDims;
};

static inline void addUniqueCodirOwnerDim(SmallVectorImpl<unsigned> &dims,
                                          unsigned dim) {
  if (!llvm::is_contained(dims, dim))
    dims.push_back(dim);
}

static inline bool
remapCodirSubviewOwnerDims(memref::SubViewOp subview,
                           SmallVectorImpl<unsigned> &selectedDims,
                           Value ownerBase) {
  std::optional<unsigned> sourceRank =
      ::mlir::carts::ValueAnalysis::getMemrefRank(subview.getSource());
  std::optional<unsigned> resultRank =
      ::mlir::carts::ValueAnalysis::getMemrefRank(subview.getResult());
  if (!sourceRank || !resultRank ||
      subview.getMixedOffsets().size() != *sourceRank)
    return false;

  llvm::SmallBitVector droppedDims = subview.getDroppedDims();
  if (droppedDims.size() != *sourceRank)
    return false;

  SmallVector<unsigned> remappedDims;
  unsigned resultDim = 0;
  for (auto [sourceDim, offset] : llvm::enumerate(subview.getMixedOffsets())) {
    bool offsetSelectsOwner = indexSelectsOwnerSlice(offset, ownerBase);
    if (droppedDims.test(sourceDim)) {
      if (offsetSelectsOwner)
        addUniqueCodirOwnerDim(remappedDims, static_cast<unsigned>(sourceDim));
      continue;
    }

    if (resultDim >= *resultRank)
      return false;
    if (offsetSelectsOwner || llvm::is_contained(selectedDims, resultDim))
      addUniqueCodirOwnerDim(remappedDims, static_cast<unsigned>(sourceDim));
    ++resultDim;
  }

  if (resultDim != *resultRank)
    return false;
  selectedDims.assign(remappedDims.begin(), remappedDims.end());
  return true;
}

static inline CodirAccessOwnerDims
traceCodirAccessToRoot(Value memref, ArrayRef<Value> indices, Value root,
                       Value ownerBase) {
  std::optional<unsigned> currentRank =
      ::mlir::carts::ValueAnalysis::getMemrefRank(memref);
  bool unsupportedMapping = !currentRank || indices.size() != *currentRank;
  SmallVector<unsigned> selectedDims;
  if (!unsupportedMapping)
    for (auto [dim, index] : llvm::enumerate(indices))
      if (indexSelectsOwnerSlice(index, ownerBase))
        addUniqueCodirOwnerDim(selectedDims, static_cast<unsigned>(dim));

  Value current = memref;
  llvm::SmallPtrSet<Value, 8> seen;
  while (current != root) {
    if (!current || !seen.insert(current).second)
      return {CodirAccessTraceStatus::Unsupported, {}};

    Operation *def = current.getDefiningOp();
    if (!def)
      return {CodirAccessTraceStatus::NotRooted, {}};

    if (auto cast = dyn_cast<memref::CastOp>(def)) {
      std::optional<unsigned> sourceRank =
          ::mlir::carts::ValueAnalysis::getMemrefRank(cast.getSource());
      if (!sourceRank || !currentRank || *sourceRank != *currentRank)
        unsupportedMapping = true;
      current = cast.getSource();
      currentRank = sourceRank;
      continue;
    }

    if (auto subview = dyn_cast<memref::SubViewOp>(def)) {
      std::optional<unsigned> sourceRank =
          ::mlir::carts::ValueAnalysis::getMemrefRank(subview.getSource());
      if (!unsupportedMapping &&
          !remapCodirSubviewOwnerDims(subview, selectedDims, ownerBase))
        unsupportedMapping = true;
      current = subview.getSource();
      currentRank = sourceRank;
      continue;
    }

    if (auto subindex = dyn_cast<polygeist::SubIndexOp>(def)) {
      std::optional<unsigned> sourceRank =
          ::mlir::carts::ValueAnalysis::getMemrefRank(subindex.getSource());
      if (!sourceRank || !currentRank || *sourceRank != *currentRank + 1) {
        unsupportedMapping = true;
        current = subindex.getSource();
        currentRank = sourceRank;
        continue;
      }

      for (unsigned &dim : selectedDims)
        ++dim;
      if (indexSelectsOwnerSlice(subindex.getIndex(), ownerBase))
        addUniqueCodirOwnerDim(selectedDims, 0);
      current = subindex.getSource();
      currentRank = sourceRank;
      continue;
    }

    if (::mlir::carts::ValueAnalysis::isDerivedFromPtr(current, root))
      return {CodirAccessTraceStatus::Unsupported, {}};
    return {CodirAccessTraceStatus::NotRooted, {}};
  }

  std::optional<unsigned> rootRank =
      ::mlir::carts::ValueAnalysis::getMemrefRank(root);
  if (!rootRank || !currentRank || *rootRank != *currentRank ||
      unsupportedMapping)
    return {CodirAccessTraceStatus::Unsupported, {}};
  for (unsigned dim : selectedDims)
    if (dim >= *rootRank)
      return {CodirAccessTraceStatus::Unsupported, {}};
  return {CodirAccessTraceStatus::Rooted, std::move(selectedDims)};
}

struct CodirMemoryAccessInfo {
  Operation *op = nullptr;
  Value memref;
  SmallVector<Value> indices;
};

static inline std::optional<CodirMemoryAccessInfo>
getCodirMemoryAccessInfo(Operation *op) {
  if (!op)
    return std::nullopt;

  if (auto load = dyn_cast<memref::LoadOp>(op))
    return CodirMemoryAccessInfo{
        op, load.getMemRef(),
        SmallVector<Value>(load.getIndices().begin(), load.getIndices().end())};
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return CodirMemoryAccessInfo{op, store.getMemRef(),
                                 SmallVector<Value>(store.getIndices().begin(),
                                                    store.getIndices().end())};
  if (auto load = dyn_cast<polygeist::DynLoadOp>(op))
    return CodirMemoryAccessInfo{
        op, load.getMemref(),
        SmallVector<Value>(load.getIndices().begin(), load.getIndices().end())};
  if (auto store = dyn_cast<polygeist::DynStoreOp>(op))
    return CodirMemoryAccessInfo{op, store.getMemref(),
                                 SmallVector<Value>(store.getIndices().begin(),
                                                    store.getIndices().end())};
  if (auto load = dyn_cast<affine::AffineLoadOp>(op))
    return CodirMemoryAccessInfo{
        op, load.getMemRef(),
        SmallVector<Value>(load.getMapOperands().begin(),
                           load.getMapOperands().end())};
  if (auto store = dyn_cast<affine::AffineStoreOp>(op))
    return CodirMemoryAccessInfo{
        op, store.getMemRef(),
        SmallVector<Value>(store.getMapOperands().begin(),
                           store.getMapOperands().end())};
  return std::nullopt;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_UTILS_CODIRACCESSTRACEUTILS_H
