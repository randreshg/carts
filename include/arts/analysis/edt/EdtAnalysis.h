///==========================================================================///
/// File: EdtAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.edt operations.
/// It also provides helpers to analyze regions and find dependencies,
/// parameters, and constants.
///==========================================================================///

#ifndef ARTS_ANALYSIS_EDT_EDTANALYSIS_H
#define ARTS_ANALYSIS_EDT_EDTANALYSIS_H

#include "arts/analysis/Analysis.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/StringRef.h"

#include "arts/analysis/edt/EdtInfo.h"
#include "arts/analysis/graphs/edt/EdtGraph.h"

#include <optional>

namespace mlir {
namespace arts {

class LoopAnalysis;
class MetadataManager;

///==========================================================================///
/// ReductionOp: recognized reduction operation kinds
///==========================================================================///

enum class ReductionOp {
  ADD,
  MIN,
  MAX,
  OR,
  AND,
  XOR,
};

///==========================================================================///
/// EdtAnalysis: per-EDT summaries and pairwise affinity metrics
///==========================================================================///

class EdtAnalysis : public ArtsAnalysis {
public:
  struct ParallelEdtWorkSummary {
    bool hasReductionLoop = false;
    int64_t peakWorkPerWorker = 0;
  };

  EdtAnalysis(AnalysisManager &AM);

  /// Build summaries for all EDTs in the module.
  void analyze();

  /// Debug printing
  void print(func::FuncOp func, llvm::raw_ostream &os);
  void toJson(func::FuncOp func, llvm::raw_ostream &os);

  /// Get EDT-level summary distribution pattern.
  std::optional<EdtDistributionPattern> getEdtDistributionPattern(EdtOp edt);

  /// Get DB access pattern inferred from EDT-level loop access analysis.
  /// Returns std::nullopt when the allocation was not observed in analyzed
  /// loops.
  std::optional<DbAccessPattern> getAllocAccessPattern(Operation *allocOp);

  /// Return true when two EDTs share at least one common input that both only
  /// read. This is used by fusion profitability policy, not legality.
  bool hasSharedReadonlyInputs(EdtOp first, EdtOp second);

  /// Return the canonical captured-value summary for one EDT.
  const EdtCaptureSummary *getCaptureSummary(EdtOp edt);

  /// Summarize the dominant loop workload inside one parallel EDT. The
  /// returned peak work is normalized per worker so heuristics can compare
  /// launch amortization against preserved parallel slack.
  ParallelEdtWorkSummary summarizeParallelEdtWork(EdtOp edt,
                                                  int64_t workerCount);

  /// Iterate all analyzed DB allocation access patterns.
  void forEachAllocAccessPattern(
      llvm::function_ref<void(Operation *, DbAccessPattern)> fn);

  /// Graph accessor
  EdtGraph &getOrCreateEdtGraph(func::FuncOp func);

  /// Invalidate EDT graph for a specific function
  bool invalidateGraph(func::FuncOp func);

  /// Invalidate internal caches
  void invalidate() override;

  /// Check if a parallel EDT is fusable: body contains only arts.for ops
  /// and the yield terminator, no dependencies, no barriers or acquires.
  static bool isParallelEdtFusable(EdtOp edt);

  /// Convenience: get EDT node by op (derives parent func internally).
  EdtNode *getEdtNode(EdtOp op);

  /// Check if a value is invariant within an EDT region.
  /// A value is invariant if it is defined outside the region or not modified
  /// within it.
  static bool isInvariantInEdt(Region &edtRegion, Value value);

  /// Checks if the target operation is reachable from the source operation in
  /// the EDT control flow graph.
  static bool isReachable(Operation *source, Operation *target);

  /// Expose sub-analyses so that EdtNode / EdtGraph can reach them
  /// without storing a raw AnalysisManager pointer.
  MetadataManager &getMetadataManager();
  LoopAnalysis &getLoopAnalysis();

  //===--------------------------------------------------------------------===//
  /// Reduction pattern analysis.
  //===--------------------------------------------------------------------===//

  /// Try to classify a binary arithmetic operation as a reduction operation.
  static std::optional<ReductionOp> classifyReductionOp(Operation *op);

  /// Return a human-readable string name for a ReductionOp kind.
  static StringRef reductionOpName(ReductionOp op);

  /// Check whether a reduction operation is suitable for atomic lowering.
  static bool isAtomicCapable(ReductionOp op);

  /// Match a load-modify-store reduction pattern on a DbRefOp result within
  /// an EDT body.
  static std::optional<ReductionOp> matchLoadModifyStore(Value dbRefResult,
                                                         Region &edtBody);

  using ArtsAnalysis::getAnalysisManager;

private:
  /// Order index per EDT in a function
  DenseMap<EdtOp, unsigned> edtOrderIndex;
  DenseMap<Operation *, EdtDistributionPattern> edtPatternByOp;
  DenseMap<Operation *, DbAccessPattern> allocPatternByOp;
  bool analyzed = false;

  /// Per-function graph caches
  llvm::DenseMap<func::FuncOp, std::unique_ptr<EdtGraph>> edtGraphs;

  void analyzeFunc(func::FuncOp func);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_EDT_EDTANALYSIS_H
