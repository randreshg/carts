///==========================================================================///
/// File: CodirToArtsBlockLocalIndex.h
///
/// Block-local index and grouped block-window index materialization.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALINDEX_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALINDEX_H

#include "CodirToArtsBlockWindowProof.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/codir/Utils/CodirConversionUtils.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/STLExtras.h"

namespace {

static inline Value subtractClampZero(OpBuilder &builder, Location loc,
                                      Value value, int64_t amount) {
  if (amount <= 0)
    return value;
  Value offset = createConstantIndex(builder, loc, amount);
  Value zero = createZeroIndex(builder, loc);
  Value canSubtract = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::uge, value, offset);
  Value shifted = arith::SubIOp::create(builder, loc, value, offset);
  return arith::SelectOp::create(builder, loc, canSubtract, shifted, zero);
}

static inline Value subtractStorageHalo(OpBuilder &builder, Location loc,
                                        Value value, int64_t amount) {
  if (amount <= 0)
    return value;
  return arith::SubIOp::create(builder, loc, value,
                               createConstantIndex(builder, loc, amount));
}

struct PlannedBlockLocalAccessRewrite {
  Value localMemref;
  unsigned ownerDim = 0;
  Value ownerBase;
  Value localOrigin;
  int64_t lowerHalo = 0;
  int64_t upperHalo = 0;
  Value groupedSourcePtr;
  unsigned ownerSlot = 0;
  int64_t blockSize = 1;
  int64_t groupBlockCount = 1;
  int64_t ownerWindowExtent = 1;
  int64_t sourceDimExtent = ShapedType::kDynamic;
  bool grouped = false;
  bool allowFullWindowAccess = false;
  bool requireOwnerWindowProof = false;
};

static inline Value materializeBlockLocalOrigin(OpBuilder &builder,
                                                Location loc, Value ownerBase,
                                                Value ownerDomainBase,
                                                int64_t blockSize) {
  Value localOrigin = ownerBase;
  if (blockSize > 1) {
    if (!ownerDomainBase)
      ownerDomainBase = createZeroIndex(builder, loc);
    Value relativeBase =
        ::mlir::carts::ValueAnalysis::sameValue(ownerBase, ownerDomainBase)
            ? createZeroIndex(builder, loc)
            : arith::SubIOp::create(builder, loc, ownerBase, ownerDomainBase)
                  .getResult();
    Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
    Value blockIndex =
        arith::DivUIOp::create(builder, loc, relativeBase, blockSizeValue);
    Value blockOffset =
        arith::MulIOp::create(builder, loc, blockIndex, blockSizeValue);
    localOrigin =
        arith::AddIOp::create(builder, loc, ownerDomainBase, blockOffset);
  }
  return localOrigin;
}

static inline FailureOr<Value>
materializeBlockLocalIndex(OpBuilder &builder, Location loc, Value index,
                           Value ownerBase, Value localOrigin,
                           int64_t lowerHalo) {
  if (!index || !ownerBase || !localOrigin)
    return failure();
  if (lowerHalo > 0) {
    localOrigin = subtractStorageHalo(builder, loc, localOrigin, lowerHalo);
  }
  if (::mlir::carts::ValueAnalysis::sameValue(index, localOrigin))
    return createZeroIndex(builder, loc);
  if (auto sub = index.getDefiningOp<arith::SubIOp>())
    if (::mlir::carts::ValueAnalysis::sameValue(sub.getRhs(), localOrigin))
      return index;
  if (!indexSelectsOwnerSlice(index, ownerBase))
    return failure();
  return arith::SubIOp::create(builder, loc, index, localOrigin).getResult();
}

static inline std::optional<std::pair<Value, int64_t>>
splitAddOrSubConstant(Value candidate) {
  candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
  if (auto add = candidate.getDefiningOp<arith::AddIOp>()) {
    if (std::optional<int64_t> rhs =
            ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(add.getRhs()))
      return std::make_pair(add.getLhs(), *rhs);
    if (std::optional<int64_t> lhs =
            ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(add.getLhs()))
      return std::make_pair(add.getRhs(), *lhs);
  }
  if (auto sub = candidate.getDefiningOp<arith::SubIOp>())
    if (std::optional<int64_t> rhs =
            ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(sub.getRhs()))
      return std::make_pair(sub.getLhs(), -*rhs);
  return std::nullopt;
}

