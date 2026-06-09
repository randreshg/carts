///==========================================================================///
/// File: CodirToArtsBlockWindowProof.h
///
/// Proof queries for grouped block-window read materialization.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKWINDOWPROOF_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKWINDOWPROOF_H

#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/MathExtras.h"
#include <limits>
#include <optional>

namespace {

struct BlockWindowProof {
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

  bool absoluteRangeStaysInWindow(Value candidate, bool allowEnd = false) const {
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
              ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(add.getRhs()))
        return std::make_pair(add.getLhs(), *rhs);
      if (std::optional<int64_t> lhs =
              ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(add.getLhs()))
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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKWINDOWPROOF_H
