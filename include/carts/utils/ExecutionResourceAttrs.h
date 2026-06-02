///==========================================================================///
/// File: ExecutionResourceAttrs.h
///
/// Target-neutral module execution-resource attributes.
///==========================================================================///

#ifndef CARTS_UTILS_EXECUTIONRESOURCEATTRS_H
#define CARTS_UTILS_EXECUTIONRESOURCEATTRS_H

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/StringRef.h"

#include <optional>

namespace mlir::carts::AttrNames::Execution {

inline constexpr llvm::StringLiteral LogicalTotalWorkers =
    "carts.logical_total_workers";
inline constexpr llvm::StringLiteral LogicalTotalLocalities =
    "carts.logical_total_localities";

} // namespace mlir::carts::AttrNames::Execution

namespace mlir::carts {

inline std::optional<int64_t> getLogicalTotalWorkers(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr = module->getAttrOfType<IntegerAttr>(
          AttrNames::Execution::LogicalTotalWorkers))
    return attr.getInt();
  return std::nullopt;
}

inline void setLogicalTotalWorkers(ModuleOp module, int64_t workers) {
  if (!module || workers <= 0)
    return;
  module->setAttr(
      AttrNames::Execution::LogicalTotalWorkers,
      IntegerAttr::get(IntegerType::get(module.getContext(), 64), workers));
}

inline std::optional<int64_t> getLogicalTotalLocalities(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr = module->getAttrOfType<IntegerAttr>(
          AttrNames::Execution::LogicalTotalLocalities))
    return attr.getInt();
  return std::nullopt;
}

inline void setLogicalTotalLocalities(ModuleOp module, int64_t localities) {
  if (!module || localities <= 0)
    return;
  module->setAttr(
      AttrNames::Execution::LogicalTotalLocalities,
      IntegerAttr::get(IntegerType::get(module.getContext(), 64), localities));
}

} // namespace mlir::carts

#endif // CARTS_UTILS_EXECUTIONRESOURCEATTRS_H
