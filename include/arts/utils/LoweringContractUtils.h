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

struct LoweringContractInfo {
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
  bool supportedBlockHalo = false;

  bool empty() const {
    return !depPattern && !distributionKind && !distributionPattern &&
           !distributionVersion && ownerDims.empty() && spatialDims.empty() &&
           blockShape.empty() && minOffsets.empty() && maxOffsets.empty() &&
           writeFootprint.empty() && !supportedBlockHalo;
  }

  bool isStencilFamily() const;
  bool hasOwnerDims() const { return !ownerDims.empty(); }
  bool usesStencilDistribution() const;
  bool supportsBlockHalo() const;
  std::optional<SmallVector<int64_t, 4>> getStaticMinOffsets() const;
  std::optional<SmallVector<int64_t, 4>> getStaticMaxOffsets() const;
};

struct AcquireRewriteContract {
  bool applyStencilHalo = false;
  bool preserveParentDependencyRange = false;
  bool usePartitionSliceAsDependencyWindow = false;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> haloMinOffsets;
  SmallVector<int64_t, 4> haloMaxOffsets;
};

LoweringContractOp getLoweringContractOp(Value target);
std::optional<LoweringContractInfo> getLoweringContract(Value target);
std::optional<LoweringContractInfo> getSemanticContract(Operation *op);
std::optional<LoweringContractInfo>
getLoweringContract(Operation *op, OpBuilder &builder, Location loc);
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
void eraseLoweringContracts(Value target);

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_LOWERINGCONTRACTUTILS_H
