//===----------------------------------------------------------------------===//
// Edt/EdtDataFlowAnalysis.h - Sparse forward dataflow for EDT deps
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_EDT_EDTDATAFLOWANALYSIS_H
#define ARTS_ANALYSIS_EDT_EDTDATAFLOWANALYSIS_H

#include "arts/ArtsDialect.h"
#include "mlir/Analysis/DataFlow/SparseAnalysis.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

/// A compact summary of EDT-related dependency/param effects.
struct EdtFactsValue {
  // Aggregate counts for dependency modes
  unsigned inCount = 0;
  unsigned outCount = 0;
  bool reads = false;
  bool writes = false;

  static EdtFactsValue join(const EdtFactsValue &lhs,
                            const EdtFactsValue &rhs) {
    EdtFactsValue out;
    out.inCount = lhs.inCount + rhs.inCount;
    out.outCount = lhs.outCount + rhs.outCount;
    out.reads = lhs.reads || rhs.reads;
    out.writes = lhs.writes || rhs.writes;
    return out;
  }

  bool operator==(const EdtFactsValue &rhs) const {
    return inCount == rhs.inCount && outCount == rhs.outCount &&
           reads == rhs.reads && writes == rhs.writes;
  }

  void print(raw_ostream &os) const {
    os << "EdtFactsValue(in=" << inCount << ", out=" << outCount
       << ", R=" << (reads ? 1 : 0) << ", W=" << (writes ? 1 : 0) << ")";
  }
};

/// Lattice wrapper for EdtFactsValue, anchored on SSA Values.
using EdtLattice = dataflow::Lattice<EdtFactsValue>;

/// Sparse forward analysis computing minimal dependency facts for values
/// crossing EDT boundaries. Primarily tracks `arts.db_dep` mode usage and
/// basic read/write summary for downstream passes.
class EdtDataFlowAnalysis : public dataflow::SparseForwardDataFlowAnalysis<EdtLattice> {
public:
  explicit EdtDataFlowAnalysis(DataFlowSolver &solver)
      : dataflow::SparseForwardDataFlowAnalysis<EdtLattice>(solver) {}

  void visitOperation(Operation *op, ArrayRef<const EdtLattice *> operands,
                      ArrayRef<EdtLattice *> results) override;

  void setToEntryState(EdtLattice *lat) override { /* default zeroed */ }
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_EDT_EDTDATAFLOWANALYSIS_H


