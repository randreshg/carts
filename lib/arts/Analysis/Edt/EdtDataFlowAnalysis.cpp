//===----------------------------------------------------------------------===//
// Edt/EdtDataFlowAnalysis.cpp
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Edt/EdtDataFlowAnalysis.h"
#include "llvm/Support/Debug.h"
// Effects
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt-dataflow)

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::dataflow;

void EdtDataFlowAnalysis::visitOperation(Operation *op,
                                         ArrayRef<const EdtLattice *> operands,
                                         ArrayRef<EdtLattice *> results) {
  if (results.empty()) return;

  EdtFactsValue agg{};
  // Merge incoming facts
  for (const EdtLattice *lat : operands) {
    if (!lat) continue;
    agg = EdtFactsValue::join(agg, lat->getValue());
  }

  // Track memory effects to flag reads/writes conservatively
  if (auto mei = dyn_cast<MemoryEffectOpInterface>(op)) {
    SmallVector<MemoryEffects::EffectInstance> effs;
    mei.getEffects(effs);
    for (auto &e : effs) {
      if (isa<MemoryEffects::Read>(e.getEffect())) agg.reads = true;
      if (isa<MemoryEffects::Write>(e.getEffect())) agg.writes = true;
    }
  }

  for (EdtLattice *res : results) {
    if (!res) continue;
    (void)res->join(agg);
    ARTS_INFO("edt-facts: op " << op->getName().getStringRef() << " R=" << agg.reads
                                << " W=" << agg.writes);
  }
}


