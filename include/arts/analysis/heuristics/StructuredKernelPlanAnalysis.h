///==========================================================================///
/// File: StructuredKernelPlanAnalysis.h
///
/// First-class structured kernel plan analysis. Classifies regular kernels
/// and produces a StructuredKernelPlan that downstream passes consult for
/// distribution, partitioning, and lowering decisions.
///
/// If no plan is proven, downstream passes fall back to their existing
/// generic logic unchanged.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICS_STRUCTUREDKERNELPLANANALYSIS_H
#define ARTS_ANALYSIS_HEURISTICS_STRUCTUREDKERNELPLANANALYSIS_H

#include "arts/analysis/Analysis.h"
#include "arts/Dialect.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <optional>

namespace mlir {
namespace arts {

/// Kernel family classification.
enum class KernelFamily : uint8_t {
  Uniform,        /// Elementwise / uniform access
  Stencil,        /// Neighborhood stencil (Jacobi, convolution)
  Wavefront,      /// Wavefront / swept (Seidel-2D)
  ReductionMixed, /// Mixed reduction + spatial (matmul)
  TimestepChain,  /// Repeated timestep iteration
  Unknown         /// Unclassifiable / irregular
};

/// How iterations map to workers.
enum class IterationTopology : uint8_t {
  Serial,         /// Single thread
  OwnerStrip,     /// 1-D strip ownership
  OwnerTile,      /// N-D tile ownership
  WavefrontTopo,  /// Wavefront diagonal
  PersistentStrip, /// Long-lived owner strip
  PersistentTile   /// Long-lived owner tile
};

/// Legal async strategy for the kernel.
enum class AsyncStrategy : uint8_t {
  Blocking,                /// Epoch wait
  AdvanceEdt,              /// Finish-EDT continuation
  CpsChain,                /// CPS continuation chain
  PersistentStructuredRegion /// Persistent region
};

/// Repetition structure of the kernel.
enum class RepetitionStructure : uint8_t {
  None,        /// Single invocation
  PairStep,    /// Alternating buffer pair
  KStep,       /// K-step grouping
  FullTimestep /// Full timestep loop
};

/// Cost signals derived from normalized kernel properties.
struct PlanCostSignals {
  double schedulerOverhead = 0.0;
  double sliceWideningPressure = 0.0;
  double expectedLocalWork = 0.0;
  double relaunchAmortization = 0.0;
};

/// The compiler's single source of truth for a structured kernel's
/// execution plan.
struct StructuredKernelPlan {
  KernelFamily family = KernelFamily::Unknown;
  llvm::SmallVector<int64_t, 4> ownerDims;
  llvm::SmallVector<int64_t, 4> physicalBlockShape;
  llvm::SmallVector<int64_t, 4> logicalWorkerSlice;
  llvm::SmallVector<int64_t, 4> haloShape;
  IterationTopology topology = IterationTopology::Serial;
  RepetitionStructure repetition = RepetitionStructure::None;
  AsyncStrategy asyncStrategy = AsyncStrategy::Blocking;
  PlanCostSignals costSignals;

  bool isValid() const { return family != KernelFamily::Unknown; }
};

/// String conversion helpers for plan attribute stamping.
llvm::StringRef kernelFamilyToString(KernelFamily family);
std::optional<KernelFamily> parseKernelFamily(llvm::StringRef str);
llvm::StringRef iterationTopologyToString(IterationTopology topo);
std::optional<IterationTopology> parseIterationTopology(llvm::StringRef str);
llvm::StringRef asyncStrategyToString(AsyncStrategy strategy);
std::optional<AsyncStrategy> parseAsyncStrategy(llvm::StringRef str);
llvm::StringRef repetitionStructureToString(RepetitionStructure rep);
std::optional<RepetitionStructure>
parseRepetitionStructure(llvm::StringRef str);

/// Structured kernel plan analysis.
///
/// Runs after concurrency and before edt-distribution. Classifies regular
/// kernels from existing IR metadata and stamps plan attributes that
/// downstream passes consult.
class StructuredKernelPlanAnalysis : public ArtsAnalysis {
public:
  explicit StructuredKernelPlanAnalysis(AnalysisManager &AM);

  /// Compute plans for all parallel EDTs in the module.
  void run(ModuleOp module);

  /// Get the plan for a ForOp (returns nullptr if no plan).
  const StructuredKernelPlan *getPlan(ForOp forOp) const;

  /// Get the plan for an EdtOp (returns nullptr if no plan).
  const StructuredKernelPlan *getPlan(EdtOp edtOp) const;

  void invalidate() override;

private:
  /// Synthesize a plan for a single ForOp within an EdtOp.
  std::optional<StructuredKernelPlan> synthesizePlan(ForOp forOp,
                                                      EdtOp parentEdt);

  /// Classify kernel family from dep pattern and access patterns.
  KernelFamily classifyFamily(ForOp forOp, EdtOp parentEdt) const;

  /// Select iteration topology from family and machine config.
  IterationTopology selectTopology(KernelFamily family,
                                   bool isInternode) const;

  /// Select async strategy from repetition structure.
  AsyncStrategy selectAsyncStrategy(RepetitionStructure repetition) const;

  /// Detect repetition structure from enclosing loops.
  RepetitionStructure detectRepetition(ForOp forOp) const;

  /// Compute cost signals from plan properties.
  PlanCostSignals computeCostSignals(const StructuredKernelPlan &plan,
                                      ForOp forOp) const;

  bool hasRun = false;
  llvm::DenseMap<Operation *, StructuredKernelPlan> forOpPlans;
  llvm::DenseMap<Operation *, StructuredKernelPlan> edtOpPlans;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICS_STRUCTUREDKERNELPLANANALYSIS_H
