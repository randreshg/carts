///==========================================================================///
/// File: SdeExposeLegalBlocks.cpp
///
/// Phase B legal-block exposure: refine committed SDE layout facts with a
/// node-agnostic budgetBlockShape from SdeMemoryLegality.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDEEXPOSELEGALBLOCKS
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAccessRelation.h"
#include "carts/dialect/sde/Analysis/SdeCommVolumeCost.h"
#include "carts/dialect/sde/Analysis/SdeDependence.h"
#include "carts/dialect/sde/Analysis/SdeMemoryLegality.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

static std::optional<sde::SdeAccessMode>
modeForRole(sde::LayoutGraphRole role) {
  switch (role) {
  case sde::LayoutGraphRole::read:
    return sde::SdeAccessMode::read;
  case sde::LayoutGraphRole::write:
    return sde::SdeAccessMode::write;
  case sde::LayoutGraphRole::unknown:
    return std::nullopt;
  }
  return std::nullopt;
}

static std::optional<SmallVector<int64_t, 4>> staticShape(Value root) {
  auto type = dyn_cast_or_null<MemRefType>(root ? root.getType() : Type{});
  if (!type || !type.hasStaticShape())
    return std::nullopt;
  return SmallVector<int64_t, 4>(type.getShape().begin(),
                                 type.getShape().end());
}

static bool isPositiveShape(ArrayRef<int64_t> shape) {
  return !shape.empty() &&
         llvm::all_of(shape, [](int64_t dim) { return dim > 0; });
}

static DictionaryAttr replaceBudgetBlockShape(DictionaryAttr dict,
                                              ArrayRef<int64_t> budget) {
  Builder builder(dict.getContext());
  SmallVector<NamedAttribute, 8> fields;
  for (NamedAttribute field : dict) {
    if (field.getName() == sde::AttrNames::LayoutGraph::BudgetBlockShape)
      continue;
    fields.push_back(field);
  }
  fields.push_back(builder.getNamedAttr(
      sde::AttrNames::LayoutGraph::BudgetBlockShape,
      buildI64ArrayAttr(dict.getContext(), budget)));
  return builder.getDictionaryAttr(fields);
}

static bool upsertTypedArrayLayout(sde::SdeSuIterateOp su, int64_t arrayId,
                                   sde::SdeAccessMode mode,
                                   ArrayRef<int64_t> ownerDims,
                                   ArrayRef<int64_t> blockShape,
                                   ArrayRef<int64_t> logicalShape) {
  if (su.getBody().empty() || ownerDims.empty() || blockShape.empty() ||
      logicalShape.empty())
    return false;

  MLIRContext *ctx = su.getContext();
  Builder attrBuilder(ctx);
  for (sde::SdeArrayLayoutOp existing :
       su.getBody().front().getOps<sde::SdeArrayLayoutOp>()) {
    if (static_cast<int64_t>(existing.getArrayId()) != arrayId ||
        existing.getMode() != mode)
      continue;

    bool changed = false;
    auto ownerAttr = existing.getOwnerDimsAttr();
    if (!ownerAttr || ownerAttr.asArrayRef() != ownerDims) {
      existing->setAttr("ownerDims",
                        attrBuilder.getDenseI64ArrayAttr(ownerDims));
      changed = true;
    }
    if (existing.getBlockShapeAttr().asArrayRef() != blockShape) {
      existing->setAttr("blockShape",
                        attrBuilder.getDenseI64ArrayAttr(blockShape));
      changed = true;
    }
    if (existing.getLogicalShapeAttr().asArrayRef() != logicalShape) {
      existing->setAttr("logicalShape",
                        attrBuilder.getDenseI64ArrayAttr(logicalShape));
      changed = true;
    }
    return changed;
  }

  Block &entry = su.getBody().front();
  OpBuilder builder(&entry, entry.begin());
  sde::SdeArrayLayoutOp::create(
      builder, su.getLoc(), builder.getDenseI64ArrayAttr(ownerDims),
      ValueRange{}, builder.getDenseI64ArrayAttr(blockShape),
      builder.getDenseI64ArrayAttr(logicalShape),
      builder.getI64IntegerAttr(arrayId),
      sde::SdeAccessModeAttr::get(ctx, mode));
  return true;
}

