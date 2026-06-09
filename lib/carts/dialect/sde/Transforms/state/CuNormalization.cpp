///==========================================================================///
/// File: CuNormalization.cpp
///
/// SDE CU normalization for all source executable work.
///
/// Wraps every run of source-executable work that is NOT already inside a CU
/// into a conservative `sde.cu_region <single>`, so the SDE boundary
/// invariant "all source executable work lives in a CU" (enforced by
/// `verify-sde`) holds for non-OpenMP host code too: init loops, scalar
/// check/verification code, sequential reductions, and scalar-effect work that
/// `ConvertOpenMPToSde` / `Parallelize` did not already place in a CU. The
/// `sde.su_iterate` bodies are normalized strictly: tile-local loops and scalar
/// plumbing are moved into the CU they schedule, leaving the SU to contain CUs,
/// barriers, and its terminator. `sde.su_distribute` direct-child legality is
/// verifier-owned because that wrapper can only contain nested SUs,
/// `sde.redist`, or barriers.
///
/// This is a STRUCTURAL transformation only. It:
///   * reads current IR and moves existing ops into a CU container — it stamps
///     no metadata and branches on no downstream contract;
///   * requires no CU isolation, no MU token, and no slice, and it introduces
///     none;
///   * introduces no CODIR concept and no codelet isolation (`cu_region` is not
///     `IsolatedFromAbove`; the wrapped body keeps referencing enclosing SSA
///     values directly);
///   * makes no distribution, movement, DB-grain, or access-window decision;
///   * leaves existing valid CUs unchanged.
///
/// `<single>` is the conservative CU kind: one logical execution, no
/// parallelism claimed. Later parallel-legality and sync passes refine it; this
/// pass only establishes containment.
///
/// The unit of wrapping is a maximal contiguous run of block-level ops bounded
/// by SDE structural ops (CUs, SUs, MU declarations, barriers) and the block
/// terminator, trimmed to its source-compute span. A run is wrapped only when
/// it contains real source compute; pure schedule/index plumbing that the
/// verifier permits outside a CU is left in place. Values produced by a wrapped
/// span and used afterward are threaded through `sde.cu_region` results.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeCuStructure.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDECUNORMALIZATION
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include <iterator>
#include <memory>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::sde;

