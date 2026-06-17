///==========================================================================///
/// File: RankExpandMu.cpp
///
/// SDE structural owner-dim/grain carrier.
///
/// Replaces the attribute-only block/owner grain with readable IR structure:
/// for every `sde.mu_alloc` governed by a committed elementwise/stencil BLOCK
/// layout with any number of owner dims, this pass rank-expands the result
/// memref so
/// the block grid is part of the type, and rewrites every CU `memref.load`/
/// `memref.store` into the physical `[block, intra-block, ...]` coordinate
/// system via the `MuLayoutRewriter`/`MuAccessIndexer` library.
///
/// This is a real transformation:
///   * a converted MU carries its grain structurally (no owner-dim attr on the
///     mu_alloc; owner dims are `recover(structure)`); multi-owner owner-tile
///     layouts expand to a `[grid..., tile...]` form, with grid dims in
///     canonical ascending owner order,
///   * accumulator-reduction / in-place / dynamic cases stay in flat form or
///     fail closed; this pass does not add compatibility attrs.
///==========================================================================///

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDERANKEXPANDMU
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include <memory>

using namespace mlir;

namespace {

// Shared helpers keep the block-grid realize gate and index localization
// identical across rank expansion, coarse avoidance, and verification.

static bool writerHasDifferentWriteGrain(carts::sde::SdeSuIterateOp writer,
                                         std::optional<int64_t> muArrayId,
                                         ArrayRef<int64_t> ownerDims,
                                         ArrayRef<int64_t> blockShape) {
  if (!writer || !muArrayId || ownerDims.empty() || blockShape.empty())
    return false;
  ArrayAttr layout = writer.getArrayLayoutAttr();
  if (!layout)
    return false;
  for (const carts::sde::LayoutGraphFact &fact :
       carts::sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != carts::sde::LayoutGraphRole::write ||
        fact.id == *muArrayId || fact.blockShape.empty())
      continue;
    if (ArrayRef<int64_t>(fact.ownerDims) != ownerDims ||
        ArrayRef<int64_t>(fact.blockShape) != blockShape)
      return true;
  }
  return false;
}

static Value findWitnessRoot(carts::sde::SdeSuIterateOp witness,
                             int64_t arrayId) {
  if (!witness)
    return {};
  if (Value root = carts::sde::findArrayLayoutRoot(
          witness, arrayId, carts::sde::SdeAccessMode::write))
    return root;
  return carts::sde::findArrayLayoutRoot(witness, arrayId,
                                         carts::sde::SdeAccessMode::read);
}

static void propagateWriterLayoutToSameExpandedMu(
    ModuleOp module, Value expandedRoot, int64_t arrayId,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> blockShape) {
  if (!expandedRoot || ownerDims.empty() || blockShape.empty())
    return;
  module.walk([&](carts::sde::SdeArrayLayoutRootOp root) {
    if (root.getMode() != carts::sde::SdeAccessMode::write ||
        static_cast<int64_t>(root.getArrayId()) != arrayId ||
        !carts::ValueAnalysis::sameMemrefRoot(root.getRoot(), expandedRoot))
      return;
    carts::sde::SdeSuIterateOp writer =
        root->getParentOfType<carts::sde::SdeSuIterateOp>();
    if (!writer)
      return;
    carts::sde::rewriteWriterArrayLayoutToPhysicalShape(writer, ownerDims,
                                                        blockShape, arrayId);
  });
}

