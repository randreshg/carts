///==========================================================================///
/// File: LoopMetadata.h
///
/// This file defines the LoopMetadata class for managing loop metadata
/// operations. It provides a clean, object-oriented interface for querying
/// and attaching metadata to loop operations (affine.for, scf.for,
/// scf.parallel, etc.).
///==========================================================================///

#ifndef ARTS_UTILS_LOOPMETADATA_H
#define ARTS_UTILS_LOOPMETADATA_H

#include "arts/Dialect.h"
#include "arts/utils/metadata/LocationMetadata.h"
#include "arts/utils/metadata/Metadata.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Operation.h"
#include "mlir/Support/LLVM.h"
#include <optional>
#include <string>

namespace mlir {
namespace arts {

/// Attribute name constants for loop metadata
namespace AttrNames {
namespace LoopMetadata {
using namespace llvm;
constexpr StringLiteral Name = "arts.loop";
constexpr StringLiteral PotentiallyParallel = "potentially_parallel";
constexpr StringLiteral HasReductions = "has_reductions";
constexpr StringLiteral ReductionKinds = "reduction_kinds";
constexpr StringLiteral TripCount = "trip_count";
constexpr StringLiteral NestingLevel = "nesting_level";
constexpr StringLiteral HasInterIterationDeps = "has_inter_iteration_deps";
constexpr StringLiteral MemrefsWithLoopCarriedDeps =
    "memrefs_with_loop_carried_deps";
constexpr StringLiteral ParallelClassification = "parallel_classification";
constexpr StringLiteral LocationKey = "location_key";
/// Loop reordering: target order by arts.id (empty = no reordering needed)
constexpr StringLiteral ReorderNestTo = "reorder_nest_to";
/// Per-dimension dependency analysis for nested loops
constexpr StringLiteral DimensionDeps = "dimension_deps";
constexpr StringLiteral OutermostParallelDim = "outermost_parallel_dim";
} // namespace LoopMetadata
} // namespace AttrNames

/// Per-dimension dependency info for nested loops.
/// Allows tracking which specific loop dimension carries dependencies,
/// enabling parallelization of outer loops even when inner loops have deps.
struct DimensionDependency {
  int64_t dimension = 0;               // 0 = outermost, 1 = next, etc.
  bool hasCarriedDep = false;          /// Does THIS dimension carry deps?
  std::optional<int64_t> distance;     /// Dependence distance if known
  SmallVector<Value> dependentMemrefs; // Which memrefs cause deps (optional)

  /// For JSON serialization
  void importFromJson(const llvm::json::Object &json);
  void exportToJson(llvm::json::Object &json) const;
};

///===--------------------------------------------------------------------===///
/// LoopMetadata - Manages metadata for loop operations
///
/// This class provides an interface for querying and attaching metadata
/// to loop operations.
///===----------------------------------------------------------------------===///
class LoopMetadata : public ArtsMetadata {
public:
  ///===-------------------------------------------------------------===///
  /// Enums
  ///===-------------------------------------------------------------===///
  /// Reduction operation kinds
  enum class ReductionKind { Add, Mul, Min, Max, And, Or, Xor, Unknown };
  const char *reductionKindToString(ReductionKind kind) const;
  ReductionKind stringToReductionKind(llvm::StringRef str) const;

  /// Parallel classification for loops
  enum class ParallelClassification { Unknown, ReadOnly, Likely, Sequential };
  const char *
  parallelClassificationToString(ParallelClassification classification) const;
  ParallelClassification
  stringToParallelClassification(llvm::StringRef str) const;

  ///===-------------------------------------------------------------===///
  /// Attributes
  ///===-------------------------------------------------------------===///
  /// Parallelism analysis
  bool potentiallyParallel = false, hasReductions = false;
  SmallVector<ReductionKind> reductionKinds;

  /// Loop structure information
  std::optional<int64_t> tripCount, nestingLevel;

  /// Dependency information
  std::optional<bool> hasInterIterationDeps;

  /// Aggregated memref usage information
  std::optional<int64_t> memrefsWithLoopCarriedDeps;
  std::optional<ParallelClassification> parallelClassification;

  /// Source Location
  LocationMetadata locationMetadata;

  /// Loop reordering: target order by arts.id (empty = no reordering needed)
  /// Populated during CollectMetadata if reordering is beneficial.
  /// Used by LoopReorderingPass to apply the transformation.
  SmallVector<int64_t> reorderNestTo;

  /// Per-dimension dependency analysis for nested loops.
  /// Each entry describes whether a specific nesting level carries
  /// dependencies. Key insight: inner loop deps don't prevent outer loop
  /// parallelism. Example: for(i) { for(j) { A[i][j] = f(A[i][j-1]) } }
  ///   - dimensionDeps[0] = {dim=0, hasCarriedDep=false} // i-loop is parallel
  ///   - dimensionDeps[1] = {dim=1, hasCarriedDep=true}  // j-loop has
  ///   A[i][j-1] dep
  SmallVector<DimensionDependency> dimensionDeps;

  /// The outermost dimension that can be parallelized (0 = outermost).
  /// -1 means no dimension is parallelizable.
  /// This allows ForLowering to parallelize only the outer loop even when
  /// inner loops have dependencies (e.g., Seidel-2D: parallelize i, sequential
  /// j).
  std::optional<int64_t> outermostParallelDim;

  ///===-------------------------------------------------------------===///
  /// Constructors
  ///===-------------------------------------------------------------===///
  explicit LoopMetadata(Operation *op) : ArtsMetadata(op) {}

  ///===-------------------------------------------------------------===///
  /// Interface
  ///===-------------------------------------------------------------===///
  StringRef getMetadataName() const override {
    return AttrNames::LoopMetadata::Name;
  }
  StringRef toString() const override { return locationMetadata.getKey(); }
  bool importFromOp() override;
  void exportToOp() override;
  void importFromJson(const llvm::json::Object &json) override;
  void exportToJson(llvm::json::Object &json) const override;

  ///===-------------------------------------------------------------===///
  /// Attribute Creation Helpers
  ///===-------------------------------------------------------------===///

  /// Create LoopMetadataAttr from current state using provided context.
  /// Use this when you need to create an attribute without modifying the
  /// associated operation.
  LoopMetadataAttr toAttribute(MLIRContext *ctx) const;

  /// Create metadata for loop after reduction handling via partial
  /// accumulators. Copies all fields from base, then sets:
  ///   - potentiallyParallel = true
  ///   - hasReductions = false
  ///   - hasInterIterationDeps = false
  ///   - memrefsWithLoopCarriedDeps = 0
  ///   - parallelClassification = Likely
  ///   - Clears reductionKinds
  static LoopMetadataAttr
  createParallelizedMetadata(MLIRContext *ctx, const LoopMetadataAttr &base);

private:
  Attribute getMetadataAttr() const override;
};

} // namespace arts
} // namespace mlir
#endif // ARTS_UTILS_LOOPMETADATA_H
