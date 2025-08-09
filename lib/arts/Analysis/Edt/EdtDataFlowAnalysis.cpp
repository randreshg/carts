//===----------------------------------------------------------------------===//
// Edt/EdtDataFlowAnalysis.cpp
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Edt/EdtDataFlowAnalysis.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "edt-dataflow"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::dataflow;

void EdtDataFlowAnalysis::visitOperation(Operation *op,
                                         ArrayRef<const EdtLattice *> operands,
                                         ArrayRef<EdtLattice *> results) {
  // Default: propagate identity
  if (results.empty())
    return;

  EdtFactsValue agg{};

  // Aggregate from operands
  for (const EdtLattice *lat : operands) {
    if (!lat)
      continue;
    agg = EdtFactsValue::join(agg, lat->getValue());
  }

  // Track `arts.db_dep` modes and trivial read/write from side-effects
  if (auto dep = dyn_cast<arts::DbDepOp>(op)) {
    auto modeAttr = dep.getModeAttr();
    if (modeAttr) {
      auto mode = modeAttr.getValue();
      if (mode == "in" || mode == "read")
        agg.inCount += 1, agg.reads = true;
      else if (mode == "out" || mode == "write")
        agg.outCount += 1, agg.writes = true;
      else // inout / unknown
        agg.inCount += 1, agg.outCount += 1, agg.reads = true, agg.writes = true;
      LLVM_DEBUG(DBGS() << "facts: db_dep mode=" << mode << "\n");
    }
  }

  // Side effects heuristic
  if (auto mei = dyn_cast_or_null<MemoryEffectOpInterface>(op)) {
    SmallVector<MemoryEffects::EffectInstance> effs;
    mei.getEffects(effs);
    for (auto &e : effs) {
      if (isa<MemoryEffects::Read>(e.getEffect()))
        agg.reads = true;
      if (isa<MemoryEffects::Write>(e.getEffect()))
        agg.writes = true;
    }
  }

  for (EdtLattice *res : results) {
    if (!res)
      continue;
    (void)res->join(agg);
    LLVM_DEBUG(DBGS() << "facts: result " << *op << " in=" << agg.inCount
                      << " out=" << agg.outCount << " R=" << agg.reads
                      << " W=" << agg.writes << "\n");
  }
}


