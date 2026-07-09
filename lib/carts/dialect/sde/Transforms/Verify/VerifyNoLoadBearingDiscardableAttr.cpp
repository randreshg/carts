///==========================================================================///
/// File: VerifyNoLoadBearingDiscardableAttr.cpp
///
/// Fail closed when distribution facts ride discardable attrs on non-CARTS ops.
///==========================================================================///

#include "carts/dialect/arts/Utils/ArtsAttrNames.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYNOLOADBEARINGDISCARDABLEATTR
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSet.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

bool isCartsDialectOperation(Operation *op) {
  if (!op || !op->getDialect())
    return false;
  StringRef ns = op->getDialect()->getNamespace();
  return ns == "sde" || ns == "arts" || ns == "arts_rt";
}

bool isAllowedNonCartsModuleAttr(StringRef name) {
  return name == arts::AttrNames::Module::RuntimeConfigPath ||
         name == arts::AttrNames::Module::RuntimeConfigData ||
         name == arts::AttrNames::Module::RuntimeTotalWorkers ||
         name == arts::AttrNames::Module::RuntimeTotalNodes ||
         name == arts::AttrNames::Module::RuntimeStaticWorkers;
}

bool isLoadBearingDiscardableAttr(StringRef attrName) {
  static const llvm::StringSet<> names = [] {
    llvm::StringSet<> set;
    set.insert("arrayLayout");
    set.insert("physicalOwnerDims");
    set.insert("physicalBlockShape");
    set.insert("groupBlockCount");
    set.insert("dep_pattern");
    set.insert("distribution_kind");
    set.insert("distribution_pattern");
    set.insert("distribution_version");
    set.insert("stencil_center_offset");
    set.insert("stencil_min_offsets");
    set.insert("stencil_max_offsets");
    set.insert("stencil_spatial_dims");
    set.insert("stencil_owner_dims");
    set.insert("stencil_write_footprint");
    set.insert("stencil_supported_block_halo");
    set.insert(sde::AttrNames::LayoutChoice);
    set.insert(arts::AttrNames::Semantic::NarrowableDep);
    return set;
  }();
  return names.contains(attrName);
}

struct VerifyNoLoadBearingDiscardableAttrPass
    : public sde::impl::VerifyNoLoadBearingDiscardableAttrBase<
          VerifyNoLoadBearingDiscardableAttrPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool failed = false;

    module.walk([&](Operation *op) {
      if (isCartsDialectOperation(op))
        return;
      for (NamedAttribute attr : op->getDiscardableAttrs()) {
        if (isa<ModuleOp>(op) && isAllowedNonCartsModuleAttr(attr.getName()))
          continue;
        if (!isLoadBearingDiscardableAttr(attr.getName()))
          continue;
        op->emitOpError()
            << "load-bearing discardable attribute '" << attr.getName()
            << "' on non-CARTS operation; distribution facts must live in "
               "typed CARTS IR or AnalysisManager results";
        failed = true;
      }
    });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::sde::createVerifyNoLoadBearingDiscardableAttrPass() {
  return std::make_unique<VerifyNoLoadBearingDiscardableAttrPass>();
}
