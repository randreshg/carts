#ifndef CARTS_DIALECT_ARTS_UTILS_LOWERINGFACTUTILS_H
#define CARTS_DIALECT_ARTS_UTILS_LOWERINGFACTUTILS_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
#include <optional>

namespace mlir {
namespace carts::arts {

/// Discriminator for the high-level computational pattern represented by
/// concrete lowering facts. When the field is `Unknown`, `getEffectiveKind()`
/// attempts to derive the kind from `depPattern`.
enum class FactMergeChange { Unchanged, Changed };

enum class FactKind : int64_t {
  Unknown = 0,
  Stencil = 1,
  Elementwise = 2,
  Matmul = 3,
  Triangular = 4
};

struct SemanticPattern {
  FactKind kind = FactKind::Unknown;
  std::optional<ArtsDepPattern> depPattern;
  std::optional<EdtDistributionKind> distributionKind;
  std::optional<EdtDistributionPattern> distributionPattern;
  std::optional<int64_t> distributionVersion;

  bool hasDistributionFacts() const {
    return kind != FactKind::Unknown || depPattern || distributionKind ||
           distributionPattern || distributionVersion;
  }
};

struct SpatialLayout {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<Value, 4> blockShape;
  SmallVector<Value, 4> minOffsets;
  SmallVector<Value, 4> maxOffsets;
  SmallVector<Value, 4> writeFootprint;
  SmallVector<int64_t, 4> staticBlockShape;
  SmallVector<int64_t, 4> staticMinOffsets;
  SmallVector<int64_t, 4> staticMaxOffsets;
  SmallVector<int64_t, 4> spatialDims;
  SmallVector<int64_t, 4> stencilIndependentDims;
  std::optional<int64_t> centerOffset;
  bool supportedBlockHalo = false;

  bool empty() const {
    return ownerDims.empty() && blockShape.empty() && minOffsets.empty() &&
           maxOffsets.empty() && writeFootprint.empty() &&
           spatialDims.empty() && staticBlockShape.empty() &&
           staticMinOffsets.empty() && staticMaxOffsets.empty() &&
           !supportedBlockHalo && stencilIndependentDims.empty() &&
           !centerOffset;
  }
};

struct AnalysisRefinement {
  bool narrowableDep = false;
  bool postDbRefined = false;

  bool empty() const { return !postDbRefined; }
};

struct LoweringFactInfo {
  SemanticPattern pattern;
  SpatialLayout spatial;
  AnalysisRefinement analysis;

  bool hasDistributionFacts() const { return pattern.hasDistributionFacts(); }

  bool hasSemanticFacts() const {
    return hasDistributionFacts() || analysis.narrowableDep;
  }

  bool hasSpatialFacts() const { return !spatial.empty(); }

  bool hasAnalysisRefinements() const { return !analysis.empty(); }

  bool empty() const {
    return !hasSemanticFacts() && !hasSpatialFacts() &&
           !hasAnalysisRefinements();
  }

  /// Resolve the effective FactKind: returns `kind` when explicitly set,
  /// otherwise derives from `depPattern`.
  FactKind getEffectiveKind() const;

  bool isStencilFamily() const {
    return getEffectiveKind() == FactKind::Stencil;
  }

  bool hasOwnerDims() const { return !spatial.ownerDims.empty(); }
  bool hasExplicitStencilFacts() const;
  bool usesStencilDistribution() const;
  bool supportsBlockHalo() const;
  std::optional<EdtDistributionPattern> getEffectiveDistributionPattern() const;
  bool isWavefrontFamily() const;
  bool prefersSemanticOwnerLayoutPreservation() const;
  bool isWavefrontStencilFacts() const;
  bool prefersNDBlock(unsigned requiredRank = 2) const;
  std::optional<SmallVector<int64_t, 4>> getStaticBlockShape() const;
  std::optional<SmallVector<int64_t, 4>> getStaticMinOffsets() const;
  std::optional<SmallVector<int64_t, 4>> getStaticMaxOffsets() const;
};

std::optional<LoweringFactInfo> getLoweringFacts(Value target);
std::optional<LoweringFactInfo> getSemanticFacts(Operation *op);
std::optional<LoweringFactInfo>
getLoweringFacts(Operation *op, OpBuilder &builder, Location loc);
FactMergeChange mergeLoweringFactInfo(LoweringFactInfo &dest,
                                      const LoweringFactInfo &src);
void normalizeLoweringFactInfo(LoweringFactInfo &info);
SmallVector<unsigned, 4> resolveFactOwnerDims(const LoweringFactInfo &info,
                                              unsigned rank);

/// Extract the halo window (min/max offsets) from concrete lowering facts.
/// Returns nullopt if there is no offset information.
std::optional<std::pair<SmallVector<int64_t, 4>, SmallVector<int64_t, 4>>>
projectHaloWindow(const LoweringFactInfo &facts);

/// Resolve effective lowering facts for an acquire operation by combining
/// pointer facts, allocation facts, and semantic annotations.
std::optional<LoweringFactInfo> resolveAcquireFacts(DbAcquireOp acquire);

/// Check if an acquire should apply stencil halo extension to worker-local
/// read slices. Returns true when mode=in and facts supports block halo.
bool shouldApplyStencilHalo(const LoweringFactInfo &facts,
                            ArtsMode effectiveMode);
bool shouldApplyStencilHalo(const LoweringFactInfo &facts, DbAcquireOp acquire);

/// Check if an acquire should use partition_offsets/partition_sizes as the
/// dependency window instead of offsets/sizes. Returns true for stencil-mode
/// read acquires with explicit stencil facts or wavefront inout acquires.
bool shouldUsePartitionSliceAsDepWindow(const LoweringFactInfo &facts,
                                        ArtsMode effectiveMode,
                                        PartitionMode partitionMode);
bool shouldUsePartitionSliceAsDepWindow(const LoweringFactInfo &facts,
                                        DbAcquireOp acquire);

/// Check if an acquire should preserve the parent dependency range instead
/// of using the worker-local partition slice. Returns true for read acquires
/// without explicit stencil facts and without narrowable_dep annotation.
bool shouldPreserveParentDepRange(const LoweringFactInfo &facts,
                                  ArtsMode effectiveMode);
bool shouldPreserveParentDepRange(const LoweringFactInfo &facts,
                                  DbAcquireOp acquire);

void transferOperationFacts(Operation *source, Operation *target);

/// Read physical DB block layout, preferring rank-expanded type recovery from
/// the committed DB grid + payload element sizes over legacy grid-slot facts.
std::optional<ArtsDbPhysicalLayout>
readArtsDbPhysicalLayoutFromCommittedType(DbAllocOp alloc);

} // namespace carts::arts
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_UTILS_LOWERINGFACTUTILS_H
