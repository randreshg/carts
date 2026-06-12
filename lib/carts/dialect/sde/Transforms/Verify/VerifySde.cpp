///==========================================================================///
/// File: VerifySde.cpp
///
/// Structural verifier for the SDE boundary. SDE owns State, Dependency, and
/// Effect over MU/CU/SU. By the boundary the IR must already be the target
/// shape, and this pass fails closed when it is not. It enforces three rules,
/// reading only current IR and local op structure (no metadata, no downstream
/// promise):
///
///   No consumed dependency graph. SDE carries no generic token/dataflow
///     dependency graph; ordering edges are derived in ARTS after codelet
///     isolation.
///   SU body shape is verified by the owning SDE op verifiers:
///     `sde.su_iterate` directly contains only direct-boundary CUs,
///     root-provenance facts, `sde.su_barrier`, and its terminator;
///     `sde.su_distribute` directly contains nested SUs, `sde.redist`, or
///     barriers. Executable work, tile-local loops, and scalar plumbing for
///     that work live in CUs.
///   All source executable work lives in a CU. Within SDE-bearing functions,
///     source work may not sit outside every CU. scf is legal under SDE only
///     inside a CU.
///   SDE has no hidden textual order between sibling CU work items. A
///     provable MU access conflict must be ordered by SU/source ordering facts,
///     not by assuming the CU op itself is a scheduling scope.
///
/// Deliberately NOT checked: CU `IsolatedFromAbove` (a ARTS concern), the
/// presence of an `sde.mu_token` / slice op / any specific access-window
/// carrier (windows are raised later by RaiseToMuAccessWindow).
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeCuStructure.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::sde;

namespace {

// The source-compute predicates this verifier branches on are shared with
// `sde-cu-normalization`. SU direct-child legality is stricter: even pure
// scalar/index plumbing must be inside a CU when it belongs to scheduled work.

static bool hasCuAncestor(Operation *op) {
  for (Operation *parent = op->getParentOp(); parent;
       parent = parent->getParentOp())
    if (isCuOp(parent))
      return true;
  return false;
}

/// Per-CU set of MU storage roots read / written, derived locally from
/// `sde.mu_token` operands of an `sde.cu_work`. CUs without token access info
/// contribute nothing, so the conflict check never requires a token to exist.
struct CuAccess {
  llvm::DenseSet<Value> reads;
  llvm::DenseSet<Value> writes;
};

static CuAccess accessesOf(sde::SdeCuWorkOp cu) {
  CuAccess access;
  for (Value token : cu.getTokens()) {
    auto tokenOp = token.getDefiningOp<sde::SdeMuTokenOp>();
    if (!tokenOp)
      continue;
    Value root = tokenOp.getSource();
    switch (tokenOp.getMode()) {
    case sde::SdeAccessMode::read:
      access.reads.insert(root);
      break;
    case sde::SdeAccessMode::write:
      access.writes.insert(root);
      break;
    case sde::SdeAccessMode::readwrite:
      access.reads.insert(root);
      access.writes.insert(root);
      break;
    }
  }
  return access;
}

/// A conflict exists when one CU writes a root the other reads or writes.
static bool conflicts(const CuAccess &earlier, const CuAccess &later) {
  for (Value root : earlier.writes)
    if (later.reads.contains(root) || later.writes.contains(root))
      return true;
  for (Value root : later.writes)
    if (earlier.reads.contains(root))
      return true;
  return false;
}

/// True if a plain `sde.su_barrier` orders `earlier` before `later` (both in
/// the same block). A barrier between them is the explicit last-resort
/// ordering.
static bool barrierBetween(Operation *earlier, Operation *later) {
  for (Operation *cursor = earlier->getNextNode(); cursor && cursor != later;
       cursor = cursor->getNextNode())
    if (isa<sde::SdeSuBarrierOp>(cursor))
      return true;
  return false;
}

struct VerifySdePass : public sde::impl::VerifySdeBase<VerifySdePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool failed = false;
    llvm::DenseMap<Operation *, bool> funcHasSdeCache;

