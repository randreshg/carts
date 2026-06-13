///==========================================================================///
/// File: VerifySdeMuAccessWindow.cpp
///
/// Verifies that per-CU MU access windows cover and describe rank-expanded
/// block-grid MUs.
///
///   R1 — coverage: every in-scope (MU, CU, mode) access reported by
///        `queryAccessWindows` has EXACTLY ONE `sde.mu_access_window` in its
///        enclosing `sde.cu_region`. Zero => the raiser did not run; more than
///        one => the idempotency guard is broken.
///
/// Block-grid bounds are enforced by the op's ODS verifier via type-derived
/// geometry (`deriveMuAccessWindowGeometry`).
/// Conservative MUs are skipped only when no committed SDE physical/window
/// facts require a structural dependency. Committed-but-unrepresentable facts
/// fail closed here, before ARTS can infer a coarse shape.
///==========================================================================///

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEMUACCESSWINDOW
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool hasAccessWindowFacts(sde::SdeSuIterateOp op) {
  bool found = false;
  op.getBody().walk([&](sde::SdeMuAccessWindowOp) {
    found = true;
    return WalkResult::interrupt();
  });
  return found;
}

static bool hasDirectCuMemrefAccess(sde::SdeMuAllocOp muAlloc) {
  for (Operation *user : muAlloc.getMemref().getUsers())
    if (isa<memref::LoadOp, memref::StoreOp>(user) &&
        user->getParentOfType<sde::SdeCuRegionOp>())
      return true;
  return false;
}

static bool isCuLocalMemrefRoot(Value root, sde::SdeSuIterateOp su) {
  Operation *def = root ? root.getDefiningOp() : nullptr;
  if (!def)
    return false;
  if (!isa<memref::AllocOp, memref::AllocaOp>(def))
    return false;
  auto cu = def->getParentOfType<sde::SdeCuRegionOp>();
  return cu && cu->getParentOfType<sde::SdeSuIterateOp>() == su;
}

static bool isPreexistingMemrefRoot(Value root, sde::SdeSuIterateOp su) {
  if (auto blockArg = dyn_cast<BlockArgument>(root)) {
    Operation *owner =
        blockArg.getOwner() ? blockArg.getOwner()->getParentOp() : nullptr;
    if (isa_and_nonnull<sde::SdeCuRegionOp, sde::SdeSuIterateOp>(owner))
      return false;
    if (owner && owner->isProperAncestor(su.getOperation()))
      return true;
    return owner && !owner->getParentOfType<sde::SdeCuRegionOp>() &&
           owner->getParentOfType<sde::SdeSuIterateOp>() != su;
  }

  Operation *def = root ? root.getDefiningOp() : nullptr;
  if (!def)
    return false;
  if (isa<memref::AllocOp, memref::AllocaOp, sde::SdeMuAllocOp,
          sde::SdeCuRegionOp>(def))
    return false;
  if (def->getParentOfType<sde::SdeCuRegionOp>() ||
      def->getParentOfType<sde::SdeSuIterateOp>() == su)
    return false;
  return true;
}

static bool hasDirectPreexistingMemrefAccess(sde::SdeSuIterateOp su) {
  bool found = false;
  su.getBody().walk([&](Operation *op) {
    Value memref;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      memref = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      memref = store.getMemref();
    else
      return WalkResult::advance();

    if (!op->getParentOfType<sde::SdeCuRegionOp>())
      return WalkResult::advance();
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (isCuLocalMemrefRoot(root, su))
      return WalkResult::advance();
    if (!isPreexistingMemrefRoot(root, su))
      return WalkResult::advance();
    found = true;
    return WalkResult::interrupt();
  });
  return found;
}

static bool requiresRaisedAccessWindows(sde::SdeMuAllocOp muAlloc) {
  auto muType = dyn_cast<MemRefType>(muAlloc.getMemref().getType());
  if (!muType || !muType.hasStaticShape())
    return false;

  sde::SdeSuIterateOp writer = sde::findCommittedBlockLayoutWitness(muAlloc);
  if (!sde::supportsRankExpandedAccessWindows(writer))
    return false;
  if (std::optional<sde::LayoutGraphFact> writeLayout =
          sde::findSingleCommittedWriterBlockLayout(writer)) {
    if (writeLayout->muBlockCount <= 1)
      return false;
  }
  if (sde::recognizeExpandedBlockGridMu(muAlloc))
    return true;
  sde::MuPhysicalLayout spec;
  return sde::isBlockGridRealizable(muAlloc, spec);
}

