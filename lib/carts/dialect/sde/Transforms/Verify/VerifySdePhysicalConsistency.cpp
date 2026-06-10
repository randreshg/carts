///==========================================================================///
/// File: VerifySdePhysicalConsistency.cpp
///
/// The pre-window physical-plan consistency gate. It runs before the
/// rank-expand and access-window transforms so it fails closed on the
/// stale-grain shape the SDE boundary forbids -- one `sde.su_iterate` carrying
/// several incompatible truths at once (the jacobi-for class) -- before
/// SdeRankExpandMu consumes the committed physical plan, instead of letting a
/// stale plan disagree with its arrayLayout or schedule and reach CODIR.
///
/// For every `sde.su_iterate` carrying a committed physical plan
/// (`physicalOwnerDims` + `physicalBlockShape`) it checks:
///
///   (a) plan well-formedness: owner dims index block-shape dims; block extents
///       are positive;
///   (b) schedule consistency: no more owner dims than realized loop dims;
///   (c) budget agreement (jacobi-for catcher): the physical block is not
///       coarser than any written array's node-agnostic budget grain on that
///       array's own owner dims. The physical owner-tile dims (a CU
///       compute-grain decision) need not equal an array's storage owner dims,
///       so only the budget-coarsening relation is enforced.
///   (d) flat-path schedule mirror (R2 on the flat MU): the physical block is
///       not coarser than the realized SU iteration extent on an owner dim.
///       `verify-sde-mu-layout` R2 enforces the equivalent mirror against
///       independent iteration extents once the MU is rank-expanded; (d) closes
///       the same gap on the flat path so the plan is a verifier-checked mirror
///       of the realized schedule, never an unchecked promise. DB/MU grain
///       stays separate from CU grain: this checks the SU's OWN schedule, not
///       arrays.
///
/// Residual global-index access against an already rank-expanded MU (the body
/// non-locality the vision warns about) is the companion `verify-sde-mu-layout`
/// gate's job; flat owner-tile bodies that CODIR still localizes from committed
/// SU-local bounds are contract-correct here, so this gate does not re-derive
/// body locality.
///
/// It reads committed facts and current IR through generated ODS accessors and
/// the shared `parseArrayLayoutFacts` view; it stamps nothing and recomputes no
/// owner dims or block shape.
///==========================================================================///

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEPHYSICALCONSISTENCY
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

// (a) + (b): the physical plan is well formed and fits the realized schedule.
static void verifyPlanWellFormed(sde::SdeSuIterateOp op,
                                 ArrayRef<int64_t> ownerDims,
                                 ArrayRef<int64_t> block, bool &hasFailure) {
  for (int64_t ownerDim : ownerDims) {
    if (ownerDim >= 0 && static_cast<size_t>(ownerDim) < block.size())
      continue;
    op.emitOpError() << "physicalOwnerDims must index physicalBlockShape "
                        "dimensions";
    hasFailure = true;
    break;
  }
  for (int64_t extent : block) {
    if (extent > 0)
      continue;
    op.emitOpError() << "physicalBlockShape entries must be positive";
    hasFailure = true;
    break;
  }
  if (ownerDims.size() > op.getSteps().size()) {
    op.emitOpError() << "physical plan names more owner dimensions than "
                        "realized SDE loop dimensions";
    hasFailure = true;
  }
}

// (c): the committed physical block must not be COARSER than a written array's
// node-agnostic budget grain on that array's own owner dimensions. The budget
// grain is node-agnostic; the cost-model physical tile may refine it (finer or
// equal) but may never coarsen past it. The jacobi-for class stamps a row strip
// (`physicalBlockShape` `[1280, 10240]`) over a 2-D `[512, 512]` owner-tile
// budget; both owner-dim extents exceed the budget and fail here.
//
// The physical owner-tile dims (a CU compute-grain decision) are NOT required
// to equal an array's storage owner dims: a 2-D compute tile legitimately
// writes a 1-D owner-distributed array (matmul output, row/col vectors). DB/MU
// grain is kept separate from CU grain, so only the budget-coarsening relation
// on each array's OWN owner dims is enforced.
static void verifyBudgetNotCoarsened(sde::SdeSuIterateOp op,
                                     ArrayRef<int64_t> block,
                                     bool &hasFailure) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write ||
        fact.layoutKind != sde::ArrayLayoutKind::blockParallel ||
        fact.budgetBlockShape.empty())
      continue;
    for (int64_t ownerDim : fact.ownerDims) {
      if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= block.size() ||
          static_cast<size_t>(ownerDim) >= fact.budgetBlockShape.size())
        continue;
      int64_t budget = fact.budgetBlockShape[ownerDim];
      if (budget > 0 && block[ownerDim] > budget) {
        op.emitOpError()
            << "physicalBlockShape is coarser than the committed node-agnostic "
               "budget grain on a written array's owner dimension; a stale "
               "physical plan must not coarsen past the budget block";
        hasFailure = true;
        return;
      }
    }
  }
}

