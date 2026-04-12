///==========================================================================///
/// File: PartitionStrategy.h
///
/// Abstract interface for DB partitioning strategies.
///
/// Responsibility split:
///   - PartitionStrategy: given a DbOp, analysis results, device capabilities,
///     produce a partition decision
///   - DbPartitioning: controller that assembles context, invokes strategies,
///     and applies rewrite plans
///   - DbHeuristics: legacy decision layer (will delegate to strategies in B2)
///
/// Strategy architecture (Track B1):
///   - CoarsePartitionStrategy:       H1.C0-H1.C5 (single datablock)
///   - BlockPartitionStrategy:        H1.B1-H1.B4 (block partitioning)
///   - StencilPartitionStrategy:      H1.S1-H1.S3 (stencil/ESD mode)
///   - FineGrainedPartitionStrategy:  H1.E1-H1.E2 (element-wise)
///
/// Design goals:
///   1. Isolate decision logic from controller orchestration
///   2. Enable per-strategy unit testing and tuning
///   3. Prepare for ML-based cost model integration (Track C)
///   4. Maintain backward compatibility during migration (Track B2)
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_TRANSFORMS_DB_PARTITIONSTRATEGY_H
#define ARTS_DIALECT_CORE_TRANSFORMS_DB_PARTITIONSTRATEGY_H

#include "arts/dialect/core/Analysis/heuristics/PartitioningHeuristics.h"
#include "arts/utils/machine/RuntimeConfig.h"
#include "llvm/ADT/StringRef.h"
#include <memory>
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// PartitionStrategy - Abstract interface for partition decision strategies
///===----------------------------------------------------------------------===///

/// Abstract base class for partition decision strategies.
///
/// Each strategy implements a specific partitioning approach:
///   - CoarsePartitionStrategy: evaluates H1.C0-H1.C5
///   - BlockPartitionStrategy: evaluates H1.B1-H1.B4
///   - StencilPartitionStrategy: evaluates H1.S1-H1.S3
///   - FineGrainedPartitionStrategy: evaluates H1.E1-H1.E2
///
/// Strategies are evaluated in priority order by DbHeuristics (Track B2).
class PartitionStrategy {
public:
  virtual ~PartitionStrategy() = default;

  /// Evaluate whether this strategy applies to the given context.
  /// Returns a PartitioningDecision if applicable, std::nullopt otherwise.
  ///
  /// Contract:
  ///   - Strategies must NOT mutate the context
  ///   - Strategies should return quickly if not applicable
  ///   - rationale field must identify the specific heuristic (e.g., "H1.C0")
  virtual std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx,
           const RuntimeConfig *machine) const = 0;

  /// Returns the strategy name for logging and diagnostics.
  virtual llvm::StringRef getName() const = 0;

  /// Returns the priority order for this strategy (lower = higher priority).
  /// Coarse: 0, Block: 100, Stencil: 200, FineGrained: 300
  virtual unsigned getPriority() const = 0;

protected:
  PartitionStrategy() = default;
};

///===----------------------------------------------------------------------===///
/// Strategy Factory
///===----------------------------------------------------------------------===///

/// Factory for creating partition strategies.
/// Ownership: caller owns the returned unique_ptr.
class PartitionStrategyFactory {
public:
  /// Create all standard strategies in priority order.
  static llvm::SmallVector<std::unique_ptr<PartitionStrategy>>
  createStandardStrategies();

  /// Create individual strategies (for testing or custom pipelines).
  static std::unique_ptr<PartitionStrategy> createCoarseStrategy();
  static std::unique_ptr<PartitionStrategy> createBlockStrategy();
  static std::unique_ptr<PartitionStrategy> createStencilStrategy();
  static std::unique_ptr<PartitionStrategy> createFineGrainedStrategy();

private:
  PartitionStrategyFactory() = delete;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_DB_PARTITIONSTRATEGY_H
