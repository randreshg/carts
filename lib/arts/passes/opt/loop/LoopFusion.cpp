///==========================================================================///
/// File: LoopFusion.cpp
///
/// Fuses independent arts.for loops within same parallel EDT.
/// Uses AnalysisManager and LoopAnalysis for bounds compatibility while
/// proving independence from per-loop memory hazards.
///
/// Before (2 separate loops with implicit barrier between them):
///   arts.edt <parallel> {
///     arts.for(%c0) to(%N) {  /// Stage 1: E = A * B
///       %E_view = arts.db_acquire ...
///       /// compute E[i]
///       arts.db_release %E_view
///     }
///     /// implicit barrier here
///     arts.for(%c0) to(%N) {  /// Stage 2: F = C * D (independent!)
///       %F_view = arts.db_acquire ...
///       /// compute F[i]
///       arts.db_release %F_view
///     }
///   }
///
/// After (independent loops fused, no barrier):
///   arts.edt <parallel> {
///     arts.for(%c0) to(%N) {  /// Fused: E & F computed together
///       %E_view = arts.db_acquire ...
///       /// compute E[i]
///       arts.db_release %E_view
///       %F_view = arts.db_acquire ...
///       /// compute F[i]
///       arts.db_release %F_view
///     }
///   }
///
/// This reduces EDT overhead and synchronization costs when loops have no
/// cross-loop write hazards. Shared read-only inputs are allowed.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#define GEN_PASS_DEF_LOOPFUSION
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/passes/Passes.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/IRMapping.h"
#include "llvm/ADT/DenseMap.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(loop_fusion);

using namespace mlir;
using namespace mlir::arts;

namespace {
struct AccessSummary {
  bool reads = false;
  bool writes = false;

  void record(DbUtils::MemoryAccessKind kind) {
    if (kind == DbUtils::MemoryAccessKind::Read)
      reads = true;
    else
      writes = true;
  }
};

struct LoopAccessSummary {
  DenseMap<Operation *, AccessSummary> datablocks;
  DenseMap<Value, AccessSummary> rawMemrefs;

  size_t size() const { return datablocks.size() + rawMemrefs.size(); }
};

struct LoopFusionPass : public impl::LoopFusionBase<LoopFusionPass> {
  LoopFusionPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    /// Get LoopAnalysis from AnalysisManager
    LoopAnalysis &loopAnalysis = AM->getLoopAnalysis();

    module.walk([&](EdtOp parallelEdt) {
      if (parallelEdt.getType() != EdtType::parallel)
        return;

      SmallVector<ForOp, 8> forOps;
      for (Operation &op : parallelEdt.getBody().front()) {
        if (auto forOp = dyn_cast<ForOp>(&op))
          forOps.push_back(forOp);
      }

      bool changed = true;
      while (changed) {
        changed = false;
        for (size_t i = 0; i + 1 < forOps.size(); ++i) {
          ForOp a = forOps[i];
          ForOp b = forOps[i + 1];

          /// Skip if barrier between them
          Operation *next = a->getNextNode();
          while (next && next != b.getOperation()) {
            if (isa<BarrierOp>(next))
              break;
            next = next->getNextNode();
          }
          if (next != b.getOperation())
            continue;

          if (haveCompatibleBounds(a, b) &&
              areIndependent(a, b, loopAnalysis)) {
            ARTS_INFO("Fusing independent for loops");
            fuseForOps(a, b);
            forOps.erase(forOps.begin() + i + 1);
            changed = true;
            break;
          }
        }
      }
    });
  }

private:
  mlir::arts::AnalysisManager *AM = nullptr;

  bool haveCompatibleBounds(ForOp a, ForOp b);
  bool areIndependent(ForOp a, ForOp b, LoopAnalysis &loopAnalysis);
  void fuseForOps(ForOp first, ForOp second);
  LoopAccessSummary getAccessSummary(ForOp forOp);
};
} // namespace

bool LoopFusionPass::haveCompatibleBounds(ForOp a, ForOp b) {
  auto &loopAnalysis = AM->getLoopAnalysis();
  LoopNode *nodeA = loopAnalysis.getOrCreateLoopNode(a.getOperation());
  LoopNode *nodeB = loopAnalysis.getOrCreateLoopNode(b.getOperation());
  return LoopNode::haveCompatibleBounds(nodeA, nodeB);
}