struct SdeExposeLegalBlocksPass
    : public sde::impl::SdeExposeLegalBlocksBase<SdeExposeLegalBlocksPass> {
  void runOnOperation() override {
    sde::SdeAccessRelation &accessRelations =
        getAnalysis<sde::SdeAccessRelation>();
    sde::SdeCommVolumeCost &commCost = getAnalysis<sde::SdeCommVolumeCost>();
    sde::SdeMemoryLegality &legality =
        getAnalysis<sde::SdeMemoryLegality>();
    if (!legality.getMaxBlockBytes())
      return;
    sde::SdeDependence &dependence = getAnalysis<sde::SdeDependence>();

    ModuleOp module = getOperation();
    bool changed = false;
    bool failed = false;
    module.walk([&](sde::SdeSuIterateOp su) {
      if (dependence.hasLoopCarriedSelfDependence(su))
        return;

      ArrayAttr layout = su.getArrayLayoutAttr();
      if (!layout)
        return;

      SmallVector<Attribute, 8> rewritten;
      bool changedSu = false;
      for (Attribute rawEntry : layout) {
        auto dict = dyn_cast<DictionaryAttr>(rawEntry);
        std::optional<sde::LayoutGraphFact> fact =
            sde::parseArrayLayoutFact(dict);
        if (!dict || !fact ||
            fact->layoutKind != sde::ArrayLayoutKind::blockParallel ||
            fact->ownerDims.empty() || fact->blockShape.empty()) {
          rewritten.push_back(rawEntry);
          continue;
        }

        std::optional<sde::SdeAccessMode> mode = modeForRole(fact->role);
        if (!mode) {
          rewritten.push_back(rawEntry);
          continue;
        }

        Value root = sde::findArrayLayoutRoot(su, fact->id, *mode);
        std::optional<SmallVector<int64_t, 4>> shape = staticShape(root);
        if (!root || !shape || shape->size() != fact->blockShape.size() ||
            !isPositiveShape(*shape) || !isPositiveShape(fact->blockShape)) {
          su.emitOpError()
              << "sde-expose-legal-blocks: cannot ground static logical shape "
                 "for array "
              << fact->id;
          failed = true;
          rewritten.push_back(rawEntry);
          continue;
        }

        (void)accessRelations.getRelationsForRoot(root);
        (void)commCost.getCommittedLayoutBytes(su, fact->id, fact->role);

        SmallVector<int64_t, 4> budget = legality.capBlockShapeToBudget(
            root, *shape, fact->ownerDims, fact->blockShape);
        if (budget.empty() || !isPositiveShape(budget)) {
          su.emitOpError()
              << "sde-expose-legal-blocks: memory legality produced no valid "
                 "block budget for array "
              << fact->id;
          failed = true;
          rewritten.push_back(rawEntry);
          continue;
        }

        if (budget == fact->budgetBlockShape)
          rewritten.push_back(rawEntry);
        else {
          rewritten.push_back(replaceBudgetBlockShape(dict, budget));
          changedSu = true;
        }

        if (upsertTypedArrayLayout(su, fact->id, *mode, fact->ownerDims, budget,
                                   *shape))
          changedSu = true;
      }

      if (!changedSu)
        return;
      su.setArrayLayoutAttr(ArrayAttr::get(su.getContext(), rewritten));
      changed = true;
    });

    if (failed) {
      signalPassFailure();
      return;
    }
    if (!changed)
      markAllAnalysesPreserved();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::sde::createSdeExposeLegalBlocksPass() {
  return std::make_unique<SdeExposeLegalBlocksPass>();
}