namespace {

/// True when `fn` contains any SDE op. This matches the verifier's scope: the
/// "source work outside a CU" rule only applies inside SDE-bearing functions,
/// so plain host helpers with no SDE structure are left entirely alone.
static bool funcHasSdeOp(func::FuncOp fn) {
  bool found = false;
  fn.walk([&](Operation *op) {
    if (isSdeDialectOp(op)) {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

/// The block-level (direct-child-of-`block`) ancestor of `user`, or null if
/// `user` is not nested under any op that is a direct child of `block`.
static Operation *blockLevelAncestor(Operation *user, Block *block) {
  Operation *cursor = user;
  while (cursor && cursor->getBlock() != block)
    cursor = cursor->getParentOp();
  return cursor;
}

static bool isNestedUnder(Operation *op, Operation *container) {
  for (Operation *cursor = op; cursor; cursor = cursor->getParentOp())
    if (cursor == container)
      return true;
  return false;
}

/// Collect direct-child results that are used outside `span`.
static void collectEscapingValues(ArrayRef<Operation *> span, Block *block,
                                  SmallVectorImpl<Value> &escaping) {
  llvm::DenseSet<Operation *> spanSet(span.begin(), span.end());
  for (Operation *op : span)
    for (Value result : op->getResults())
      for (Operation *user : result.getUsers()) {
        Operation *ancestor = blockLevelAncestor(user, block);
        if (!ancestor || !spanSet.contains(ancestor)) {
          escaping.push_back(result);
          break;
        }
      }
}

/// Move the contiguous op span [first, last] into a fresh
/// `sde.cu_region <single>` inserted at `first`.
static void wrapSpanInCuRegion(Operation *first, Operation *last,
                               ArrayRef<Value> escaping) {
  Block *block = first->getBlock();
  OpBuilder builder(first);
  SmallVector<Type> resultTypes;
  resultTypes.reserve(escaping.size());
  for (Value value : escaping)
    resultTypes.push_back(value.getType());

  auto cuRegion = SdeCuRegionOp::create(
      builder, first->getLoc(), resultTypes,
      SdeCuKindAttr::get(builder.getContext(), SdeCuKind::single),
      /*nowait=*/nullptr, /*iterArgs=*/ValueRange{});
  Block &body = ensureBlock(cuRegion.getBody());
  // Block::getOperations().splice uses the standard half-open [first, last)
  // contract, so advance past `last` to include it.
  body.getOperations().splice(body.end(), block->getOperations(),
                              first->getIterator(),
                              std::next(last->getIterator()));
  OpBuilder yieldBuilder = OpBuilder::atBlockEnd(&body);
  SdeYieldOp::create(yieldBuilder, first->getLoc(), escaping);

  for (auto pair : llvm::zip(escaping, cuRegion->getResults())) {
    Value oldValue = std::get<0>(pair);
    Value newValue = std::get<1>(pair);
    oldValue.replaceUsesWithIf(newValue, [&](OpOperand &use) {
      return !isNestedUnder(use.getOwner(), cuRegion);
    });
  }
}

/// Wrap every source-compute span in `block` that is not already inside a CU.
/// Returns false (after emitting a diagnostic) when a span cannot be safely
/// wrapped; the block's other spans are still normalized for maximal evidence.
static bool normalizeBlock(Block *block, bool strictSuBody = false) {
  // Snapshot the direct children (excluding the terminator) up front: wrapping
  // splices ops out of `block`, so we must not be walking a live block
  // iterator.
  SmallVector<Operation *> ops;
  for (Operation &op : block->without_terminator())
    ops.push_back(&op);

  bool ok = true;
  size_t i = 0, n = ops.size();
  while (i < n) {
    // SDE structural ops (existing CUs/SUs, MU declarations, barriers) are
    // boundaries: they stay at this level and split the movable runs.
    if (isSdeDialectOp(ops[i])) {
      ++i;
      continue;
    }
    // Maximal movable run, bounded by the next SDE structural op.
    size_t runEnd = i;
    while (runEnd < n && !isSdeDialectOp(ops[runEnd]))
      ++runEnd;
    size_t lo = i, hi = runEnd;
    if (!strictSuBody) {
      // Trim to the source-compute span; edge schedule/index plumbing the
      // verifier permits outside a CU stays at block scope rather than being
      // pulled in.
      while (lo < hi && !isSourceComputeOp(ops[lo]))
        ++lo;
      while (hi > lo && !isSourceComputeOp(ops[hi - 1]))
        --hi;
    }
    if (lo < hi) {
      ArrayRef<Operation *> span(ops.data() + lo, hi - lo);
      SmallVector<Value> escaping;
      collectEscapingValues(span, block, escaping);
      wrapSpanInCuRegion(ops[lo], ops[hi - 1], escaping);
    }
    i = runEnd;
  }
  return ok;
}

struct SdeCuNormalizationPass
    : public sde::impl::SdeCuNormalizationBase<SdeCuNormalizationPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    // Collect target blocks before mutating. Two kinds, disjoint:
    //   * the body block of every su_iterate (raw compute directly in an
    //     iterate SU body is illegal — the SU must be scheduling-only), and
    //   * every block of every SDE-bearing func (source work outside any CU is
    //     illegal there).
    // The cu_regions this pass inserts never move an SU/CU boundary op, so
    // these collected block pointers stay valid across all wrapping.
    SmallVector<Block *> targets;
    llvm::DenseSet<Block *> suTargets;
    module.walk([&](sde::SdeSuIterateOp op) {
      Region &body = op.getBody();
      if (!body.empty()) {
        targets.push_back(&body.front());
        suTargets.insert(&body.front());
      }
    });
    module.walk([&](func::FuncOp fn) {
      if (!funcHasSdeOp(fn))
        return;
      for (Block &block : fn.getBody())
        targets.push_back(&block);
    });

    bool ok = true;
    for (Block *block : targets)
      ok &= normalizeBlock(block, suTargets.contains(block));

    if (!ok)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createSdeCuNormalizationPass() {
  return std::make_unique<SdeCuNormalizationPass>();
}
} // namespace mlir::carts::sde