static inline FailureOr<Value> materializeGroupedBlockLocalIndex(
    OpBuilder &builder, Location loc, Value index, Value ownerBase,
    Value windowBase, int64_t lowerHalo, int64_t upperHalo, int64_t blockSize,
    int64_t groupBlockCount, int64_t ownerWindowExtent, int64_t sourceDimExtent,
    Value &relativeBlock, bool allowFullWindowAccess = false,
    bool requireOwnerWindowProof = false,
    const DenseMap<Value, Value> *sourceByBlockArgument = nullptr) {
  if (!index || !ownerBase || !windowBase || blockSize <= 0 ||
      groupBlockCount <= 0 || ownerWindowExtent <= 0 || lowerHalo < 0 ||
      upperHalo < 0)
    return failure();
  if (groupBlockCount > std::numeric_limits<int64_t>::max() / blockSize)
    return failure();
  if (!allowFullWindowAccess && !indexSelectsOwnerSlice(index, ownerBase))
    return failure();

  int64_t windowExtent = blockSize * groupBlockCount;
  BlockWindowProof proof{windowBase, windowExtent, sourceByBlockArgument};
  auto pointStaysInWindow = [&](Value candidate) {
    return proof.pointStaysInWindow(candidate);
  };
  auto upperStaysInWindow = [&](Value candidate) {
    return proof.upperStaysInWindow(candidate);
  };
  BlockWindowProof ownerProof{ownerBase, ownerWindowExtent,
                              sourceByBlockArgument};
  auto pointStaysInOwnerWindow = [&](Value candidate) {
    if (ownerWindowExtent <= 0)
      return false;
    if (ownerWindowExtent > std::numeric_limits<int64_t>::max() - upperHalo)
      return false;
    int64_t expandedUpperExtent = ownerWindowExtent + upperHalo;
    if (std::optional<int64_t> offset =
            ownerProof.getOwnerRelativeConstant(candidate))
      return *offset >= -lowerHalo && *offset < expandedUpperExtent;
    if (std::optional<std::pair<Value, int64_t>> split =
            splitAddOrSubConstant(candidate)) {
      if ((split->second < 0 && -split->second <= lowerHalo) ||
          (split->second > 0 && split->second <= upperHalo))
        return ownerProof.pointStaysInWindow(split->first);
    }
    return ownerProof.pointStaysInWindow(candidate);
  };
  auto upperStaysInOwnerWindow = [&](Value candidate) {
    if (ownerWindowExtent <= 0)
      return false;
    if (ownerWindowExtent > std::numeric_limits<int64_t>::max() - upperHalo)
      return false;
    int64_t expandedUpperExtent = ownerWindowExtent + upperHalo;
    if (std::optional<int64_t> offset =
            ownerProof.getOwnerRelativeConstant(candidate))
      return *offset >= -lowerHalo && *offset <= expandedUpperExtent;
    if (std::optional<std::pair<Value, int64_t>> split =
            splitAddOrSubConstant(candidate)) {
      if ((split->second < 0 && -split->second <= lowerHalo) ||
          (split->second > 0 && split->second <= upperHalo))
        return ownerProof.upperStaysInWindow(split->first);
    }
    return ownerProof.upperStaysInWindow(candidate);
  };

  bool provenInWindow = false;
  if (!requireOwnerWindowProof)
    provenInWindow = pointStaysInWindow(index);
  if (!provenInWindow && !requireOwnerWindowProof) {
    if (auto blockArg = dyn_cast<BlockArgument>(index)) {
      auto loop =
          dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
      if (loop && loop.getInductionVar() == index &&
          ::mlir::carts::ValueAnalysis::isConstantAtLeastOne(loop.getStep())) {
        provenInWindow = pointStaysInWindow(loop.getLowerBound()) &&
                         upperStaysInWindow(loop.getUpperBound());
      }
    }
  }
  if (!provenInWindow) {
    provenInWindow = pointStaysInOwnerWindow(index);
    if (!provenInWindow) {
      if (auto blockArg = dyn_cast<BlockArgument>(index)) {
        auto loop =
            dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
        if (loop && loop.getInductionVar() == index &&
            ::mlir::carts::ValueAnalysis::isConstantAtLeastOne(
                loop.getStep())) {
          provenInWindow = pointStaysInOwnerWindow(loop.getLowerBound()) &&
                           upperStaysInOwnerWindow(loop.getUpperBound());
        }
      }
    }
  }
  // Full-window read acquires every backing block. A static source extent that
  // fits in that window proves every in-bounds source index selects one of
  // them.
  if (!provenInWindow && allowFullWindowAccess && sourceDimExtent >= 0 &&
      sourceDimExtent <= windowExtent)
    provenInWindow = true;
  if (!provenInWindow)
    return failure();

  Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
  Value blockSelectIndex = index;
  if (lowerHalo > 0)
    blockSelectIndex = arith::MaxUIOp::create(builder, loc, index, windowBase);
  if (upperHalo > 0) {
    Value windowLastIndex = arith::AddIOp::create(
        builder, loc, windowBase,
        createConstantIndex(builder, loc, windowExtent - 1));
    blockSelectIndex =
        arith::MinUIOp::create(builder, loc, blockSelectIndex, windowLastIndex);
  }
  Value relativeIndex =
      ::mlir::carts::ValueAnalysis::sameValue(blockSelectIndex, windowBase)
          ? createZeroIndex(builder, loc)
          : arith::SubIOp::create(builder, loc, blockSelectIndex, windowBase)
                .getResult();
  relativeBlock =
      arith::DivUIOp::create(builder, loc, relativeIndex, blockSizeValue);
  Value blockOffset =
      arith::MulIOp::create(builder, loc, relativeBlock, blockSizeValue);
  Value blockBase =
      arith::AddIOp::create(builder, loc, windowBase, blockOffset);

  Value blockPayloadBase = blockBase;
  if (lowerHalo > 0)
    blockPayloadBase = subtractStorageHalo(builder, loc, blockBase, lowerHalo);

  if (::mlir::carts::ValueAnalysis::sameValue(index, blockPayloadBase))
    return createZeroIndex(builder, loc);
  return arith::SubIOp::create(builder, loc, index, blockPayloadBase)
      .getResult();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALINDEX_H
