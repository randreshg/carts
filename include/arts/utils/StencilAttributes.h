///==========================================================================///
/// File: StencilAttributes.h
///
/// Stencil and halo attribute getter/setter functions for ARTS operations.
///==========================================================================///

#ifndef CARTS_UTILS_STENCILATTRIBUTES_H
#define CARTS_UTILS_STENCILATTRIBUTES_H

#include "arts/Dialect.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/StringRef.h"
#include <optional>

namespace mlir {
namespace arts {
namespace AttrNames {
namespace Operation {
namespace Stencil {
using namespace llvm;
constexpr StringLiteral StencilCenterOffset("stencil_center_offset");
constexpr StringLiteral ElementStride("element_stride");
constexpr StringLiteral LeftHaloArgIdx("left_halo_arg_idx");
constexpr StringLiteral RightHaloArgIdx("right_halo_arg_idx");
} // namespace Stencil
} // namespace Operation
} // namespace AttrNames

inline std::optional<int64_t> getStencilCenterOffset(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::Stencil::StencilCenterOffset))
    return attr.getInt();
  return std::nullopt;
}

inline void setStencilCenterOffset(Operation *op, int64_t centerOffset) {
  if (!op)
    return;
  op->setAttr(
      AttrNames::Operation::Stencil::StencilCenterOffset,
      IntegerAttr::get(IntegerType::get(op->getContext(), 64), centerOffset));
}

inline std::optional<int64_t> getElementStride(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::Stencil::ElementStride))
    return attr.getInt();
  return std::nullopt;
}

inline void setElementStride(Operation *op, int64_t stride) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::Stencil::ElementStride,
              IntegerAttr::get(IndexType::get(op->getContext()), stride));
}

inline std::optional<unsigned> getLeftHaloArgIndex(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::Stencil::LeftHaloArgIdx)) {
    int64_t value = attr.getInt();
    if (value >= 0)
      return static_cast<unsigned>(value);
  }
  return std::nullopt;
}

inline std::optional<unsigned> getRightHaloArgIndex(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::Stencil::RightHaloArgIdx)) {
    int64_t value = attr.getInt();
    if (value >= 0)
      return static_cast<unsigned>(value);
  }
  return std::nullopt;
}

inline void setLeftHaloArgIndex(Operation *op, unsigned argIndex) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::Stencil::LeftHaloArgIdx,
              IntegerAttr::get(IndexType::get(op->getContext()), argIndex));
}

inline void setRightHaloArgIndex(Operation *op, unsigned argIndex) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::Stencil::RightHaloArgIdx,
              IntegerAttr::get(IndexType::get(op->getContext()), argIndex));
}

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_STENCILATTRIBUTES_H
