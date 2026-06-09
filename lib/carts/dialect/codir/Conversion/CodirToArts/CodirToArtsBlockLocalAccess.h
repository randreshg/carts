///==========================================================================///
/// File: CodirToArtsBlockLocalAccess.h
///
/// Block-local access index materialization for CODIR-to-ARTS lowering.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALACCESS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALACCESS_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/codir/Utils/CodirConversionUtils.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/MathExtras.h"
#include <limits>
#include <optional>

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

struct PlannedBlockLocalAccessRewrite {
  Value localMemref;
  unsigned ownerDim = 0;
  Value ownerBase;
  Value localOrigin;
  int64_t lowerHalo = 0;
  Value groupedSourcePtr;
  unsigned ownerSlot = 0;
  int64_t blockSize = 1;
  int64_t groupBlockCount = 1;
  int64_t sourceDimExtent = ShapedType::kDynamic;
  bool grouped = false;
  bool allowFullWindowAccess = false;
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
    Value halo = createConstantIndex(builder, loc, lowerHalo);
    Value zero = createZeroIndex(builder, loc);
    Value canSubtract = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, localOrigin, halo);
    Value shifted = arith::SubIOp::create(builder, loc, localOrigin, halo);
    localOrigin =
        arith::SelectOp::create(builder, loc, canSubtract, shifted, zero);
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