static bool hasConflictingCommittedWriterLayouts(sde::SdeMuAllocOp muAlloc) {
  std::optional<SmallVector<int64_t, 4>> selectedOwnerDims;
  std::optional<SmallVector<int64_t, 4>> selectedBlockShape;
  for (Operation *user : muAlloc.getMemref().getUsers()) {
    if (!isa<memref::StoreOp>(user))
      continue;
    sde::SdeSuIterateOp writer = user->getParentOfType<sde::SdeSuIterateOp>();
    while (writer && !sde::recoverCommittedPhysicalLayout(writer))
      writer = writer->getParentOfType<sde::SdeSuIterateOp>();
    if (!writer)
      continue;

    std::optional<sde::CommittedSuPhysicalLayout> layout =
        sde::recoverCommittedPhysicalLayout(writer);
    if (!layout)
      continue;

    if (!selectedOwnerDims) {
      selectedOwnerDims = layout->ownerDims;
      selectedBlockShape = layout->blockShape;
      continue;
    }
    if (*selectedOwnerDims != layout->ownerDims ||
        *selectedBlockShape != layout->blockShape)
      return true;
  }
  return false;
}

static bool containsAll(ArrayRef<int64_t> haystack, ArrayRef<int64_t> needles) {
  llvm::SmallDenseSet<int64_t, 4> values;
  for (int64_t value : haystack)
    values.insert(value);
  for (int64_t value : needles)
    if (!values.contains(value))
      return false;
  return true;
}

static bool i64ArrayContains(ArrayAttr attr, int64_t needle) {
  if (!attr)
    return false;
  return llvm::any_of(attr, [&](Attribute value) {
    auto integer = dyn_cast<IntegerAttr>(value);
    return integer && integer.getInt() == needle;
  });
}

static bool hasQueuedRedistributionForWindow(sde::SdeSuIterateOp su,
                                             sde::SdeMuAccessWindowOp win) {
  IntegerAttr arrayId = win.getArrayIdAttr();
  if (!arrayId)
    return false;
  for (const sde::LayoutGraphFact &fact :
       sde::parseArrayLayoutFacts(su.getArrayLayoutAttr()))
    if (fact.id == arrayId.getInt() &&
        fact.role == sde::LayoutGraphRole::read && fact.commVolumeBytes > 0)
      return true;
  Value mu = win.getMu();
  for (Operation *user : mu.getUsers()) {
    if (auto halo = dyn_cast<sde::SdeSuHaloOp>(user))
      if (halo.getArrayIdAttr() &&
          halo.getArrayIdAttr().getInt() == arrayId.getInt())
        return true;
    if (auto reduce = dyn_cast<sde::SdeSuReduceScatterOp>(user))
      if (reduce.getArrayIdAttr() &&
          reduce.getArrayIdAttr().getInt() == arrayId.getInt())
        return true;
  }
  return false;
}

static void verifyPartialReductionOwnersCovered(sde::SdeSuIterateOp op,
                                                bool &failed) {
  std::optional<SmallVector<int64_t, 4>> reductionOwnerDims =
      readI64ArrayAttr(op.getPartialReductionOwnerDimsAttr());
  if (!reductionOwnerDims || reductionOwnerDims->empty())
    return;

  std::optional<sde::CommittedSuPhysicalLayout> committedLayout =
      sde::recoverCommittedPhysicalLayout(op);
  if (!committedLayout || committedLayout->ownerDims.empty())
    return;

  if (containsAll(committedLayout->ownerDims, *reductionOwnerDims))
    return;

  // Inner pipeline-reduction axes may extend beyond the committed block-partition
  // owner dims; require only that every committed partition owner dim appears in
  // partialReductionOwnerDims.
  if (containsAll(*reductionOwnerDims, committedLayout->ownerDims))
    return;

  op.emitOpError() << "partialReductionOwnerDims are not covered by committed "
                      "physical owner dims; SDE must author a compatible "
                      "partial-reduction physical spec before sde-to-arts";
  failed = true;
}

static bool hasCommittedBoundaryFacts(sde::SdeSuIterateOp op) {
  if (sde::SdeCuRegionOp cu = sde::findSuComputeCuRegion(op);
      cu && cu.getGroupBlockCountAttr())
    return true;
  if (sde::recoverCommittedPhysicalLayout(op))
    return true;
  return op.getArrayLayoutAttr() || op.getAccessMinOffsetsAttr() ||
         op.getAccessMaxOffsetsAttr() || op.getOwnerDimsAttr() ||
         op.getSpatialDimsAttr() || op.getWriteFootprintAttr() ||
         op.getPartialReductionAttr() || op.getPartialReductionDimsAttr() ||
         op.getPartialReductionOwnerDimsAttr();
}

