///==========================================================================///
/// File: VerifySde.cpp
///
/// Structural verifier for the SDE boundary. SDE owns State, Dependency, and
/// Effect over MU/CU/SU. By the boundary the IR must already be the target
/// shape, and this pass fails closed when it is not. It enforces three rules,
/// reading only current IR and local op structure (no metadata, no downstream
/// contract):
///
///   No target SDE dependency graph. SDE carries no mu_dep dependency
///     graph and no generic token/dataflow dependency graph; ordering edges are
///     derived in CODIR after codelet isolation.
///   All source executable work lives in a CU. Raw scf / source
///     compute may not sit directly inside an SU body, and (within SDE-bearing
///     functions) source work may not sit outside every CU. scf is legal under
///     SDE only inside a CU.
///   CUs are async/schedulable by default. Sibling CUs with a
///   provable
///     MU access conflict must be ordered explicitly, not by textual order.
///
/// Deliberately NOT checked: CU `IsolatedFromAbove` (a CODIR concern), the
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

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::sde;

namespace {

// The structural predicates this verifier branches on — isSdeDialectOp, isCuOp,
// isSuOp, isSchedulePlumbing, isSourceComputeOp — are the same predicates the
// `sde-cu-normalization` transform wraps against. They live in
// `carts/dialect/sde/Utils/SdeCuStructure.h` as the single source of truth so
// the verifier's accept set and the transform's wrap set never drift.

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
      // Reject persistent mu_dep dependency graphs.
      if (auto task = dyn_cast<sde::SdeCuTaskOp>(op)) {
        if (!task.getDeps().empty()) {
          task.emitOpError()
              << "carries a target SDE dependency graph: sde.cu_task deps "
                 "express an mu_dep dependency graph, which SDE does not "
                 "have. Order CUs via SU sequence/parallel-wave structure, "
                 "source effect/control ordering, or sde.su_barrier";
          failed = true;
        }
      }
      // Reject mu_dep when consumed as a generic edge instead of a cu_task dep.
      if (auto dep = dyn_cast<sde::SdeMuDepOp>(op)) {
        for (Operation *user : dep.getDep().getUsers()) {
          if (!isa<sde::SdeCuTaskOp>(user)) {
            dep.emitOpError()
                << "result feeds a target SDE dependency graph; SDE has no "
                   "target mu_dep dependency graph";
            failed = true;
            break;
          }
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
                 "ordering. CUs are async/schedulable by default; order them "
                 "via "
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
