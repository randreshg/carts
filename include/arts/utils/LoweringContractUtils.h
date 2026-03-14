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

/// Discriminator for the high-level computational pattern a lowering contract
/// represents.  When the field is `Unknown` on a `LoweringContractInfo`,
/// `getEffectiveKind()` will attempt to derive the kind from `depPattern`.
enum class ContractKind : int64_t {
  Unknown = 0,
  Stencil = 1,
  Elementwise = 2,
  Matmul = 3,
  Triangular = 4,
  Replicated = 5
};

struct LoweringContractInfo {
  ContractKind kind = ContractKind::Unknown;
  std::optional<ArtsDepPattern> depPattern;
  std::optional<EdtDistributionKind> distributionKind;
  std::optional<EdtDistributionPattern> distributionPattern;
  std::optional<int64_t> distributionVersion;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> spatialDims;
  SmallVector<Value, 4> blockShape;
  SmallVector<Value, 4> minOffsets;
  SmallVector<Value, 4> maxOffsets;
  SmallVector<Value, 4> writeFootprint;
  SmallVector<int64_t, 4> staticBlockShape;
  SmallVector<int64_t, 4> staticMinOffsets;
  SmallVector<int64_t, 4> staticMaxOffsets;
  SmallVector<int64_t, 4> staticWriteFootprint;
  bool supportedBlockHalo = false;

  // Dimension-aware stencil analysis (EXT-PART-1)
  SmallVector<int64_t, 4> stencilIndependentDims;

  // ESD annotation (DT-3)
  std::optional<int64_t> esdByteOffset;
  std::optional<int64_t> esdByteSize;

  // Cached block window (DT-7)
  std::optional<int64_t> cachedStartBlock;
  std::optional<int64_t> cachedBlockCount;

  // Post-DB refinement marker (DT-1)
  bool postDbRefined = false;

  // Task cost estimate (ET-1)
  std::optional<int64_t> estimatedTaskCost;

  // Critical path distance (ET-6)
  std::optional<int64_t> criticalPathDistance;

  bool hasDistributionContract() const {
    return kind != ContractKind::Unknown || depPattern || distributionKind ||
           distributionPattern || distributionVersion;
  }

  bool hasSpatialContract() const {
    return !ownerDims.empty() || !spatialDims.empty() || !blockShape.empty() ||
           !minOffsets.empty() || !maxOffsets.empty() ||
           !writeFootprint.empty() || !staticBlockShape.empty() ||
           !staticMinOffsets.empty() || !staticMaxOffsets.empty() ||
           !staticWriteFootprint.empty() || supportedBlockHalo ||
           !stencilIndependentDims.empty();
  }

  bool hasRuntimeHints() const {
    return esdByteOffset || esdByteSize || cachedStartBlock || cachedBlockCount;
  }

  bool hasAnalysisRefinements() const {
    return postDbRefined || estimatedTaskCost ||
           criticalPathDistance;
  }

  bool empty() const {
    return !hasDistributionContract() && !hasSpatialContract() &&
           !hasRuntimeHints() && !hasAnalysisRefinements();
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

LoweringContractOp getLoweringContractOp(Value target);
std::optional<LoweringContractInfo> getLoweringContract(Value target);
std::optional<LoweringContractInfo> getSemanticContract(Operation *op);
std::optional<LoweringContractInfo>
getLoweringContract(Operation *op, OpBuilder &builder, Location loc);
void mergeLoweringContractInfo(LoweringContractInfo &dest,
                               const LoweringContractInfo &src);
void normalizeLoweringContractInfo(LoweringContractInfo &info);
AcquireRewriteContract deriveAcquireRewriteContract(DbAcquireOp acquire);
SmallVector<unsigned, 4>
resolveContractOwnerDims(const LoweringContractInfo &info, unsigned rank);
bool prefersContractNDBlock(const LoweringContractInfo &info,
                            unsigned requiredRank = 2);

LoweringContractOp upsertLoweringContract(OpBuilder &builder, Location loc,
                                          Value target,
                                          const LoweringContractInfo &info);
void copyLoweringContract(Value source, Value target, OpBuilder &builder,
                          Location loc);

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_LOWERINGCONTRACTUTILS_H