// (R2, flat path): on the flat (pre-rank-expand) MU path the committed physical
// plan is the only carrier of the realized grain, so it must not name a
// physical block COARSER than the realized SU iteration extent on an owner dim.
// The SU step on an owner dim is the realized compute-tile stride; a physical
// block finer-or-equal to that stride is structurally realizable, a physical
// block coarser than the iteration extent (the jacobi-for row strip whose body
// still iterates the coarse logical tile) is the stale shape the boundary
// forbids. `verify-sde-mu-layout` R2 enforces the equivalent mirror against
// independent iteration extents once the MU is rank-expanded; this closes the
// gap on the flat path so the plan is a verifier-checked mirror, never an
// unchecked promise that SdeRankExpandMu silently bails on. DB/MU grain stays
// separate from CU grain: this checks the physical block against the SU's OWN
// realized schedule, not against any array's storage block.
static void verifyPhysicalFitsIterationExtent(sde::SdeSuIterateOp op,
                                              ArrayRef<int64_t> ownerDims,
                                              ArrayRef<int64_t> block,
                                              bool &hasFailure) {
  OperandRange lowers = op.getLowerBounds();
  OperandRange uppers = op.getUpperBounds();
  for (auto [slot, ownerDim] : llvm::enumerate(ownerDims)) {
    if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= block.size() ||
        static_cast<size_t>(ownerDim) >= lowers.size() ||
        static_cast<size_t>(ownerDim) >= uppers.size())
      continue;
    std::optional<int64_t> lo =
        ValueAnalysis::tryFoldConstantIndex(lowers[ownerDim]);
    std::optional<int64_t> hi =
        ValueAnalysis::tryFoldConstantIndex(uppers[ownerDim]);
    if (!lo || !hi || *hi <= *lo)
      continue;
    int64_t extent = *hi - *lo;
    if (block[ownerDim] > extent) {
      op.emitOpError()
          << "physicalBlockShape is coarser than the realized SU iteration "
             "extent on an owner dimension; the committed physical block must "
             "be a refinement of the realized schedule, not a stale coarse "
             "tile";
      hasFailure = true;
      return;
    }
  }
}

static void verifyPhysicalConsistency(sde::SdeSuIterateOp op,
                                      bool &hasFailure) {
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  std::optional<SmallVector<int64_t, 4>> block =
      readI64ArrayAttr(op.getPhysicalBlockShapeAttr());

  // Both halves of the plan must be present together, or neither.
  if (!op.getPhysicalOwnerDimsAttr() && !op.getPhysicalBlockShapeAttr())
    return;
  if (!ownerDims || !block || ownerDims->empty() || block->empty()) {
    op.emitOpError()
        << "physical plan requires non-empty physicalOwnerDims and "
           "physicalBlockShape together";
    hasFailure = true;
    return;
  }

  verifyPlanWellFormed(op, *ownerDims, *block, hasFailure);
  verifyBudgetNotCoarsened(op, *block, hasFailure);
  verifyPhysicalFitsIterationExtent(op, *ownerDims, *block, hasFailure);
}

struct VerifySdePhysicalConsistencyPass
    : public sde::impl::VerifySdePhysicalConsistencyBase<
          VerifySdePhysicalConsistencyPass> {
  void runOnOperation() override {
    bool hasFailure = false;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      verifyPhysicalConsistency(op, hasFailure);
    });
    if (hasFailure)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdePhysicalConsistencyPass() {
  return std::make_unique<VerifySdePhysicalConsistencyPass>();
}
} // namespace mlir::carts::sde