static inline FailureOr<Value> materializeGroupedBlockLocalIndex(
    OpBuilder &builder, Location loc, Value index, Value ownerBase,
    int64_t lowerHalo, int64_t blockSize, int64_t groupBlockCount,
    int64_t sourceDimExtent, Value &relativeBlock,
    bool allowFullWindowAccess = false,
    const DenseMap<Value, Value> *sourceByBlockArgument = nullptr) {
  if (!index || !ownerBase || blockSize <= 0 || groupBlockCount <= 0)
    return failure();
  if (groupBlockCount > std::numeric_limits<int64_t>::max() / blockSize)
    return failure();
  if (!allowFullWindowAccess && !indexSelectsOwnerSlice(index, ownerBase))
    return failure();

  int64_t windowExtent = blockSize * groupBlockCount;
  struct WindowProof {
    Value ownerBase;
    int64_t windowExtent = 0;
    const DenseMap<Value, Value> *sourceByBlockArgument = nullptr;

    struct ConstantRange {
      int64_t lower = 0;
      int64_t upper = 0;
    };

    Value getSourceValue(Value candidate) const {
      if (!sourceByBlockArgument || !candidate)
        return {};
      auto it = sourceByBlockArgument->find(candidate);
      if (it == sourceByBlockArgument->end() || it->second == candidate)
        return {};
      return it->second;
    }

    static std::optional<int64_t> checkedAdd(int64_t lhs, int64_t rhs) {
      if (lhs < 0 || rhs < 0 || lhs > std::numeric_limits<int64_t>::max() - rhs)
        return std::nullopt;
      return lhs + rhs;
    }

    static std::optional<int64_t> checkedMul(int64_t lhs, int64_t rhs) {
      if (lhs < 0 || rhs < 0)
        return std::nullopt;
      if (lhs != 0 && rhs > std::numeric_limits<int64_t>::max() / lhs)
        return std::nullopt;
      return lhs * rhs;
    }

    static std::optional<int64_t> ceilDivNonNegative(int64_t lhs, int64_t rhs) {
      if (lhs < 0 || rhs <= 0)
        return std::nullopt;
      return llvm::divideCeil(lhs, rhs);
    }

    bool hasZeroOwnerBase() const {
      return ::mlir::carts::ValueAnalysis::isZeroConstant(ownerBase);
    }

    bool absoluteRangeStaysInWindow(Value candidate,
                                    bool allowEnd = false) const {
      if (!hasZeroOwnerBase())
        return false;
      std::optional<ConstantRange> range = getUnsignedRange(candidate);
      if (!range || range->lower < 0)
        return false;
      return allowEnd ? range->upper <= windowExtent
                      : range->upper < windowExtent;
    }

    std::optional<ConstantRange> getUnsignedRange(Value candidate,
                                                  unsigned depth = 0) const {
      if (!candidate || depth > 12)
        return std::nullopt;
      candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
      if (std::optional<int64_t> constant =
              ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(candidate)) {
        if (*constant < 0)
          return std::nullopt;
        return ConstantRange{*constant, *constant};
      }
      if (Value source = getSourceValue(candidate))
        if (std::optional<ConstantRange> range =
                getUnsignedRange(source, depth + 1))
          return range;

      if (auto blockArg = dyn_cast<BlockArgument>(candidate)) {
        auto loop =
            dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
        if (!loop || loop.getInductionVar() != candidate)
          return std::nullopt;
        if (!::mlir::carts::ValueAnalysis::isConstantAtLeastOne(loop.getStep()))
          return std::nullopt;
        std::optional<ConstantRange> lower =
            getUnsignedRange(loop.getLowerBound(), depth + 1);
        std::optional<ConstantRange> upper =
            getUnsignedRange(loop.getUpperBound(), depth + 1);
        if (!lower || !upper || upper->upper == 0)
          return std::nullopt;
        return ConstantRange{lower->lower, upper->upper - 1};
      }

      Operation *def = candidate.getDefiningOp();
      if (!def)
        return std::nullopt;
      auto rangeOfOperand = [&](Value operand) -> std::optional<ConstantRange> {
        return getUnsignedRange(operand, depth + 1);
      };

      if (auto add = dyn_cast<arith::AddIOp>(def)) {
        auto lhs = rangeOfOperand(add.getLhs());
        auto rhs = rangeOfOperand(add.getRhs());
        if (!lhs || !rhs)
          return std::nullopt;
        auto lower = checkedAdd(lhs->lower, rhs->lower);
        auto upper = checkedAdd(lhs->upper, rhs->upper);
        if (!lower || !upper)
          return std::nullopt;
        return ConstantRange{*lower, *upper};
      }
      if (auto sub = dyn_cast<arith::SubIOp>(def)) {
        auto lhs = rangeOfOperand(sub.getLhs());
        auto rhs = rangeOfOperand(sub.getRhs());
        if (!lhs || !rhs || lhs->lower < rhs->upper || lhs->upper < rhs->lower)
          return std::nullopt;
        return ConstantRange{lhs->lower - rhs->upper, lhs->upper - rhs->lower};
      }
      if (auto mul = dyn_cast<arith::MulIOp>(def)) {
        auto lhs = rangeOfOperand(mul.getLhs());
        auto rhs = rangeOfOperand(mul.getRhs());
        if (!lhs || !rhs)
          return std::nullopt;
        auto lower = checkedMul(lhs->lower, rhs->lower);
        auto upper = checkedMul(lhs->upper, rhs->upper);
        if (!lower || !upper)
          return std::nullopt;
        return ConstantRange{*lower, *upper};
      }
      if (auto div = dyn_cast<arith::DivUIOp>(def)) {
        auto lhs = rangeOfOperand(div.getLhs());
        auto rhs = rangeOfOperand(div.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        return ConstantRange{lhs->lower / rhs->upper, lhs->upper / rhs->lower};
      }
      if (auto div = dyn_cast<arith::DivSIOp>(def)) {
        auto lhs = rangeOfOperand(div.getLhs());
        auto rhs = rangeOfOperand(div.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        return ConstantRange{lhs->lower / rhs->upper, lhs->upper / rhs->lower};
      }
      if (auto div = dyn_cast<arith::CeilDivUIOp>(def)) {
        auto lhs = rangeOfOperand(div.getLhs());
        auto rhs = rangeOfOperand(div.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        auto lower = ceilDivNonNegative(lhs->lower, rhs->upper);
        auto upper = ceilDivNonNegative(lhs->upper, rhs->lower);
        if (!lower || !upper)
          return std::nullopt;
        return ConstantRange{*lower, *upper};
      }
      if (auto div = dyn_cast<arith::CeilDivSIOp>(def)) {
        auto lhs = rangeOfOperand(div.getLhs());
        auto rhs = rangeOfOperand(div.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        auto lower = ceilDivNonNegative(lhs->lower, rhs->upper);
        auto upper = ceilDivNonNegative(lhs->upper, rhs->lower);
        if (!lower || !upper)
          return std::nullopt;
        return ConstantRange{*lower, *upper};
      }
      if (auto rem = dyn_cast<arith::RemUIOp>(def)) {
        auto lhs = rangeOfOperand(rem.getLhs());
        auto rhs = rangeOfOperand(rem.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        return ConstantRange{0, std::min(lhs->upper, rhs->upper - 1)};
      }
      if (auto min = dyn_cast<arith::MinUIOp>(def)) {
        auto lhs = rangeOfOperand(min.getLhs());
        auto rhs = rangeOfOperand(min.getRhs());
        if (!lhs || !rhs)
          return std::nullopt;
        return ConstantRange{std::min(lhs->lower, rhs->lower),
                             std::min(lhs->upper, rhs->upper)};
      }
      if (auto max = dyn_cast<arith::MaxUIOp>(def)) {
        auto lhs = rangeOfOperand(max.getLhs());
        auto rhs = rangeOfOperand(max.getRhs());
        if (!lhs || !rhs)
          return std::nullopt;
        return ConstantRange{std::max(lhs->lower, rhs->lower),
                             std::max(lhs->upper, rhs->upper)};
      }
      if (auto select = dyn_cast<arith::SelectOp>(def)) {
        auto trueRange = rangeOfOperand(select.getTrueValue());
        auto falseRange = rangeOfOperand(select.getFalseValue());
        if (!trueRange || !falseRange)
          return std::nullopt;
        return ConstantRange{std::min(trueRange->lower, falseRange->lower),
                             std::max(trueRange->upper, falseRange->upper)};
      }
      return std::nullopt;
    }

    std::optional<int64_t> getOwnerRelativeConstant(Value candidate) const {
      std::optional<int64_t> candidateConst =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(candidate);
      std::optional<int64_t> ownerConst =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(ownerBase);
      if (candidateConst && ownerConst)
        return *candidateConst - *ownerConst;

      int64_t offset = 0;
      Value base =
          ::mlir::carts::ValueAnalysis::stripConstantOffset(candidate, &offset);
      if (::mlir::carts::ValueAnalysis::sameValue(base, ownerBase))
        return offset;
      return std::nullopt;
    }

    std::optional<std::pair<Value, int64_t>>
    splitAddConstant(Value candidate) const {
      candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
      if (auto add = candidate.getDefiningOp<arith::AddIOp>()) {
        if (std::optional<int64_t> rhs =
                ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
                    add.getRhs()))
          return std::make_pair(add.getLhs(), *rhs);
        if (std::optional<int64_t> lhs =
                ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
                    add.getLhs()))
          return std::make_pair(add.getRhs(), *lhs);
      }
      return std::nullopt;
    }

    bool pointStaysInWindow(Value candidate, unsigned depth = 0) const {
      if (!candidate || depth > 8)
        return false;
      if (absoluteRangeStaysInWindow(candidate))
        return true;
      std::optional<int64_t> offset = getOwnerRelativeConstant(candidate);
      if (offset)
        return *offset >= 0 && *offset < windowExtent;

      candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
      if (auto blockArg = dyn_cast<BlockArgument>(candidate)) {
        auto loop =
            dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
        if (!loop || loop.getInductionVar() != candidate)
          return false;
        if (!::mlir::carts::ValueAnalysis::isConstantAtLeastOne(loop.getStep()))
          return false;
        return pointStaysInWindow(loop.getLowerBound(), depth + 1) &&
               upperStaysInWindow(loop.getUpperBound(), depth + 1);
      }
      return false;
    }

    bool upperOffsetStaysInWindow(Value candidate) const {
      if (absoluteRangeStaysInWindow(candidate, /*allowEnd=*/true))
        return true;
      std::optional<int64_t> offset = getOwnerRelativeConstant(candidate);
      return offset && *offset >= 0 && *offset <= windowExtent;
    }

    bool pointPlusOffsetStaysInWindow(Value base, int64_t offset,
                                      unsigned depth) const {
      if (!base || offset < 0 || depth > 8)
        return false;
      if (hasZeroOwnerBase()) {
        std::optional<ConstantRange> range = getUnsignedRange(base);
        if (range && range->lower >= 0 && range->upper <= windowExtent - offset)
          return true;
      }
      if (std::optional<int64_t> baseOffset = getOwnerRelativeConstant(base))
        return *baseOffset >= 0 && *baseOffset + offset <= windowExtent;

      base = ::mlir::carts::ValueAnalysis::stripNumericCasts(base);
      auto blockArg = dyn_cast<BlockArgument>(base);
      if (!blockArg)
        return false;
      auto loop =
          dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
      if (!loop || loop.getInductionVar() != base)
        return false;
      std::optional<int64_t> step =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(loop.getStep());
      return step && *step > 0 && offset <= *step &&
             upperStaysInWindow(loop.getUpperBound(), depth + 1);
    }

    bool upperStaysInWindow(Value candidate, unsigned depth = 0) const {
      if (!candidate || depth > 8)
        return false;
      if (upperOffsetStaysInWindow(candidate))
        return true;
      if (auto add = splitAddConstant(candidate))
        if (pointPlusOffsetStaysInWindow(add->first, add->second, depth + 1))
          return true;
      candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
      if (auto min = candidate.getDefiningOp<arith::MinUIOp>())
        return upperStaysInWindow(min.getLhs(), depth + 1) ||
               upperStaysInWindow(min.getRhs(), depth + 1);
      if (auto min = candidate.getDefiningOp<arith::MinSIOp>())
        return upperStaysInWindow(min.getLhs(), depth + 1) ||
               upperStaysInWindow(min.getRhs(), depth + 1);
      return false;
    }
  };
  WindowProof proof{ownerBase, windowExtent, sourceByBlockArgument};
  auto pointStaysInWindow = [&](Value candidate) {
    return proof.pointStaysInWindow(candidate);
  };
  auto upperStaysInWindow = [&](Value candidate) {
    return proof.upperStaysInWindow(candidate);
  };

  bool provenInWindow = pointStaysInWindow(index);
  if (!provenInWindow) {
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
  // Full-window read acquires every backing block. A static source extent that
  // fits in that window proves every in-bounds source index selects one of
  // them.
  if (!provenInWindow && allowFullWindowAccess && sourceDimExtent >= 0 &&
      sourceDimExtent <= windowExtent)
    provenInWindow = true;
  if (!provenInWindow)
    return failure();

  Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
  Value relativeIndex =
      ::mlir::carts::ValueAnalysis::sameValue(index, ownerBase)
          ? createZeroIndex(builder, loc)
          : arith::SubIOp::create(builder, loc, index, ownerBase).getResult();
  relativeBlock =
      arith::DivUIOp::create(builder, loc, relativeIndex, blockSizeValue);
  Value blockOffset =
      arith::MulIOp::create(builder, loc, relativeBlock, blockSizeValue);
  Value blockBase = arith::AddIOp::create(builder, loc, ownerBase, blockOffset);

  Value blockPayloadBase = blockBase;
  if (lowerHalo > 0)
    blockPayloadBase = subtractClampZero(builder, loc, blockBase, lowerHalo);

  if (::mlir::carts::ValueAnalysis::sameValue(index, blockPayloadBase))
    return createZeroIndex(builder, loc);
  return arith::SubIOp::create(builder, loc, index, blockPayloadBase)
      .getResult();
}

static inline LogicalResult rewritePlannedBlockLocalAccesses(
    arts::EdtOp task, ArrayRef<PlannedBlockLocalAccessRewrite> rewrites,
    const DenseMap<Value, Value> *sourceByBlockArgument = nullptr) {
  if (rewrites.empty())
    return success();

  auto rewriteIndices = [&](Operation *op, MutableOperandRange memrefOperand,
                            MutableOperandRange indices) -> WalkResult {
    Value memref = memrefOperand[0].get();
    SmallVector<const PlannedBlockLocalAccessRewrite *, 4> matching;
    for (const PlannedBlockLocalAccessRewrite &rewrite : rewrites)
      if (memref == rewrite.localMemref)
        matching.push_back(&rewrite);
    if (matching.empty())
      return WalkResult::advance();

    bool hasGrouped = llvm::any_of(
        matching, [](const PlannedBlockLocalAccessRewrite *rewrite) {
          return rewrite->grouped;
        });
    if (hasGrouped) {
      Value sourcePtr;
      unsigned sourceRank = 0;
      for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
        if (!rewrite->grouped || !rewrite->groupedSourcePtr ||
            rewrite->blockSize <= 0 || rewrite->groupBlockCount <= 0) {
          op->emitError("grouped planned block-local access requires "
                        "block-window facts for every owner dimension");
          return WalkResult::interrupt();
        }
        if (!sourcePtr)
          sourcePtr = rewrite->groupedSourcePtr;
        if (sourcePtr != rewrite->groupedSourcePtr) {
          op->emitError("grouped planned block-local access mixes dependency "
                        "sources");
          return WalkResult::interrupt();
        }
        sourceRank = std::max<unsigned>(sourceRank, rewrite->ownerSlot + 1);
      }
      if (auto sourceType = dyn_cast<MemRefType>(sourcePtr.getType()))
        sourceRank = std::max<unsigned>(sourceRank, sourceType.getRank());
      if (sourceRank == 0)
        sourceRank = 1;

      OpBuilder builder(op);
      SmallVector<Value, 4> dbRefIndices(
          sourceRank, createZeroIndex(builder, op->getLoc()));
      for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
        if (indices.size() <= rewrite->ownerDim ||
            rewrite->ownerSlot >= dbRefIndices.size()) {
          op->emitError("grouped planned block-local access has malformed "
                        "owner-dimension facts");
          return WalkResult::interrupt();
        }
        Value relativeBlock;
        FailureOr<Value> localIndex = materializeGroupedBlockLocalIndex(
            builder, op->getLoc(), indices[rewrite->ownerDim].get(),
            rewrite->ownerBase, rewrite->lowerHalo, rewrite->blockSize,
            rewrite->groupBlockCount, rewrite->sourceDimExtent, relativeBlock,
            rewrite->allowFullWindowAccess, sourceByBlockArgument);
        if (failed(localIndex)) {
          op->emitError("grouped planned block-local access does not stay "
                        "within the block window");
          return WalkResult::interrupt();
        }
        dbRefIndices[rewrite->ownerSlot] = relativeBlock;
        indices[rewrite->ownerDim].set(*localIndex);
      }
      Value selectedPayload =
          arts::DbRefOp::create(builder, op->getLoc(), sourcePtr, dbRefIndices);
      memrefOperand.assign(ValueRange{selectedPayload});
      return WalkResult::advance();
    }

    for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
      if (indices.size() <= rewrite->ownerDim) {
        op->emitError("planned block-local access is missing the owner "
                      "dimension index");
        return WalkResult::interrupt();
      }

      OpBuilder builder(op);
      FailureOr<Value> localIndex = materializeBlockLocalIndex(
          builder, op->getLoc(), indices[rewrite->ownerDim].get(),
          rewrite->ownerBase, rewrite->localOrigin, rewrite->lowerHalo);
      if (failed(localIndex)) {
        op->emitError("planned block-local access does not stay within the "
                      "owner slice");
        return WalkResult::interrupt();
      }
      indices[rewrite->ownerDim].set(*localIndex);
    }
    return WalkResult::advance();
  };

  Block &body = task.getBody().front();
  WalkResult result = body.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteIndices(op, load.getMemrefMutable(),
                            load.getIndicesMutable());
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteIndices(op, store.getMemrefMutable(),
                            store.getIndicesMutable());
    return WalkResult::advance();
  });

  return result.wasInterrupted() ? failure() : success();
}


} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALACCESS_H
