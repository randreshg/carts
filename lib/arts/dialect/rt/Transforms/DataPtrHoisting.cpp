///==========================================================================///
/// File: DataPtrHoisting.cpp
///
/// This pass hoists invariant ARTS dep/db pointer views out of loops and
/// recovers loop-local blocked slices when lowering leaves per-element dep/db
/// selection inside a worker EDT. Without this optimization, hot loops reload
/// dependency pointers O(n^k) times for k-nested loops and may rebuild
/// block-selection math even when the loop already stays inside one owned
/// blocked slice.
///
/// Before (O(n^2) loads - pointer loaded every iteration):
///   scf.for %i = ... {
///     scf.for %j = ... {
///       %ptr = llvm.load %dep_ptr_addr : !llvm.ptr  /// Redundant load!
///       %val = polygeist.load %ptr[%i, %j] : f64
///     }
///   }
///
/// After (O(1) loads - pointer loaded once before loop nest):
///   %ptr = llvm.load %dep_ptr_addr : !llvm.ptr  /// Hoisted out
///   scf.for %i = ... {
///     scf.for %j = ... {
///       %val = polygeist.load %ptr[%i, %j] : f64
///     }
///   }
///
/// LLVM's LICM cannot hoist these loads without TBAA metadata proving
/// they don't alias with data stores.
///==========================================================================///

#define GEN_PASS_DEF_DATAPTRHOISTING
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/dialect/rt/Transforms/DataPtrHoistingInternal.h"
#include "arts/utils/LoopInvarianceUtils.h"
#include "arts/utils/LoopUtils.h"
#include "mlir/Pass/Pass.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(data_ptr_hoisting);

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::arts::data_ptr_hoisting;

namespace {

struct DataPtrHoistingPass
    : public impl::DataPtrHoistingBase<DataPtrHoistingPass> {
  void runOnOperation() override;
};

} // namespace