static void verifyCommittedFactsHaveWindowDeps(sde::SdeSuIterateOp op,
                                               bool &failed) {
  if (!hasCommittedBoundaryFacts(op))
    return;
  if (!hasDirectPreexistingMemrefAccess(op))
    return;

  if (sde::recoverCommittedPhysicalLayout(op)) {
    op.emitOpError()
        << "has committed physical partition facts but no access-window "
           "dependencies for a direct external memref access; SDE must raise "
           "access windows or transform the root before sde-to-arts";
  } else {
    op.emitOpError()
        << "has committed SDE layout/access facts but no access-window "
           "dependencies for a direct external memref access; SDE must raise "
           "access windows or transform the root before sde-to-arts";
  }
  failed = true;
}

static void verifyWindowOwnerRankRepresentable(sde::SdeMuAccessWindowOp win,
                                               bool &failed) {
  auto su = win->getParentOfType<sde::SdeSuIterateOp>();
  if (!su)
    return;
  IntegerAttr arrayId = win.getArrayIdAttr();
  if (!arrayId)
    return;
  ArrayAttr layout = su.getArrayLayoutAttr();
  if (!layout)
    return;

  auto modeMatches = [&](sde::LayoutGraphRole role) {
    if (win.getMode() == sde::SdeAccessMode::readwrite)
      return role == sde::LayoutGraphRole::write;
    if (win.getMode() == sde::SdeAccessMode::read)
      return role == sde::LayoutGraphRole::read;
    if (win.getMode() == sde::SdeAccessMode::write)
      return role == sde::LayoutGraphRole::write;
    return false;
  };

  std::optional<sde::LayoutGraphFact> matched;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.id == arrayId.getInt() && modeMatches(fact.role)) {
      matched = fact;
      break;
    }
  }

  size_t committedOwnerDimCount = 0;
  if (matched) {
    committedOwnerDimCount = matched->ownerDims.size();
  } else if (win.getMode() == sde::SdeAccessMode::write ||
             win.getMode() == sde::SdeAccessMode::readwrite) {
    if (std::optional<sde::CommittedSuPhysicalLayout> committed =
            sde::recoverCommittedPhysicalLayout(su))
      committedOwnerDimCount = committed->ownerDims.size();
    else
      return;
  } else {
    return;
  }

  std::optional<sde::MuAccessWindowGeometry> geom =
      sde::deriveMuAccessWindowGeometry(win);
  if (!geom)
    return;
  if (static_cast<size_t>(geom->ownerDimCount) == committedOwnerDimCount)
    return;
  if (hasQueuedRedistributionForWindow(su, win))
    return;

  win.emitOpError()
      << "access-window owner-dim count disagrees with committed arrayLayout; "
         "SDE must emit redistribution or a compatible physical spec before "
         "sde-to-arts";
  failed = true;
}

static bool hasCommittedBlockLayoutFacts(sde::SdeSuIterateOp op) {
  if (sde::recoverCommittedPhysicalLayout(op))
    return true;
  if (sde::SdeCuRegionOp cu = sde::findSuComputeCuRegion(op);
      cu && cu.getGroupBlockCountAttr())
    return true;
  for (const sde::LayoutGraphFact &fact :
       sde::parseArrayLayoutFacts(op.getArrayLayoutAttr())) {
    if (fact.role == sde::LayoutGraphRole::read && !fact.ownerDims.empty() &&
        !fact.blockShape.empty())
      return true;
  }
  return false;
}