    auto funcHasSde = [&](func::FuncOp fn) -> bool {
      auto it = funcHasSdeCache.find(fn);
      if (it != funcHasSdeCache.end())
        return it->second;
      bool found = false;
      fn.walk([&](Operation *op) {
        if (isSdeDialectOp(op)) {
          found = true;
          return WalkResult::interrupt();
        }
        return WalkResult::advance();
      });
      funcHasSdeCache[fn] = found;
      return found;
    };

    // Dependency graph rejection plus CU containment.
    module.walk([&](Operation *op) {
      // Reject mu_dep when consumed as a generic edge.
      if (auto dep = dyn_cast<sde::SdeMuDepOp>(op)) {
        if (!dep.getDep().use_empty()) {
          dep.emitOpError()
              << "result is consumed; sde.mu_dep must remain a local "
                 "declaration";
          failed = true;
        }
      }
      // Reject generic token/dataflow dependency graphs.
      if (auto barrier = dyn_cast<sde::SdeSuBarrierOp>(op)) {
        if (!barrier.getTokens().empty()) {
          barrier.emitOpError()
              << "carries a generic token/dataflow dependency graph: a "
                 "token-carrying sde.su_barrier waits on a completion-token "
                 "graph, which SDE does not have. Use a plain "
                 "sde.su_barrier "
                 "or SU sequence/parallel-wave / source effect-control "
                 "ordering";
          failed = true;
        }
      }
      // Reject control_token when consumed as a generic edge.
      if (auto token = dyn_cast<sde::SdeControlTokenOp>(op)) {
        for (Operation *user : token.getToken().getUsers()) {
          if (!isa<sde::SdeSuBarrierOp>(user)) {
            token.emitOpError()
                << "result is used as a generic token/dataflow dependency "
                   "edge; "
                   "SDE has no generic token/dataflow dependency graph";
            failed = true;
            break;
          }
        }
      }

      // Diagnose only maximal source-compute roots so a whole raw nest yields
      // one diagnostic at its root.
      if (!isSourceComputeOp(op))
        return;
      Operation *parent = op->getParentOp();
      if (parent && isSourceComputeOp(parent))
        return; // inner op of a compute nest; root carries the diagnostic
      if (parent && isSuOp(parent)) {
        op->emitOpError()
            << "is raw scf/source compute directly inside an SU body; nest it "
               "in a CU (sde.cu_work / sde.cu_region / ...). scf is legal "
               "under "
               "SDE only inside a CU";
        failed = true;
      } else if (!hasCuAncestor(op)) {
        auto fn = op->getParentOfType<func::FuncOp>();
        if (fn && funcHasSde(fn)) {
          op->emitOpError()
              << "is source executable work outside any CU; all source "
                 "executable work (including non-OpenMP host "
                 "init/check/reduction/scalar-effect work) belongs in a CU";
          failed = true;
        }
      }
      // else: parent is a CU (or the op is inside a CU) — legal.
    });

    // Conflicting sibling cu_work without explicit ordering relies on hidden
    // textual order. Distinct SU bands live in distinct blocks, so they are not
    // flagged. Only fires when access info is representable.
    module.walk([&](Block *block) {
      llvm::SmallVector<sde::SdeCuWorkOp, 8> cus;
      for (Operation &op : *block)
        if (auto cu = dyn_cast<sde::SdeCuWorkOp>(&op))
          cus.push_back(cu);
      if (cus.size() < 2)
        return;

      llvm::SmallVector<CuAccess, 8> accesses;
      accesses.reserve(cus.size());
      for (sde::SdeCuWorkOp cu : cus)
        accesses.push_back(accessesOf(cu));

      for (size_t i = 0; i < cus.size(); ++i)
        for (size_t j = i + 1; j < cus.size(); ++j) {
          if (!conflicts(accesses[i], accesses[j]))
            continue;
          if (barrierBetween(cus[i].getOperation(), cus[j].getOperation()))
            continue;
          cus[j].emitOpError()
              << "relies on hidden textual order: it conflicts with an earlier "
                 "compute unit on a shared memory unit but has no explicit "
                 "ordering. CU ops are executable bodies, not scheduling "
                 "scopes; order them via "
                 "SU sequence/parallel-wave structure, source effect/control "
                 "ordering, or sde.su_barrier";
          failed = true;
        }
    });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::sde::createVerifySdePass() {
  return std::make_unique<VerifySdePass>();
}
