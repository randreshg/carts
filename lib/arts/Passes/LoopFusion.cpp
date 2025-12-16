///==========================================================================///
/// File: LoopFusion.cpp
///
/// Fuses independent arts.for loops within same parallel EDT.
/// Uses ArtsAnalysisManager and LoopAnalysis for independence checking.
///
/// Before (2 separate loops with implicit barrier between them):
///   arts.edt <parallel> {
///     arts.for(%c0) to(%N) {  // Stage 1: E = A * B
///       %E_view = arts.db_acquire ...
///       // compute E[i]
///       arts.db_release %E_view
///     }
///     // implicit barrier here
///     arts.for(%c0) to(%N) {  // Stage 2: F = C * D (independent!)
///       %F_view = arts.db_acquire ...
///       // compute F[i]
///       arts.db_release %F_view
///     }
///   }
///
/// After (independent loops fused, no barrier):
///   arts.edt <parallel> {
///     arts.for(%c0) to(%N) {  // Fused: E & F computed together
///       %E_view = arts.db_acquire ...
///       // compute E[i]
///       arts.db_release %E_view
///       %F_view = arts.db_acquire ...
///       // compute F[i]
///       arts.db_release %F_view
///     }
///   }
///
/// This reduces EDT overhead and synchronization costs when loops access
/// disjoint sets of datablocks.
///==========================================================================///

#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/IRMapping.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_loop_fusion);

using namespace mlir;
using namespace mlir::arts;

namespace {
struct LoopFusionPass : public arts::ArtsLoopFusionBase<LoopFusionPass> {
  LoopFusionPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    // Get LoopAnalysis from ArtsAnalysisManager
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

          // Skip if barrier between them
          Operation *next = a->getNextNode();
          while (next && next != b.getOperation()) {
            if (isa<BarrierOp>(next))
              break;
            next = next->getNextNode();
          }
          if (next != b.getOperation())
            continue;

          if (haveCompatibleBounds(a, b) && areIndependent(a, b, loopAnalysis)) {
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
  ArtsAnalysisManager *AM = nullptr;

  bool haveCompatibleBounds(ForOp a, ForOp b);
  bool areIndependent(ForOp a, ForOp b, LoopAnalysis &loopAnalysis);
  void fuseForOps(ForOp first, ForOp second);
  DenseSet<Operation *> getDbAccesses(ForOp forOp);
};
} // namespace

bool LoopFusionPass::haveCompatibleBounds(ForOp a, ForOp b) {
  auto getConst = [](Value v) -> std::optional<int64_t> {
    if (auto c = v.getDefiningOp<arith::ConstantIndexOp>())
      return c.value();
    return std::nullopt;
  };

  auto aLower = getConst(a.getLowerBound()[0]);
  auto bLower = getConst(b.getLowerBound()[0]);
  auto aUpper = getConst(a.getUpperBound()[0]);
  auto bUpper = getConst(b.getUpperBound()[0]);

  if (aLower && bLower && *aLower != *bLower)
    return false;
  if (aUpper && bUpper && *aUpper != *bUpper)
    return false;

  if (!aLower && a.getLowerBound()[0] != b.getLowerBound()[0])
    return false;
  if (!aUpper && a.getUpperBound()[0] != b.getUpperBound()[0])
    return false;

  return true;
}

/// Collects the underlying datablock Operations accessed by a ForOp.
/// Uses getUnderlyingDb to trace through db_ref, acquire chains, and
/// block arguments to identify the actual DbAllocOp/DbAcquireOp.
DenseSet<Operation *> LoopFusionPass::getDbAccesses(ForOp forOp) {
  DenseSet<Operation *> accesses;
  forOp.walk([&](Operation *op) {
    Value memref;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      memref = load.getMemRef();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      memref = store.getMemRef();
    else if (auto dbRef = dyn_cast<DbRefOp>(op))
      memref = dbRef.getResult();
    else if (auto acq = dyn_cast<DbAcquireOp>(op))
      memref = acq.getSourcePtr();
    else
      return;

    // Use ArtsUtils to trace to the underlying datablock
    if (Operation *db = getUnderlyingDb(memref)) {
      accesses.insert(db);
      ARTS_DEBUG("  Access -> underlying DB: " << *db);
    }
  });
  ARTS_DEBUG("ForOp accesses " << accesses.size() << " unique datablocks");
  return accesses;
}

bool LoopFusionPass::areIndependent(ForOp a, ForOp b,
                                        LoopAnalysis &loopAnalysis) {
  // First check: no shared datablock accesses between loops
  ARTS_DEBUG("=== Checking independence between two ForOps ===");
  ARTS_DEBUG("Loop A:");
  auto aAccesses = getDbAccesses(a);
  ARTS_DEBUG("Loop B:");
  auto bAccesses = getDbAccesses(b);

  for (Operation *db : aAccesses) {
    if (bAccesses.contains(db)) {
      ARTS_DEBUG("Loops share datablock - not independent");
      return false;
    }
  }

  // Second check: use LoopAnalysis to verify no cross-loop dependencies
  LoopNode *loopNodeA = loopAnalysis.getLoopNode(a.getOperation());
  LoopNode *loopNodeB = loopAnalysis.getLoopNode(b.getOperation());

  if (loopNodeA && loopNodeB) {
    // Check parallel classification from loop metadata
    auto classA = loopNodeA->parallelClassification;
    auto classB = loopNodeB->parallelClassification;

    // If either loop is Sequential due to dependencies, be conservative
    if (classA == LoopMetadata::ParallelClassification::Sequential ||
        classB == LoopMetadata::ParallelClassification::Sequential) {
      ARTS_DEBUG("One of the loops is Sequential - not fusing");
      return false;
    }

    // Check for inter-iteration dependencies
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
  return true; // No shared memrefs - loops are independent
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
std::unique_ptr<Pass> createLoopFusionPass(ArtsAnalysisManager *AM) {
  return std::make_unique<LoopFusionPass>(AM);
}
} // namespace arts
} // namespace mlir