/// Collect the per-loop read/write summary keyed by the underlying datablock
/// when available. If an access does not trace to a datablock, fall back to
/// the raw memref value so non-DB scratch buffers are still treated
/// conservatively.
LoopAccessSummary LoopFusionPass::getAccessSummary(ForOp forOp) {
  LoopAccessSummary accesses;
  forOp.walk([&](Operation *op) {
    auto access = DbUtils::getMemoryAccessInfo(op);
    if (!access)
      return;

    if (Operation *db = DbUtils::getUnderlyingDb(access->memref)) {
      accesses.datablocks[db].record(access->kind);
      ARTS_DEBUG("  Access -> underlying DB: " << *db);
      return;
    }

    accesses.rawMemrefs[access->memref].record(access->kind);
    ARTS_DEBUG("  Access -> raw memref: " << access->memref);
  });
  ARTS_DEBUG("ForOp accesses " << accesses.size()
                               << " unique memory root(s)");
  return accesses;
}

bool LoopFusionPass::areIndependent(ForOp a, ForOp b,
                                    LoopAnalysis &loopAnalysis) {
  auto loopAClass = classifyPointwiseLoopCompute(a);
  auto loopBClass = classifyPointwiseLoopCompute(b);
  if (loopAClass != loopBClass) {
    ARTS_DEBUG("Loops have different pointwise compute classes - keep "
               "separate");
    return false;
  }

  auto hasCrossLoopHazard = [](const AccessSummary &lhs,
                               const AccessSummary &rhs) {
    /// Shared reads are safe to fuse. Any shared writer must preserve the
    /// existing barrier.
    return lhs.writes || rhs.writes;
  };

  /// First check: no shared writable accesses between loops.
  ARTS_INFO("Checking independence between two ForOps");
  ARTS_DEBUG("Loop A:");
  LoopAccessSummary aAccesses = getAccessSummary(a);
  ARTS_DEBUG("Loop B:");
  LoopAccessSummary bAccesses = getAccessSummary(b);

  for (auto &[db, accessA] : aAccesses.datablocks) {
    auto it = bAccesses.datablocks.find(db);
    if (it != bAccesses.datablocks.end() &&
        hasCrossLoopHazard(accessA, it->second)) {
      ARTS_DEBUG("Loops share writable datablock - not independent");
      return false;
    }
  }

  for (auto &[memref, accessA] : aAccesses.rawMemrefs) {
    auto it = bAccesses.rawMemrefs.find(memref);
    if (it != bAccesses.rawMemrefs.end() &&
        hasCrossLoopHazard(accessA, it->second)) {
      ARTS_DEBUG("Loops share writable raw memref - not independent");
      return false;
    }
  }

  /// Second check: consult loop metadata for diagnostics only. Internal
  /// loop-carried dependences do not, by themselves, block fusion because the
  /// fused loop preserves each iteration's program order. Only cross-loop
  /// hazards matter here.
  LoopNode *loopNodeA = loopAnalysis.getLoopNode(a.getOperation());
  LoopNode *loopNodeB = loopAnalysis.getLoopNode(b.getOperation());

  if (loopNodeA && loopNodeB) {
    if (loopNodeA->hasInterIterationDeps.has_value() &&
        loopNodeA->hasInterIterationDeps.value()) {
      ARTS_DEBUG("Loop A has inter-iteration dependencies");
    }
    if (loopNodeB->hasInterIterationDeps.has_value() &&
        loopNodeB->hasInterIterationDeps.value()) {
      ARTS_DEBUG("Loop B has inter-iteration dependencies");
    }
  }

  ARTS_DEBUG("Loops are independent - can fuse");
  return true; /// No shared memrefs - loops are independent
}

void LoopFusionPass::fuseForOps(ForOp first, ForOp second) {
  OpBuilder builder(second);
  Block &firstBody = first.getRegion().front();
  Block &secondBody = second.getRegion().front();

  IRMapping mapper;
  if (secondBody.getNumArguments() > 0 && firstBody.getNumArguments() > 0)
    mapper.map(secondBody.getArgument(0), firstBody.getArgument(0));

  builder.setInsertionPoint(firstBody.getTerminator());
  for (Operation &op : secondBody.without_terminator())
    builder.clone(op, mapper);

  second.erase();
}

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createLoopFusionPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<LoopFusionPass>(AM);
}
} // namespace arts
} // namespace mlir