static void reconcileReadArrayLayoutWithExpandedMu(
    carts::sde::SdeSuIterateOp op, int64_t arrayId, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> physicalBlockShape, ArrayRef<int64_t> logicalShape) {
  if (!op || ownerDims.empty() || physicalBlockShape.empty() ||
      logicalShape.empty())
    return;
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return;

  MLIRContext *ctx = op.getContext();
  Builder builder(ctx);
  StringAttr ownerDimsName =
      builder.getStringAttr(carts::sde::AttrNames::LayoutGraph::OwnerDims);
  StringAttr blockShapeName =
      builder.getStringAttr(carts::sde::AttrNames::LayoutGraph::BlockShape);
  StringAttr budgetBlockShapeName = builder.getStringAttr(
      carts::sde::AttrNames::LayoutGraph::BudgetBlockShape);
  StringAttr muBlockCountName =
      builder.getStringAttr(carts::sde::AttrNames::LayoutGraph::MuBlockCount);

  int64_t blockCount = carts::sde::inferCuCountFromMuPartition(
      logicalShape, ownerDims, physicalBlockShape);
  bool changed = false;
  SmallVector<Attribute, 4> rewritten;
  rewritten.reserve(layout.size());
  for (Attribute attr : layout) {
    auto dict = dyn_cast<DictionaryAttr>(attr);
    std::optional<carts::sde::LayoutGraphFact> fact =
        dict ? carts::sde::parseArrayLayoutFact(dict) : std::nullopt;
    bool ownerDimsMatch = fact && fact->ownerDims == ownerDims;
    bool contractionReaderSubset =
        fact &&
        fact->layoutKind == carts::sde::ArrayLayoutKind::blockContraction &&
        llvm::all_of(fact->ownerDims, [&](int64_t dim) {
          return llvm::is_contained(ownerDims, dim);
        });
    if (!dict || !fact || fact->id != arrayId ||
        fact->role != carts::sde::LayoutGraphRole::read ||
        (!ownerDimsMatch && !contractionReaderSubset) ||
        (fact->layoutKind != carts::sde::ArrayLayoutKind::blockParallel &&
         fact->layoutKind != carts::sde::ArrayLayoutKind::blockContraction)) {
      rewritten.push_back(attr);
      continue;
    }

    bool sameBlock = ArrayRef<int64_t>(fact->blockShape) == physicalBlockShape;
    bool noStaleBudget = fact->budgetBlockShape.empty();
    bool sameCount = blockCount <= 0 || fact->muBlockCount == blockCount;
    if (sameBlock && noStaleBudget && sameCount) {
      rewritten.push_back(attr);
      continue;
    }

    SmallVector<NamedAttribute, 8> fields;
    fields.reserve(dict.size());
    for (NamedAttribute named : dict) {
      StringAttr name = named.getName();
      if (name == ownerDimsName || name == blockShapeName ||
          name == budgetBlockShapeName || name == muBlockCountName)
        continue;
      fields.push_back(named);
    }
    fields.push_back(builder.getNamedAttr(
        ownerDimsName, carts::buildI64ArrayAttr(ctx, ownerDims)));
    fields.push_back(builder.getNamedAttr(
        blockShapeName, carts::buildI64ArrayAttr(ctx, physicalBlockShape)));
    if (blockCount > 0)
      fields.push_back(builder.getNamedAttr(
          muBlockCountName, builder.getI64IntegerAttr(blockCount)));
    rewritten.push_back(builder.getDictionaryAttr(fields));
    changed = true;
  }

  if (changed)
    op.setArrayLayoutAttr(ArrayAttr::get(ctx, rewritten));
}

static void propagateReadLayoutToSameExpandedMu(
    ModuleOp module, Value expandedRoot, int64_t arrayId,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> blockShape,
    ArrayRef<int64_t> logicalShape) {
  if (!expandedRoot || ownerDims.empty() || blockShape.empty() ||
      logicalShape.empty())
    return;
  module.walk([&](carts::sde::SdeArrayLayoutRootOp root) {
    if (root.getMode() != carts::sde::SdeAccessMode::read ||
        static_cast<int64_t>(root.getArrayId()) != arrayId ||
        !carts::ValueAnalysis::sameMemrefRoot(root.getRoot(), expandedRoot))
      return;
    carts::sde::SdeSuIterateOp reader =
        root->getParentOfType<carts::sde::SdeSuIterateOp>();
    reconcileReadArrayLayoutWithExpandedMu(reader, arrayId, ownerDims,
                                           blockShape, logicalShape);
  });
}

