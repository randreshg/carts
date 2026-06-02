///==========================================================================///
/// File: PromoteTargetAttrs.cpp
///
/// Copy `polygeist.target-cpu` / `polygeist.target-features` from the
/// frontend-emitted module attrs into target-neutral `carts.target-cpu` /
/// `carts.target-features` so downstream CARTS passes observe a stable target
/// description after the upstream LLVM-conversion passes strip foreign module
/// attrs.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_PROMOTETARGETATTRS
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/utils/Debug.h"
#include "carts/utils/TargetAttributes.h"

ARTS_DEBUG_SETUP(sde_promote_target_attrs);

using namespace mlir;
using namespace mlir::carts;

namespace {

constexpr llvm::StringLiteral kPolygeistTargetCpu = "polygeist.target-cpu";
constexpr llvm::StringLiteral kPolygeistTargetFeatures =
    "polygeist.target-features";

struct PromoteTargetAttrsPass
    : public sde::impl::PromoteTargetAttrsBase<PromoteTargetAttrsPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    if (!target::getTargetCpu(module)) {
      if (auto attr = module->getAttrOfType<StringAttr>(kPolygeistTargetCpu)) {
        target::setTargetCpu(module, attr.getValue());
        ARTS_DEBUG("Promoted polygeist.target-cpu='"
                   << attr.getValue() << "' to carts.target-cpu");
      }
    }

    if (!target::getTargetFeatures(module)) {
      if (auto attr =
              module->getAttrOfType<StringAttr>(kPolygeistTargetFeatures)) {
        target::setTargetFeatures(module, attr.getValue());
        ARTS_DEBUG(
            "Promoted polygeist.target-features to carts.target-features ("
            << attr.getValue().size() << " bytes)");
      }
    }
  }
};

} // namespace
