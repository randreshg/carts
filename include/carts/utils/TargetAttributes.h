#ifndef CARTS_UTILS_TARGETATTRIBUTES_H
#define CARTS_UTILS_TARGETATTRIBUTES_H

#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/StringRef.h"
#include <optional>

namespace mlir::carts::target {
namespace AttrNames::Module {

inline constexpr llvm::StringLiteral TargetCpu = "carts.target-cpu";
inline constexpr llvm::StringLiteral TargetFeatures = "carts.target-features";

} // namespace AttrNames::Module

inline std::optional<StringRef> getTargetCpu(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr =
          module->getAttrOfType<StringAttr>(AttrNames::Module::TargetCpu))
    return attr.getValue();
  return std::nullopt;
}

inline void setTargetCpu(ModuleOp module, StringRef cpu) {
  if (!module || cpu.empty())
    return;
  module->setAttr(AttrNames::Module::TargetCpu,
                  StringAttr::get(module.getContext(), cpu));
}

inline std::optional<StringRef> getTargetFeatures(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr =
          module->getAttrOfType<StringAttr>(AttrNames::Module::TargetFeatures))
    return attr.getValue();
  return std::nullopt;
}

inline void setTargetFeatures(ModuleOp module, StringRef features) {
  if (!module || features.empty())
    return;
  module->setAttr(AttrNames::Module::TargetFeatures,
                  StringAttr::get(module.getContext(), features));
}

} // namespace mlir::carts::target

#endif // CARTS_UTILS_TARGETATTRIBUTES_H