struct SdeRankExpandMuPass
    : public carts::sde::impl::SdeRankExpandMuBase<SdeRankExpandMuPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    llvm::SmallVector<carts::sde::SdeMuAllocOp> worklist;
    module.walk([&](carts::sde::SdeMuAllocOp mu) { worklist.push_back(mu); });

    bool failed = false;
    for (carts::sde::SdeMuAllocOp mu : worklist) {
      auto logicalType = dyn_cast<MemRefType>(mu.getMemref().getType());
      if (!logicalType || !logicalType.hasStaticShape())
        continue; // dynamic / non-memref -> conservative

      std::optional<carts::sde::CommittedMuBlockLayout> committed =
          carts::sde::findCommittedMuBlockLayout(mu);
      if (!committed ||
          !carts::sde::supportsRankExpandedAccessWindows(committed->writer))
        continue; // out of scope -> leave flat, add NO attrs

      // Resolve the array id before rewriter.apply replaces the MU root (which
      // invalidates the layout-root users the lookup walks).
      std::optional<int64_t> muArrayId =
          carts::sde::getMuArrayIdFromLayoutRoot(mu);

      std::optional<carts::sde::SdeStructuredClassification> classification =
          carts::sde::queryStructuredClassification(committed->writer);
      if (!classification)
        classification = committed->writer.getStructuredClassification();

      // Commit classification durably before the rewrite makes the body div/rem
      // (un-re-derivable). 1n-inert: only runs for committed block layouts.
      if (classification &&
          !committed->writer.getStructuredClassificationAttr())
        committed->writer.setStructuredClassificationAttr(
            carts::sde::SdeStructuredClassificationAttr::get(
                committed->writer.getContext(), *classification));

      std::unique_ptr<carts::sde::MuAccessIndexer> indexer =
          carts::sde::makeMuAccessIndexerForCommittedLayout(committed->writer,
                                                            committed->layout);
      carts::sde::MuLayoutRewriter rewriter(committed->layout, *indexer);
      if (mlir::failed(rewriter.apply(mu))) {
        mu.emitOpError()
            << "committed block-grid layout cannot be realized as a "
               "rank-expanded MU (unsupported use of the MU root); refusing to "
               "leave a partial owner-dim promise";
        failed = true;
        continue;
      }
      SmallVector<int64_t, 4> ownerDims;
      ownerDims.reserve(committed->layout.ownerDims.size());
      for (unsigned dim : committed->layout.ownerDims)
        ownerDims.push_back(static_cast<int64_t>(dim));
      SmallVector<int64_t, 4> blockShape = committed->layout.logicalShape;
      for (auto [slot, dim] : llvm::enumerate(committed->layout.ownerDims))
        blockShape[dim] = committed->layout.blockExtents[slot];
      bool restrictToOwnArray =
          muArrayId && writerHasDifferentWriteGrain(
                           committed->writer, muArrayId, ownerDims, blockShape);
      carts::sde::rewriteWriterArrayLayoutToPhysicalShape(
          committed->writer, ownerDims, blockShape,
          restrictToOwnArray ? muArrayId : std::nullopt);
      if (muArrayId) {
        Value expandedRoot = findWitnessRoot(committed->writer, *muArrayId);
        propagateWriterLayoutToSameExpandedMu(module, expandedRoot, *muArrayId,
                                              ownerDims, blockShape);
        propagateReadLayoutToSameExpandedMu(module, expandedRoot, *muArrayId,
                                            ownerDims, blockShape,
                                            committed->layout.logicalShape);
      }
    }

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createSdeRankExpandMuPass() {
  return std::make_unique<SdeRankExpandMuPass>();
}
} // namespace mlir::carts::sde
