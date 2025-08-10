//===----------------------------------------------------------------------===//
// Edt/EdtDataFlowAnalysis.cpp
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Edt/EdtDataFlowAnalysis.h"
#include "llvm/Support/Debug.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt-dataflow)

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
      ARTS_INFO("facts: db_dep mode=" << mode);
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
    ARTS_INFO("facts: result " << *op << " in=" << agg.inCount
                                << " out=" << agg.outCount << " R="
                                << agg.reads << " W=" << agg.writes);
  }
}


