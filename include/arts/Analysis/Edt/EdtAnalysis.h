///==========================================================================
/// File: EdtAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.edt operations.
/// It also provides helpers to analyze regions and find dependencies,
/// parameters, and constants.
///==========================================================================

#ifndef CARTS_ANALYSIS_EDTANALYSIS_H
#define CARTS_ANALYSIS_EDTANALYSIS_H

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace mlir {
namespace arts {

///==========================================================================
/// EdtEnvManager: region-local collector for EDT value usage
/// Responsibility: assist passes transforming EDT regions by identifying
/// parameters, constants, and dependencies used across region boundaries.
/// Non-Goals: inter-procedural analysis; global graph reasoning.
///==========================================================================
class EdtEnvManager {
public:
  EdtEnvManager(PatternRewriter &rewriter, Region &region);
  ~EdtEnvManager();

  /// Collect parameters and dependencies
  void naiveCollection(bool ignoreDeps = false);

  /// Analyze and adjust the set of parameters
  void adjust();

  /// Add interface
  bool addParameter(Value val);
  void addDependency(Value val, StringRef mode = "inout");

  /// Print
  void print();

  /// Getters
  Region &getRegion() { return region; }
  ArrayRef<Value> getParameters() { return parameters.getArrayRef(); }
  ArrayRef<Value> getConstants() { return constants.getArrayRef(); }
  ArrayRef<Value> getDependencies() { return dependencies.getArrayRef(); }
  DenseMap<Value, StringRef> &getDepsToProcess() { return depsToProcess; }

  void clearDepsToProcess() { depsToProcess.clear(); }

private:
  PatternRewriter &rewriter;
  Region &region;
  /// Set of possible parameters, constants, and dependencies
  SetVector<Value> parameters, constants, dependencies;
  /// Set of dependencies to process
  DenseMap<Value, StringRef> depsToProcess;
};

///==========================================================================
/// EdtAnalysis Class Declaration
///==========================================================================
// class EdtAnalysis {
// public:
//   explicit EdtAnalysis(Operation *module);

// private:
// };

} // namespace arts
} // namespace mlir

//==========================================================================
// EdtAnalysis: per-EDT summaries and pairwise affinity metrics
//==========================================================================

namespace mlir {
namespace arts {

class DbGraph;  // Forward declaration; provided by DB analysis
class EdtGraph; // Forward declaration; provided by EDT graph

/// High-level summary for an EDT region, used by placement heuristics.
struct EdtTaskSummary {
  uint64_t totalOps = 0;
  uint64_t numLoads = 0;
  uint64_t numStores = 0;
  uint64_t numCalls = 0;
  uint64_t bytesRead = 0;
  uint64_t bytesWritten = 0;
  uint64_t maxLoopDepth = 0;
  double computeToMemRatio =
      0.0; ///< (totalOps - (loads+stores)) / max(1, loads+stores)

  /// Base allocations (original memrefs) read and written by this EDT
  DenseSet<Value> basesRead;
  DenseSet<Value> basesWritten;

  /// When DbGraph is available: DbAlloc ops (as Operation*) read/written
  DenseSet<Operation *> dbAllocsRead;
  DenseSet<Operation *> dbAllocsWritten;

  /// Detailed per-alloc access metrics (when DbGraph is available)
  DenseMap<Operation *, uint64_t>
      dbAllocAccessCount; ///< loads+stores per alloc
  DenseMap<Operation *, uint64_t>
      dbAllocAccessBytes; ///< estimated bytes per alloc

  bool isReadOnly() const { return !basesRead.empty() && basesWritten.empty(); }
  bool isWriteOnly() const {
    return basesRead.empty() && !basesWritten.empty();
  }
  bool isReadWrite() const {
    return !basesRead.empty() && !basesWritten.empty();
  }
};

/// Pairwise affinity between two EDTs for placement decisions.
struct EdtPairAffinity {
  double dataOverlap = 0.0; ///< Jaccard overlap on base sets
  double hazardScore =
      0.0; ///< fraction of overlapping bases with at least one writer
  bool mayConflict = false; ///< true if hazardScore > 0
  double reuseProximity =
      0.0;                    ///< higher when tasks are closer in program order
  double localityScore = 0.0; ///< dataOverlap * reuseProximity
  double concurrencyRisk = 0.0; ///< hazardScore * dataOverlap
};

/// EdtAnalysis computes per-task summaries and pairwise affinities.
/// Optionally leverages DbGraph/EdtGraph when available; otherwise falls
/// back to conservative, intra-region inspection.
class EdtAnalysis {
public:
  explicit EdtAnalysis(Operation *module, DbGraph *db = nullptr,
                       EdtGraph *edt = nullptr);

  /// Build summaries for all EDTs in the module.
  void analyze();

  /// Get summary for a specific EDT; returns nullptr if unknown.
  const EdtTaskSummary *getSummary(Operation *edtOp) const;

  /// Compute affinity metrics for two EDTs using their summaries.
  EdtPairAffinity affinity(Operation *a, Operation *b) const;

  /// Debug printing
  void print(func::FuncOp func, llvm::raw_ostream &os) const;
  /// Export compact JSON summary for ArtsMate placement
  void toJson(func::FuncOp func, llvm::raw_ostream &os) const;
  /// Emit only the EDTs array section for bundling
  void emitEdtsArray(func::FuncOp func, llvm::raw_ostream &os) const;

private:
  Operation *module;
  DbGraph *dbGraph;   ///< optional
  EdtGraph *edtGraph; ///< optional

  DenseMap<Operation *, EdtTaskSummary> summaries;
  DenseMap<Operation *, unsigned>
      taskOrderIndex; ///< order index per EDT in a function

  void analyzeFunc(func::FuncOp func);
  EdtTaskSummary summarizeEdt(Operation *edtOp) const;

  /// Helpers
  static Value getOriginalAllocation(Value v);
  static unsigned getElementByteWidth(Type t);
  static uint64_t estimateMaxLoopDepth(Region &region);
};

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_EDTANALYSIS_H