void DataPtrHoistingPass::runOnOperation() {
  ModuleOp module = getOperation();
  ARTS_INFO_HEADER(DataPtrHoistingPass);

  int hoistedCount = 0, divRemHoisted = 0, dbPtrHoisted = 0, m2rHoisted = 0,
      singleBlockDepViews = 0, cachedNeighborPtrLoads = 0,
      versionedWindowLoops = 0, versionedWindowAccesses = 0;
  module.walk([&](func::FuncOp funcOp) {
    bool isEdt = funcOp.getName().starts_with("__arts_edt_");
    SmallVector<std::pair<LLVM::LoadOp, scf::ForOp>> loadsToHoist;
    DominanceInfo domInfo(funcOp);
    if (isEdt) {
      funcOp.walk([&](LLVM::LoadOp loadOp) {
        /// Check if this is a deps pointer load
        if (!isDepsPtrLoad(loadOp))
          return;

        /// Check if address operands are loop-invariant
        Value addr = loadOp.getAddr();
        Operation *addrOp = addr.getDefiningOp();
        if (!addrOp)
          return;

        scf::ForOp targetLoop = findHoistTarget(loadOp, addrOp, domInfo);
        if (!targetLoop)
          return;

        loadsToHoist.push_back({loadOp, targetLoop});
      });
    }

    /// Now hoist the collected loads
    for (auto &[loadOp, targetLoop] : loadsToHoist) {
      if (hoistLoadOutOfLoop(loadOp, targetLoop))
        hoistedCount++;
    }

    /// Hoist datablock pointer loads (db_gep + llvm.load) out of inner loops.
    SmallVector<std::tuple<LLVM::LoadOp, Operation *, scf::ForOp>>
        dbLoadsToHoist;
    funcOp.walk([&](LLVM::LoadOp loadOp) {
      if (!isDbPtrLoad(loadOp))
        return;

      Operation *addrOp = loadOp.getAddr().getDefiningOp();
      if (!addrOp)
        return;

      scf::ForOp targetLoop = findHoistTarget(loadOp, addrOp, domInfo);
      if (!targetLoop)
        return;

      dbLoadsToHoist.push_back({loadOp, addrOp, targetLoop});
    });

    for (auto &[loadOp, addrOp, targetLoop] : dbLoadsToHoist) {
      if (!targetLoop || !targetLoop->isAncestor(loadOp))
        continue;
      if (targetLoop->isAncestor(addrOp))
        addrOp->moveBefore(targetLoop);
      loadOp->moveBefore(targetLoop);
      dbPtrHoisted++;

      /// Also hoist pointer2memref users if they live inside the loop.
      for (Operation *user :
           llvm::make_early_inc_range(loadOp.getResult().getUsers())) {
        if (auto m2r = dyn_cast<polygeist::Pointer2MemrefOp>(user)) {
          if (!targetLoop->isAncestor(m2r))
            continue;
          m2r->moveBefore(targetLoop);
          m2rHoisted++;
        }
      }
    }

    /// Recover single-owner blocked dep slices before generic memref hoisting.
    /// When a worker loop already stays within one blocked acquire, the dep
    /// index is invariant and the loop can reuse the local IV instead of
    /// rebuilding dep_gep/div/rem per element.
    SmallVector<std::pair<LLVM::LoadOp, scf::ForOp>> singleBlockDepLoads;
    if (isEdt) {
      funcOp.walk([&](LLVM::LoadOp loadOp) {
        if (!isDepsPtrLoad(loadOp))
          return;
        auto loop = loadOp->getParentOfType<scf::ForOp>();
        if (!loop)
          return;
        singleBlockDepLoads.push_back({loadOp, loop});
      });
    }

    for (auto &[loadOp, loop] : singleBlockDepLoads) {
      if (!loadOp || !loop)
        continue;
      if (materializeSingleBlockBlockedDepView(loadOp, loop))
        singleBlockDepViews++;
    }

    /// Hoist loop-invariant pointer materializations after the dep/db load
    /// rewrites above so later stencil-specific rewrites see stable memref
    /// views instead of loop-local pointer2memref rebuilds.
    SmallVector<std::pair<polygeist::Pointer2MemrefOp, scf::ForOp>>
        ptr2memrefToHoist;
    if (isEdt) {
      funcOp.walk([&](polygeist::Pointer2MemrefOp ptr2memref) {
        scf::ForOp targetLoop =
            findInvariantOpHoistTarget(ptr2memref.getOperation(), domInfo);
        if (!targetLoop)
          return;
        ptr2memrefToHoist.push_back({ptr2memref, targetLoop});
      });
    }

    for (auto &[ptr2memref, targetLoop] : ptr2memrefToHoist) {
      if (!targetLoop || !targetLoop->isAncestor(ptr2memref))
        continue;
      ptr2memref->moveBefore(targetLoop);
      m2rHoisted++;
    }

    /// Materialize small invariant dep-pointer caches for loop-window stencil
    /// accesses. This turns per-iteration dep loads into loop-invariant loads
    /// plus an in-loop pointer select. The rewrite is keyed to the nearest
    /// enclosing loop that carries the partitioned index; it does not require
    /// that loop to be innermost because the cached pointer can still feed
    /// deeper inner loops.
    SmallVector<std::pair<LLVM::LoadOp, scf::ForOp>> cachedNeighborLoads;
    if (isEdt) {
      funcOp.walk([&](LLVM::LoadOp loadOp) {
        if (!isDepsPtrLoad(loadOp))
          return;
        auto loop = loadOp->getParentOfType<scf::ForOp>();
        if (!loop)
          return;
        cachedNeighborLoads.push_back({loadOp, loop});
      });
    }

    for (auto &[loadOp, loop] : cachedNeighborLoads) {
      if (!loadOp || !loop)
        continue;
      if (materializeNeighborPtrCache(loadOp, loop))
        cachedNeighborPtrLoads++;
    }

    /// After invariant pointer views are exposed and neighbor caches are
    /// materialized, split innermost stencil loops into boundary and interior
    /// bands so the bulk interior avoids per-element pointer/index selection.
    SmallVector<scf::ForOp> loopsToVersion;
    if (isEdt) {
      funcOp.walk([&](scf::ForOp loop) {
        if (isInnermostLoop(loop))
          loopsToVersion.push_back(loop);
      });
    }

    for (scf::ForOp loop : loopsToVersion) {
      if (!loop)
        continue;
      if (versionLoopWindowAccesses(loop, versionedWindowAccesses))
        versionedWindowLoops++;
    }

    /// Hoist div/rem out of inner loops when operands are invariant and the
    /// divisor is provably non-zero.
    funcOp.walk([&](scf::ForOp loop) {
      SmallVector<Operation *> divRemOps;
      for (Operation &op : loop.getBody()->getOperations()) {
        if (op.hasTrait<OpTrait::IsTerminator>())
          continue;
        if (isa<scf::ForOp>(op))
          continue;
        if (!isa<arith::DivUIOp, arith::RemUIOp>(op))
          continue;
        if (!isSafeDivRemToHoist(&op, loop, domInfo))
          continue;
        divRemOps.push_back(&op);
      }
      for (Operation *op : divRemOps) {
        op->moveBefore(loop);
        divRemHoisted++;
      }
    });
  });

  ARTS_INFO("Hoisted " << hoistedCount << " data pointer loads out of loops");
  ARTS_INFO("Hoisted " << dbPtrHoisted
                       << " datablock pointer loads out of loops");
  ARTS_INFO("Hoisted " << m2rHoisted << " pointer2memref ops out of loops");
  ARTS_INFO("Recovered " << singleBlockDepViews << " single-block dep views");
  ARTS_INFO("Hoisted " << divRemHoisted << " div/rem ops out of loops");
  ARTS_INFO("Cached " << cachedNeighborPtrLoads
                      << " loop-window dep pointer loads");
  ARTS_INFO("Versioned " << versionedWindowLoops
                         << " loop-window loops and rewrote "
                         << versionedWindowAccesses << " memory accesses");
  ARTS_INFO_FOOTER(DataPtrHoistingPass);
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {

std::unique_ptr<Pass> createDataPtrHoistingPass() {
  return std::make_unique<DataPtrHoistingPass>();
}

} // namespace arts
} // namespace mlir
