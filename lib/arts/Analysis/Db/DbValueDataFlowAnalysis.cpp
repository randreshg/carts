//===----------------------------------------------------------------------===//
// Db/DbValueDataFlowAnalysis.cpp
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Db/DbValueDataFlowAnalysis.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db-dataflow"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

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
    LLVM_DEBUG(DBGS() << "facts: db_alloc value alloc=1\n");
  }
  if (auto dep = dyn_cast<arts::DbDepOp>(op)) {
    auto modeAttr = dep.getModeAttr();
    if (modeAttr) {
      auto mode = modeAttr.getValue();
      if (mode == "in" || mode == "read")
        agg.inCount += 1;
      else if (mode == "out" || mode == "write")
        agg.outCount += 1;
      else
        agg.inCount += 1, agg.outCount += 1;
      LLVM_DEBUG(DBGS() << "facts: db_dep mode=" << mode << "\n");
    }
  }

  for (DbLattice *res : results) {
    if (!res)
      continue;
    (void)res->join(agg);
    LLVM_DEBUG(DBGS() << "facts: result in=" << agg.inCount
                      << " out=" << agg.outCount << " alloc=" << agg.isAlloc
                      << "\n");
  }
}