struct VerifySdeMuAccessWindowPass
    : public sde::impl::VerifySdeMuAccessWindowBase<
          VerifySdeMuAccessWindowPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool failed = false;

    module.walk([&](sde::SdeSuIterateOp op) {
      verifyPartialReductionOwnersCovered(op, failed);
      verifyCommittedFactsHaveWindowDeps(op, failed);
      if (hasCommittedBlockLayoutFacts(op) || !hasAccessWindowFacts(op))
        return;
      op.emitOpError()
          << "has SDE MU access-window facts but no committed block layout; "
             "SDE must author a physical layout shape or fail "
             "before sde-to-arts";
      failed = true;
    });

    // R1 — coverage: each in-scope per-CU MU access has exactly one matching
    // window in that CU.
    module.walk([&](sde::SdeMuAllocOp mu) {
      llvm::SmallVector<sde::RaisedWindowSpec, 4> specs =
          sde::queryAccessWindows(mu);
      if (specs.empty()) {
        auto muType = dyn_cast<MemRefType>(mu.getMemref().getType());
        if (muType && muType.getRank() == 0 && hasDirectCuMemrefAccess(mu)) {
          mu.emitOpError()
              << "rank-0 DB-backed MU is accessed from a cu_region but "
                 "cannot be represented by sde.mu_access_window; SDE must "
                 "scalar-forward it or keep it CU-local before ARTS";
          failed = true;
          return;
        }
        if (hasConflictingCommittedWriterLayouts(mu)) {
          mu.emitOpError()
              << "has conflicting committed SDE physical writer layouts and "
                 "no representable sde.mu_access_window; SDE must "
                 "reconcile layout/movement before sde-to-arts";
          failed = true;
          return;
        }
        if (requiresRaisedAccessWindows(mu)) {
          mu.emitOpError()
              << "has committed SDE physical access facts but no "
                 "representable sde.mu_access_window; SDE must rank-expand "
                 "the MU and raise windows or fail before sde-to-arts";
          failed = true;
        }
        return; // out of scope -> no window required
      }
      for (const sde::RaisedWindowSpec &spec : specs) {
        unsigned countModeMatch = 0;
        sde::SdeCuRegionOp cu = spec.cu;
        for (Operation &op : cu.getBody().front())
          if (auto win = dyn_cast<sde::SdeMuAccessWindowOp>(op))
            if (win.getMu() == spec.mu && win.getMode() == spec.mode)
              ++countModeMatch;
        if (countModeMatch == 0) {
          mu.emitOpError() << "converted block-grid MU is in scope but has no "
                              "sde.mu_access_window for "
                           << sde::stringifySdeAccessMode(spec.mode)
                           << " access in its enclosing cu_region; run "
                              "raise-to-mu-access-window first";
          failed = true;
        } else if (countModeMatch > 1) {
          mu.emitOpError()
              << "duplicate sde.mu_access_window for this MU/mode in its "
                 "cu_region (raise-to-mu-access-window idempotency broken)";
          failed = true;
        }
      }
    });

    auto windowCarriesExpectedArrayId =
        [&](sde::SdeMuAccessWindowOp win,
            const sde::RaisedWindowSpec &spec) -> bool {
      if (!spec.arrayId)
        return !win.getArrayIdAttr();
      IntegerAttr windowId = win.getArrayIdAttr();
      return windowId && windowId.getInt() == *spec.arrayId;
    };

    module.walk([&](sde::SdeCuRegionOp cu) {
      for (Operation &op : cu.getBody().front()) {
        auto win = dyn_cast<sde::SdeMuAccessWindowOp>(op);
        if (!win)
          continue;
        verifyWindowOwnerRankRepresentable(win, failed);
        if (auto muAlloc = win.getMu().getDefiningOp<sde::SdeMuAllocOp>()) {
          llvm::SmallVector<sde::RaisedWindowSpec, 4> expected =
              sde::queryAccessWindows(muAlloc);
          bool hasExpected = false;
          for (const sde::RaisedWindowSpec &spec : expected)
            if (spec.cu == cu && spec.mu == win.getMu() &&
                spec.mode == win.getMode()) {
              hasExpected = true;
              if (!windowCarriesExpectedArrayId(win, spec)) {
                win.emitOpError()
                    << "arrayId does not match the committed SDE MU root "
                       "identity for this access window";
                failed = true;
              }
              break;
            }
          if (!hasExpected) {
            win.emitOpError()
                << "does not match any in-scope per-CU access-window spec";
            failed = true;
          }
        }
        unsigned duplicates = 0;
        for (Operation &otherOp : cu.getBody().front()) {
          auto other = dyn_cast<sde::SdeMuAccessWindowOp>(otherOp);
          if (other && other.getMu() == win.getMu() &&
              other.getMode() == win.getMode())
            ++duplicates;
        }
        if (duplicates > 1) {
          win.emitOpError()
              << "duplicate sde.mu_access_window for this MU/mode in one "
                 "cu_region";
          failed = true;
        }
      }
    });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdeMuAccessWindowPass() {
  return std::make_unique<VerifySdeMuAccessWindowPass>();
}
} // namespace mlir::carts::sde
