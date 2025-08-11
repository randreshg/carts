//===----------------------------------------------------------------------===//
// Db/DbValueDataFlowAnalysis.cpp
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Db/DbValueDataFlowAnalysis.h"
#include "llvm/Support/Debug.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db-dataflow)

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::dataflow;

void DbValueDataFlowAnalysis::visitOperation(
    Operation *op, ArrayRef<const DbLattice *> operands,
    ArrayRef<DbLattice *> results) {
  if (results.empty())
    return;

  DbFactsValue agg{};
  for (const DbLattice *lat : operands) {
    if (!lat)
      continue;
    agg = DbFactsValue::join(agg, lat->getValue());
  }

  if (auto alloc = dyn_cast<arts::DbAllocOp>(op)) {
    agg.isAlloc = true;
    ARTS_INFO("facts: db_alloc value alloc=1");
  }
  // if (auto dep = dyn_cast<arts::DbDepOp>(op)) {
  //   auto modeAttr = dep.getModeAttr();
  //   if (modeAttr) {
  //     auto mode = modeAttr.getValue();
  //     if (mode == "in" || mode == "read")
  //       agg.inCount += 1;
  //     else if (mode == "out" || mode == "write")
  //       agg.outCount += 1;
  //     else
  //       agg.inCount += 1, agg.outCount += 1;
  //      ARTS_INFO("facts: db_dep mode=" << mode);
  //   }
  // }

  for (DbLattice *res : results) {
    if (!res)
      continue;
    (void)res->join(agg);
    ARTS_INFO("facts: result in=" << agg.inCount
                                   << " out=" << agg.outCount
                                   << " alloc=" << agg.isAlloc);
  }
}


