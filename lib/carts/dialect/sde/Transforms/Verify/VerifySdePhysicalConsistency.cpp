///==========================================================================///
/// File: VerifySdePhysicalConsistency.cpp
///
/// The aggregate pre-CODIR physical-plan consistency gate. It fails closed on
/// the stale-grain shape the SDE boundary forbids -- one `sde.su_iterate`
/// carrying several incompatible truths at once (the jacobi-for class) -- so a
/// committed physical plan that disagrees with its arrayLayout or schedule never
/// reaches CODIR.
///
/// For every `sde.su_iterate` carrying a committed physical plan
/// (`physicalOwnerDims` + `physicalBlockShape`) it checks:
///
///   (a) plan well-formedness: owner dims index block-shape dims; block extents
///       are positive;
///   (b) schedule consistency: no more owner dims than realized loop dims;
///   (c) budget agreement (jacobi-for catcher): the physical block is not
///       coarser than any written array's node-agnostic budget grain on that
///       array's own owner dims. The physical owner-tile dims (a CU compute-grain
///       decision) need not equal an array's storage owner dims, so only the
///       budget-coarsening relation is enforced.
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
// The physical owner-tile dims (a CU compute-grain decision) are NOT required to
// equal an array's storage owner dims: a 2-D compute tile legitimately writes a
// 1-D owner-distributed array (matmul output, row/col vectors). DB/MU grain is
// kept separate from CU grain, so only the budget-coarsening relation on each
// array's OWN owner dims is enforced.
static void verifyBudgetNotCoarsened(sde::SdeSuIterateOp op,
                                     ArrayRef<int64_t> block, bool &hasFailure) {
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

static void verifyPhysicalConsistency(sde::SdeSuIterateOp op, bool &hasFailure) {
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  std::optional<SmallVector<int64_t, 4>> block =
      readI64ArrayAttr(op.getPhysicalBlockShapeAttr());

  // Both halves of the plan must be present together, or neither.
  if (!op.getPhysicalOwnerDimsAttr() && !op.getPhysicalBlockShapeAttr())
    return;
  if (!ownerDims || !block || ownerDims->empty() || block->empty()) {
    op.emitOpError() << "physical plan requires non-empty physicalOwnerDims and "
                        "physicalBlockShape together";
    hasFailure = true;
    return;
  }

  verifyPlanWellFormed(op, *ownerDims, *block, hasFailure);
  verifyBudgetNotCoarsened(op, *block, hasFailure);
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
