//===----------------------------------------------------------------------===//
// Db/DbValueDataFlowAnalysis.h - Sparse forward value facts for DB ops
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_DB_DBVALUEDATAFLOWANALYSIS_H
#define ARTS_ANALYSIS_DB_DBVALUEDATAFLOWANALYSIS_H

#include "arts/ArtsDialect.h"
#include "mlir/Analysis/DataFlow/SparseAnalysis.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

struct DbFactsValue {
  bool isAlloc = false;  // this value is a DB allocation
  unsigned inCount = 0;  // number of times used as "in"
  unsigned outCount = 0; // number of times used as "out"

  static DbFactsValue join(const DbFactsValue &lhs, const DbFactsValue &rhs) {
    DbFactsValue out;
    out.isAlloc = lhs.isAlloc || rhs.isAlloc;
    out.inCount = lhs.inCount + rhs.inCount;
    out.outCount = lhs.outCount + rhs.outCount;
    return out;
  }

  bool operator==(const DbFactsValue &rhs) const {
    return isAlloc == rhs.isAlloc && inCount == rhs.inCount &&
           outCount == rhs.outCount;
  }

  void print(raw_ostream &os) const {
    os << "DbFactsValue(alloc=" << (isAlloc ? 1 : 0) << ", in=" << inCount
       << ", out=" << outCount << ")";
  }
};

using DbLattice = dataflow::Lattice<DbFactsValue>;

class DbValueDataFlowAnalysis
    : public dataflow::SparseForwardDataFlowAnalysis<DbLattice> {
public:
  explicit DbValueDataFlowAnalysis(DataFlowSolver &solver)
      : dataflow::SparseForwardDataFlowAnalysis<DbLattice>(solver) {}

  void visitOperation(Operation *op, ArrayRef<const DbLattice *> operands,
                      ArrayRef<DbLattice *> results) override;

  void setToEntryState(DbLattice *lat) override { /* default zero */ }
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBVALUEDATAFLOWANALYSIS_H
