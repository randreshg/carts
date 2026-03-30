#ifndef CARTS_UTILS_LOWERINGCONTRACTUTILS_H
#define CARTS_UTILS_LOWERINGCONTRACTUTILS_H

#include "arts/Dialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
#include <optional>

namespace mlir {
namespace arts {

class AnalysisManager;

/// Discriminator for the high-level computational pattern a lowering contract
/// represents.  When the field is `Unknown` on a `LoweringContractInfo`,
/// `getEffectiveKind()` will attempt to derive the kind from `depPattern`.
enum class ContractKind : int64_t {
  Unknown = 0,
  Stencil = 1,
  Elementwise = 2,
  Matmul = 3,
  Triangular = 4
};

struct LoweringContractInfo {
  ContractKind kind = ContractKind::Unknown;
  std::optional<ArtsDepPattern> depPattern;
  std::optional<EdtDistributionKind> distributionKind;
  std::optional<EdtDistributionPattern> distributionPattern;
  std::optional<int64_t> distributionVersion;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<Value, 4> blockShape;
  SmallVector<Value, 4> minOffsets;
  SmallVector<Value, 4> maxOffsets;
  SmallVector<Value, 4> writeFootprint;
  SmallVector<int64_t, 4> staticBlockShape;
  SmallVector<int64_t, 4> staticMinOffsets;
  SmallVector<int64_t, 4> staticMaxOffsets;
  bool supportedBlockHalo = false;

  /// Spatial dimensions of the contract.
  SmallVector<int64_t, 4> spatialDims;

  /// Dimension-aware stencil analysis.
  SmallVector<int64_t, 4> stencilIndependentDims;

  /// Dependency-range narrowing remains advisory until a lowering pass with an
  /// exact window planner consumes it.
  bool narrowableDep = false;

  /// Post-DB refinement marker.
  bool postDbRefined = false;

  /// Critical path distance.
  std::optional<int64_t> criticalPathDistance;

  bool hasDistributionContract() const {
    return kind != ContractKind::Unknown || depPattern || distributionKind ||
           distributionPattern || distributionVersion;
  }

  bool hasSemanticContract() const {
    return hasDistributionContract() || narrowableDep;
  }

  bool hasSpatialContract() const {
    return !ownerDims.empty() || !blockShape.empty() || !minOffsets.empty() ||
           !maxOffsets.empty() || !writeFootprint.empty() ||
           !spatialDims.empty() || !staticBlockShape.empty() ||
           !staticMinOffsets.empty() || !staticMaxOffsets.empty() ||
           supportedBlockHalo || !stencilIndependentDims.empty();
  }

  bool hasAnalysisRefinements() const {
    return postDbRefined || criticalPathDistance;
  }

  bool empty() const {
    return !hasSemanticContract() && !hasSpatialContract() &&
           !hasAnalysisRefinements();
  }

  /// Resolve the effective ContractKind: returns `kind` when explicitly set,
  /// otherwise derives from `depPattern`.
  ContractKind getEffectiveKind() const;

  bool isStencilFamily() const {
    return getEffectiveKind() == ContractKind::Stencil;
  }

  bool hasOwnerDims() const { return !ownerDims.empty(); }
  bool hasExplicitStencilContract() const;
  bool usesStencilDistribution() const;
  bool supportsBlockHalo() const;
  std::optional<SmallVector<int64_t, 4>> getStaticBlockShape() const;
  std::optional<SmallVector<int64_t, 4>> getStaticMinOffsets() const;
  std::optional<SmallVector<int64_t, 4>> getStaticMaxOffsets() const;
};

struct AcquireRewriteContract {
  bool applyStencilHalo = false;
  bool preserveParentDepRange = false;
  bool usePartitionSliceAsDepWindow = false;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> haloMinOffsets;
  SmallVector<int64_t, 4> haloMaxOffsets;
};

enum class OperationContractTransferPolicy : uint8_t {
  /// Copy only explicitly stamped semantic contract attributes.
  PreserveStampedContractOnly = 0,
  /// Also project effective dep-pattern fallback when the destination does not
  /// have an explicit dep-pattern attribute.
  IncludeEffectiveDepPatternFallback = 1
};

LoweringContractOp getLoweringContractOp(Value target);
std::optional<LoweringContractInfo> getLoweringContract(Value target);
std::optional<LoweringContractInfo> getSemanticContract(Operation *op);
std::optional<LoweringContractInfo>
getLoweringContract(Operation *op, OpBuilder &builder, Location loc);
void normalizeLoweringContractInfo(LoweringContractInfo &info);
AcquireRewriteContract deriveAcquireRewriteContract(DbAcquireOp acquire);
AcquireRewriteContract resolveAcquireRewriteContract(AnalysisManager *AM,
                                                     DbAcquireOp acquire);
SmallVector<unsigned, 4>
resolveContractOwnerDims(const LoweringContractInfo &info, unsigned rank);
bool prefersContractNDBlock(const LoweringContractInfo &info,
                            unsigned requiredRank = 2);

LoweringContractOp upsertLoweringContract(OpBuilder &builder, Location loc,
                                          Value target,
                                          const LoweringContractInfo &info);
void transferOperationContract(
    Operation *source, Operation *target,
    OperationContractTransferPolicy policy =
        OperationContractTransferPolicy::PreserveStampedContractOnly);
void transferLoweringContract(Operation *source, Value target,
                              OpBuilder &builder, Location loc);
void transferValueContract(Value source, Value target, OpBuilder &builder,
                           Location loc);
void transferContract(
    Operation *sourceOp, Operation *targetOp, Value sourceContractTarget,
    Value targetContractTarget, OpBuilder &builder, Location loc,
    OperationContractTransferPolicy policy =
        OperationContractTransferPolicy::PreserveStampedContractOnly);

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_LOWERINGCONTRACTUTILS_H
