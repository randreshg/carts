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

#include "arts/ArtsDialect.h"
#include "arts/Utils/Metadata/AccessStats.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
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
constexpr StringLiteral ReadCount = "read_count";
constexpr StringLiteral WriteCount = "write_count";
constexpr StringLiteral TripCount = "trip_count";
constexpr StringLiteral NestingLevel = "nesting_level";
constexpr StringLiteral HasUniformStride = "has_uniform_stride";
constexpr StringLiteral HasGatherScatter = "has_gather_scatter";
constexpr StringLiteral DataMovementPattern = "data_movement_pattern";
constexpr StringLiteral SuggestedPartitioning = "suggested_partitioning";
constexpr StringLiteral HasInterIterationDeps = "has_inter_iteration_deps";
constexpr StringLiteral MemrefsWithLoopCarriedDeps =
    "memrefs_with_loop_carried_deps";
constexpr StringLiteral ParallelClassification = "parallel_classification";
constexpr StringLiteral LocationKey = "location_key";
} // namespace LoopMetadata
} // namespace AttrNames

///===--------------------------------------------------------------------===///
/// LoopMetadata - Manages metadata for loop operations
///
/// This class provides an interface for querying and attaching metadata
/// to loop operations.
///===----------------------------------------------------------------------===///
class LoopMetadata : public ArtsMetadata {
public:
  //===-------------------------------------------------------------===//
  // Enums
  //===-------------------------------------------------------------===//
  /// Reduction operation kinds
  enum class ReductionKind { Add, Mul, Min, Max, And, Or, Xor, Unknown };
  const char *reductionKindToString(ReductionKind kind) const;
  ReductionKind stringToReductionKind(llvm::StringRef str) const;

  /// Data movement patterns for loops
  enum class DataMovement {
    Streaming,
    Tiled,
    Random,
    Stencil,
    Gather,
    Scatter,
    Unknown
  };
  const char *dataMovementToString(DataMovement pattern) const;
  DataMovement stringToDataMovement(llvm::StringRef str) const;

  /// Loop partitioning strategies
  enum class Partitioning { Block, Dynamic, Guided, Auto, Unknown };
  const char *partitioningToString(Partitioning strategy) const;
  Partitioning stringToPartitioning(llvm::StringRef str) const;

  /// Parallel classification for loops
  enum class ParallelClassification { Unknown, ReadOnly, Likely, Sequential };
  const char *
  parallelClassificationToString(ParallelClassification classification) const;
  ParallelClassification
  stringToParallelClassification(llvm::StringRef str) const;

  //===-------------------------------------------------------------===//
  // Attributes
  //===-------------------------------------------------------------===//
  /// Parallelism analysis
  bool potentiallyParallel = false, hasReductions = false;
  SmallVector<ReductionKind> reductionKinds;

  /// Memory access patterns
  AccessStats accessStats;

  /// Loop structure information
  std::optional<int64_t> tripCount, nestingLevel;

  /// Access pattern analysis
  std::optional<bool> hasUniformStride, hasGatherScatter;
  std::optional<DataMovement> dataMovementPattern;

  /// Partitioning hints
  std::optional<Partitioning> suggestedPartitioning;

  /// Dependency information
  std::optional<bool> hasInterIterationDeps;

  /// Aggregated memref usage information
  std::optional<int64_t> memrefsWithLoopCarriedDeps;
  std::optional<ParallelClassification> parallelClassification;

  /// Source Location
  LocationMetadata locationMetadata;

  //===-------------------------------------------------------------===//
  // Constructors
  //===-------------------------------------------------------------===//
  explicit LoopMetadata(Operation *op) : ArtsMetadata(op) {}

  //===-------------------------------------------------------------===//
  // Interface
  //===-------------------------------------------------------------===//
  StringRef getMetadataName() const override {
    return AttrNames::LoopMetadata::Name;
  }
  StringRef toString() const override { return locationMetadata.getKey(); }
  bool importFromOp() override;
  void exportToOp() override;
  void importFromJson(const llvm::json::Object &json) override;
  void exportToJson(llvm::json::Object &json) const override;

  //===-------------------------------------------------------------===//
  // Attribute Creation Helpers
  //===-------------------------------------------------------------===//

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
