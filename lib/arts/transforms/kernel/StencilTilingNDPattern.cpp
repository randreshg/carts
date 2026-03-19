///==========================================================================///
/// StencilTilingNDPattern.cpp - Semantic N-D stencil contract stamping
///
/// Detects out-of-place affine stencil kernels and records an N-D halo-aware
/// block contract before DB creation so later passes do not fall back to
/// coarse or full-range when the full footprint is available.
///
/// Before:
///   arts.for (%i = %lb0 to %ub0) {
///     scf.for %j = %lb1 to %ub1 {
///       %x0 = memref.load %A[%i - 1, %j]
///       %x1 = memref.load %A[%i, %j]
///       %x2 = memref.load %A[%i + 1, %j]
///       memref.store %y, %B[%i, %j]
///     }
///   }
///
/// After:
///   arts.for (%i = %lb0 to %ub0) {
///     ...
///   } {arts.kernel_transform = #arts.kernel_transform<stencil_tiling_nd>,
///      arts.stencil_owner_dims = [0, 1],
///      arts.stencil_min_offsets = [-1, 0],
///      arts.stencil_max_offsets = [1, 0]}
///==========================================================================///

#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/transforms/kernel/KernelTransform.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Interfaces/CallInterfaces.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include <cstdlib>

ARTS_DEBUG_SETUP(kernel_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct IndexExpr {
  bool valid = false;
  bool invariant = false;
  unsigned ivOrdinal = 0;
  int64_t minOffset = 0;
  int64_t maxOffset = 0;
};

struct LinearExpr {
  bool valid = false;
  DenseMap<Value, int64_t> coeffs;
  int64_t constant = 0;
};

struct LogicalAccess {
  Value rootMemref;
  SmallVector<LinearExpr, 4> indices;
};

struct MatchResult {
  ForOp artsFor;
  EdtOp parentEdt;
  Value outputMemref;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> minOffsets;
  SmallVector<int64_t, 4> maxOffsets;
  SmallVector<int64_t, 4> writeFootprint;
  ArtsDepPattern pattern = ArtsDepPattern::unknown;
};

static bool isPureOp(Operation *op) {
  if (!op)
    return false;
  if (isa<arts::ForOp, arts::YieldOp, memref::LoadOp, memref::StoreOp,
          affine::AffineLoadOp, affine::AffineStoreOp, affine::AffineApplyOp,
          scf::ForOp, scf::YieldOp, affine::AffineForOp, affine::AffineYieldOp>(
          op))
    return true;
  if (isa<CallOpInterface>(op))
    return false;
  if (auto iface = dyn_cast<MemoryEffectOpInterface>(op))
    return iface.hasNoEffect();
  return op->getNumRegions() == 0;
}

static std::optional<std::pair<int64_t, int64_t>>
getStaticIvRange(scf::ForOp loop) {
  if (!loop)
    return std::nullopt;
  auto lb = ValueAnalysis::tryFoldConstantIndex(loop.getLowerBound());
  auto ub = ValueAnalysis::tryFoldConstantIndex(loop.getUpperBound());
  auto step = ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  if (!lb || !ub || !step || *step <= 0 || *ub <= *lb)
    return std::nullopt;

  int64_t minVal = *lb;
  int64_t maxVal = *ub - *step;
  return std::make_pair(minVal, maxVal);
}

static std::optional<std::pair<int64_t, int64_t>>
getStaticIvRange(affine::AffineForOp loop) {
  if (!loop || !loop.hasConstantLowerBound() || !loop.hasConstantUpperBound())
    return std::nullopt;
  int64_t lb = loop.getConstantLowerBound();
  int64_t ub = loop.getConstantUpperBound();
  int64_t step = loop.getStep();
  if (step <= 0 || ub <= lb)
    return std::nullopt;
  return std::make_pair(lb, ub - step);
}

static bool collectSpatialNestIvs(ForOp artsFor, SmallVector<Value, 4> &ivs,
                                  Block *&spatialBody) {
  if (!artsFor)
    return false;
  if (artsFor.getBody()->getNumArguments() == 0)
    return false;
  ivs.push_back(artsFor.getBody()->getArgument(0));

  Block *block = artsFor.getBody();
  while (block) {
    SmallVector<Operation *, 2> nestedFors;
    for (Operation &op : block->without_terminator()) {
      if (isa<scf::ForOp, affine::AffineForOp>(&op)) {
        nestedFors.push_back(&op);
        continue;
      }
      if (!isPureOp(&op))
        return false;
    }

    /// Allow index/scalar setup alongside the single nested spatial loop.
    /// Real kernels often materialize loop-local arithmetic before descending
    /// into the next loop, and treating that as a structural mismatch causes
    /// us to miss reusable stencil families like sw4lite/rhs4sg-base.
    if (nestedFors.size() != 1)
      break;

    Operation *nestedLoop = nestedFors.front();
    if (auto scfFor = dyn_cast<scf::ForOp>(nestedLoop)) {
      ivs.push_back(scfFor.getInductionVar());
      block = scfFor.getBody();
      continue;
    }
    auto affineFor = cast<affine::AffineForOp>(nestedLoop);
    ivs.push_back(affineFor.getInductionVar());
    block = affineFor.getBody();
  }

  spatialBody = block;
  return !ivs.empty() && spatialBody;
}

static bool accumulateAffineExprTerms(AffineExpr expr, ValueRange operands,
                                      unsigned numDims, int64_t scale,
                                      DenseMap<Value, int64_t> &coeffs,
                                      int64_t &constant);

static bool accumulateLinearTerms(Value value, int64_t scale,
                                  DenseMap<Value, int64_t> &coeffs,
                                  int64_t &constant) {
  value = ValueAnalysis::stripNumericCasts(value);
  if (!value)
    return false;

  if (auto constOp = value.getDefiningOp<arith::ConstantIndexOp>()) {
    constant += scale * constOp.value();
    return true;
  }

  if (auto affineApply = value.getDefiningOp<affine::AffineApplyOp>()) {
    AffineMap map = affineApply.getAffineMap();
    if (map.getNumResults() != 1)
      return false;
    return accumulateAffineExprTerms(map.getResult(0),
                                     affineApply.getOperands(),
                                     map.getNumDims(), scale, coeffs, constant);
  }

  if (auto add = value.getDefiningOp<arith::AddIOp>()) {
    return accumulateLinearTerms(add.getLhs(), scale, coeffs, constant) &&
           accumulateLinearTerms(add.getRhs(), scale, coeffs, constant);
  }

  if (auto sub = value.getDefiningOp<arith::SubIOp>()) {
    return accumulateLinearTerms(sub.getLhs(), scale, coeffs, constant) &&
           accumulateLinearTerms(sub.getRhs(), -scale, coeffs, constant);
  }

  coeffs[value] += scale;
  return true;
}

static bool accumulateAffineExprTerms(AffineExpr expr, ValueRange operands,
                                      unsigned numDims, int64_t scale,
                                      DenseMap<Value, int64_t> &coeffs,
                                      int64_t &constant) {
  if (!expr)
    return false;
  if (auto constExpr = dyn_cast<AffineConstantExpr>(expr)) {
    constant += scale * constExpr.getValue();
    return true;
  }
  if (auto dimExpr = dyn_cast<AffineDimExpr>(expr)) {
    if (dimExpr.getPosition() >= operands.size())
      return false;
    return accumulateLinearTerms(operands[dimExpr.getPosition()], scale, coeffs,
                                 constant);
  }
  if (auto symbolExpr = dyn_cast<AffineSymbolExpr>(expr)) {
    unsigned operandIndex = numDims + symbolExpr.getPosition();
    if (operandIndex >= operands.size())
      return false;
    return accumulateLinearTerms(operands[operandIndex], scale, coeffs,
                                 constant);
  }
  if (auto binary = dyn_cast<AffineBinaryOpExpr>(expr)) {
    if (binary.getKind() == AffineExprKind::Add) {
      return accumulateAffineExprTerms(binary.getLHS(), operands, numDims,
                                       scale, coeffs, constant) &&
             accumulateAffineExprTerms(binary.getRHS(), operands, numDims,
                                       scale, coeffs, constant);
    }
    if (binary.getKind() == AffineExprKind::Mul) {
      if (auto lhsConst = dyn_cast<AffineConstantExpr>(binary.getLHS()))
        return accumulateAffineExprTerms(binary.getRHS(), operands, numDims,
                                         scale * lhsConst.getValue(), coeffs,
                                         constant);
      if (auto rhsConst = dyn_cast<AffineConstantExpr>(binary.getRHS()))
        return accumulateAffineExprTerms(binary.getLHS(), operands, numDims,
                                         scale * rhsConst.getValue(), coeffs,
                                         constant);
      return false;
    }
    if (binary.getKind() == AffineExprKind::Mod ||
        binary.getKind() == AffineExprKind::FloorDiv ||
        binary.getKind() == AffineExprKind::CeilDiv)
      return false;
  }
  return false;
}

static DenseMap<Value, std::pair<int64_t, int64_t>>
collectReductionRanges(Block *root, ArrayRef<Value> spatialIvs) {
  DenseMap<Value, std::pair<int64_t, int64_t>> ranges;
  if (!root)
    return ranges;

  DenseSet<Value> ownerSet;
  for (Value iv : spatialIvs)
    ownerSet.insert(iv);
  root->walk([&](scf::ForOp loop) {
    Value iv = loop.getInductionVar();
    if (ownerSet.contains(iv))
      return;
    auto range = getStaticIvRange(loop);
    if (!range)
      return;
    int64_t tripCount = range->second - range->first + 1;
    if (tripCount <= 0 || tripCount > 16)
      return;
    ranges[iv] = *range;
  });
  root->walk([&](affine::AffineForOp loop) {
    Value iv = loop.getInductionVar();
    if (ownerSet.contains(iv))
      return;
    auto range = getStaticIvRange(loop);
    if (!range)
      return;
    int64_t tripCount = range->second - range->first + 1;
    if (tripCount <= 0 || tripCount > 16)
      return;
    ranges[iv] = *range;
  });
  return ranges;
}

static IndexExpr classifyStoreIndex(const LinearExpr &linear,
                                    ArrayRef<Value> ivs) {
  IndexExpr expr;
  if (!linear.valid)
    return expr;

  SmallVector<unsigned, 2> matchedIvs;
  for (unsigned i = 0; i < ivs.size(); ++i) {
    auto it = linear.coeffs.find(ivs[i]);
    if (it == linear.coeffs.end())
      continue;
    if (it->second != 1)
      return expr;
    matchedIvs.push_back(i);
  }

  if (matchedIvs.empty()) {
    expr.valid = true;
    expr.invariant = true;
    return expr;
  }
  if (matchedIvs.size() != 1 || linear.constant != 0 ||
      linear.coeffs.size() != 1)
    return expr;

  expr.valid = true;
  expr.ivOrdinal = matchedIvs.front();
  return expr;
}

static IndexExpr classifyLoadIndex(
    const LinearExpr &linear, ArrayRef<Value> spatialIvs,
    const DenseMap<Value, std::pair<int64_t, int64_t>> &reductionRanges) {
  IndexExpr expr;
  if (!linear.valid)
    return expr;

  SmallVector<unsigned, 2> matchedOwners;
  for (unsigned i = 0; i < spatialIvs.size(); ++i) {
    auto it = linear.coeffs.find(spatialIvs[i]);
    if (it == linear.coeffs.end())
      continue;
    if (it->second != 1)
      return expr;
    matchedOwners.push_back(i);
  }

  if (matchedOwners.empty()) {
    expr.valid = true;
    expr.invariant = true;
    return expr;
  }
  if (matchedOwners.size() != 1)
    return expr;

  int64_t minOffset = linear.constant;
  int64_t maxOffset = linear.constant;
  for (const auto &[value, coeff] : linear.coeffs) {
    if (llvm::is_contained(spatialIvs, value))
      continue;
    auto rangeIt = reductionRanges.find(value);
    if (rangeIt == reductionRanges.end())
      return expr;
    auto [minVal, maxVal] = rangeIt->second;
    int64_t scaledMin = coeff >= 0 ? coeff * minVal : coeff * maxVal;
    int64_t scaledMax = coeff >= 0 ? coeff * maxVal : coeff * minVal;
    minOffset += scaledMin;
    maxOffset += scaledMax;
  }

  expr.valid = true;
  expr.ivOrdinal = matchedOwners.front();
  expr.minOffset = minOffset;
  expr.maxOffset = maxOffset;
  return expr;
}

static bool buildLinearExpr(Value value, LinearExpr &expr) {
  expr = LinearExpr();
  DenseMap<Value, int64_t> coeffs;
  int64_t constant = 0;
  if (!accumulateLinearTerms(value, /*scale=*/1, coeffs, constant))
    return false;
  expr.valid = true;
  expr.coeffs = std::move(coeffs);
  expr.constant = constant;
  return true;
}

static bool buildAffineLinearExpr(AffineExpr affineExpr, AffineMap map,
                                  ValueRange operands, LinearExpr &expr) {
  expr = LinearExpr();
  DenseMap<Value, int64_t> coeffs;
  int64_t constant = 0;
  if (!accumulateAffineExprTerms(affineExpr, operands, map.getNumDims(),
                                 /*scale=*/1, coeffs, constant))
    return false;
  expr.valid = true;
  expr.coeffs = std::move(coeffs);
  expr.constant = constant;
  return true;
}

static bool buildLeafIndices(memref::LoadOp load,
                             SmallVectorImpl<LinearExpr> &indices) {
  indices.clear();
  indices.reserve(load.getIndices().size());
  for (Value idx : load.getIndices()) {
    LinearExpr linear;
    if (!buildLinearExpr(idx, linear))
      return false;
    indices.push_back(std::move(linear));
  }
  return true;
}

static bool buildLeafIndices(memref::StoreOp store,
                             SmallVectorImpl<LinearExpr> &indices) {
  indices.clear();
  indices.reserve(store.getIndices().size());
  for (Value idx : store.getIndices()) {
    LinearExpr linear;
    if (!buildLinearExpr(idx, linear))
      return false;
    indices.push_back(std::move(linear));
  }
  return true;
}

static bool buildLeafIndices(affine::AffineLoadOp load,
                             SmallVectorImpl<LinearExpr> &indices) {
  indices.clear();
  indices.reserve(load.getAffineMap().getNumResults());
  for (AffineExpr result : load.getAffineMap().getResults()) {
    LinearExpr linear;
    if (!buildAffineLinearExpr(result, load.getAffineMap(),
                               load.getMapOperands(), linear))
      return false;
    indices.push_back(std::move(linear));
  }
  return true;
}

static bool buildLeafIndices(affine::AffineStoreOp store,
                             SmallVectorImpl<LinearExpr> &indices) {
  indices.clear();
  indices.reserve(store.getAffineMap().getNumResults());
  for (AffineExpr result : store.getAffineMap().getResults()) {
    LinearExpr linear;
    if (!buildAffineLinearExpr(result, store.getAffineMap(),
                               store.getMapOperands(), linear))
      return false;
    indices.push_back(std::move(linear));
  }
  return true;
}

static Value stripMemrefCasts(Value value) {
  while (value) {
    if (auto cast = value.getDefiningOp<memref::CastOp>()) {
      value = cast.getSource();
      continue;
    }
    if (auto cast = value.getDefiningOp<UnrealizedConversionCastOp>()) {
      if (cast.getInputs().size() != 1)
        break;
      value = cast.getInputs().front();
      continue;
    }
    break;
  }
  return value;
}

template <typename LoadOpTy>
static bool prependContainerIndices(LoadOpTy containerLoad,
                                    SmallVectorImpl<LinearExpr> &indices) {
  if (!containerLoad)
    return false;
  Type resultTy = containerLoad.getResult().getType();
  if (!isa<BaseMemRefType>(resultTy))
    return false;

  SmallVector<LinearExpr, 4> prefix;
  if (!buildLeafIndices(containerLoad, prefix))
    return false;

  SmallVector<LinearExpr, 4> combined;
  combined.reserve(prefix.size() + indices.size());
  combined.append(prefix.begin(), prefix.end());
  combined.append(indices.begin(), indices.end());
  indices.assign(combined.begin(), combined.end());
  return true;
}

static bool resolveLogicalAccess(Value memref, ArrayRef<LinearExpr> leafIndices,
                                 LogicalAccess &access) {
  access = {};
  access.indices.append(leafIndices.begin(), leafIndices.end());
  Value current = stripMemrefCasts(memref);
  if (!current)
    return false;

  while (current) {
    if (auto load = current.getDefiningOp<memref::LoadOp>()) {
      if (!prependContainerIndices(load, access.indices))
        return false;
      current = stripMemrefCasts(load.getMemRef());
      continue;
    }
    if (auto load = current.getDefiningOp<affine::AffineLoadOp>()) {
      if (!prependContainerIndices(load, access.indices))
        return false;
      current = stripMemrefCasts(load.getMemRef());
      continue;
    }
    access.rootMemref = current;
    return access.rootMemref != nullptr;
  }

  return false;
}

static bool isOutOfPlaceStencil(ForOp artsFor, MatchResult &result) {
  SmallVector<Value, 4> spatialIvs;
  Block *spatialBody = nullptr;
  if (!collectSpatialNestIvs(artsFor, spatialIvs, spatialBody))
    return false;
  auto reductionRanges = collectReductionRanges(spatialBody, spatialIvs);

  SmallVector<LogicalAccess, 16> loads;
  SmallVector<LogicalAccess, 4> stores;
  bool invalid = false;
  spatialBody->walk([&](Operation *op) {
    if (invalid)
      return WalkResult::interrupt();
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (isa<BaseMemRefType>(load.getType()))
        return WalkResult::advance();
      SmallVector<LinearExpr, 4> leafIndices;
      LogicalAccess access;
      if (!buildLeafIndices(load, leafIndices) ||
          !resolveLogicalAccess(load.getMemRef(), leafIndices, access)) {
        invalid = true;
        return WalkResult::interrupt();
      }
      loads.push_back(std::move(access));
      return WalkResult::advance();
    }
    if (auto load = dyn_cast<affine::AffineLoadOp>(op)) {
      if (isa<BaseMemRefType>(load.getResult().getType()))
        return WalkResult::advance();
      SmallVector<LinearExpr, 4> leafIndices;
      LogicalAccess access;
      if (!buildLeafIndices(load, leafIndices) ||
          !resolveLogicalAccess(load.getMemRef(), leafIndices, access)) {
        invalid = true;
        return WalkResult::interrupt();
      }
      loads.push_back(std::move(access));
      return WalkResult::advance();
    }
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      SmallVector<LinearExpr, 4> leafIndices;
      LogicalAccess access;
      if (!buildLeafIndices(store, leafIndices) ||
          !resolveLogicalAccess(store.getMemRef(), leafIndices, access)) {
        invalid = true;
        return WalkResult::interrupt();
      }
      stores.push_back(std::move(access));
      return WalkResult::advance();
    }
    if (auto store = dyn_cast<affine::AffineStoreOp>(op)) {
      SmallVector<LinearExpr, 4> leafIndices;
      LogicalAccess access;
      if (!buildLeafIndices(store, leafIndices) ||
          !resolveLogicalAccess(store.getMemRef(), leafIndices, access)) {
        invalid = true;
        return WalkResult::interrupt();
      }
      stores.push_back(std::move(access));
      return WalkResult::advance();
    }
    if (!isPureOp(op)) {
      invalid = true;
      return WalkResult::interrupt();
    }
    if (isa<CallOpInterface>(op)) {
      invalid = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  if (invalid || loads.empty() || stores.empty())
    return false;

  DenseSet<Value> outputMemrefs;
  DenseMap<unsigned, int64_t> ivToOwnerDim;
  DenseMap<unsigned, unsigned> ivToOwnerAxis;
  SmallVector<int64_t, 4> ownerDims;

  for (const LogicalAccess &store : stores) {
    outputMemrefs.insert(store.rootMemref);
    for (auto [dim, idx] : llvm::enumerate(store.indices)) {
      IndexExpr expr = classifyStoreIndex(idx, spatialIvs);
      if (!expr.valid)
        return false;
      if (expr.invariant)
        continue;
      auto [it, inserted] = ivToOwnerDim.try_emplace(expr.ivOrdinal, dim);
      if (!inserted && it->second != static_cast<int64_t>(dim))
        return false;
      if (inserted) {
        ivToOwnerAxis[expr.ivOrdinal] = ownerDims.size();
        ownerDims.push_back(dim);
      }
    }
  }

  if (ownerDims.size() < 2)
    return false;

  SmallVector<int64_t, 4> minOffsets(ownerDims.size(), 0);
  SmallVector<int64_t, 4> maxOffsets(ownerDims.size(), 0);
  bool hasHalo = false;

  for (const LogicalAccess &load : loads) {
    const bool readsOutputValue = outputMemrefs.contains(load.rootMemref);
    if (readsOutputValue)
      return false;
    DenseSet<unsigned> seenOwnerIvs;
    for (auto [dim, idx] : llvm::enumerate(load.indices)) {
      IndexExpr expr = classifyLoadIndex(idx, spatialIvs, reductionRanges);
      if (!expr.valid)
        return false;
      if (expr.invariant)
        continue;
      auto ownerIt = ivToOwnerAxis.find(expr.ivOrdinal);
      if (ownerIt == ivToOwnerAxis.end())
        return false;

      /// Peer arrays may legally drop invariant dimensions, such as the
      /// leading component axis in sw4lite helper buffers. Require consistent
      /// owner-IV ordering across arrays, but not identical raw dim numbers.
      if (!seenOwnerIvs.insert(expr.ivOrdinal).second)
        return false;
      unsigned ownerIdx = ownerIt->second;
      minOffsets[ownerIdx] = std::min(minOffsets[ownerIdx], expr.minOffset);
      maxOffsets[ownerIdx] = std::max(maxOffsets[ownerIdx], expr.maxOffset);
      hasHalo = hasHalo || expr.minOffset != 0 || expr.maxOffset != 0;
    }
  }

  if (!hasHalo)
    return false;

  int64_t maxAbsOffset = 0;
  unsigned haloDims = 0;
  bool hasNegativeHalo = false;
  bool hasPositiveHalo = false;
  for (unsigned i = 0; i < ownerDims.size(); ++i) {
    maxAbsOffset = std::max<int64_t>(maxAbsOffset, std::abs(minOffsets[i]));
    maxAbsOffset = std::max<int64_t>(maxAbsOffset, std::abs(maxOffsets[i]));
    hasNegativeHalo = hasNegativeHalo || minOffsets[i] < 0;
    hasPositiveHalo = hasPositiveHalo || maxOffsets[i] > 0;
    if (minOffsets[i] != 0 || maxOffsets[i] != 0)
      haloDims++;
  }

  /// One-sided neighborhoods encode directional dependencies that cannot be
  /// represented by this out-of-place stencil contract.
  if (!hasNegativeHalo || !hasPositiveHalo)
    return false;

  ArtsDepPattern pattern = ArtsDepPattern::stencil_tiling_nd;
  if (maxAbsOffset > 1) {
    pattern = ArtsDepPattern::higher_order_stencil;
  } else if (ownerDims.size() >= 3 && haloDims >= 2) {
    pattern = ArtsDepPattern::cross_dim_stencil_3d;
  }

  result.artsFor = artsFor;
  result.parentEdt = artsFor->getParentOfType<EdtOp>();
  result.outputMemref = stores.front().rootMemref;
  result.ownerDims = ownerDims;
  result.minOffsets = minOffsets;
  result.maxOffsets = maxOffsets;
  result.writeFootprint.assign(ownerDims.size(), 0);
  result.pattern = pattern;
  return true;
}

class StencilTilingNDPattern final : public KernelPatternTransform {
public:
  bool match(ForOp artsFor) override {
    matchResult = MatchResult{};
    return isOutOfPlaceStencil(artsFor, matchResult);
  }

  LogicalResult apply(OpBuilder &builder) override {
    if (!matchResult.artsFor)
      return failure();

    auto stamp = [&](Operation *op) {
      if (!op)
        return;
      setDepPattern(op, matchResult.pattern);
      setEdtDistributionPattern(op, EdtDistributionPattern::stencil);
      setSupportedBlockHalo(op);
      setStencilSpatialDims(op, matchResult.ownerDims);
      setStencilOwnerDims(op, matchResult.ownerDims);
      setStencilMinOffsets(op, matchResult.minOffsets);
      setStencilMaxOffsets(op, matchResult.maxOffsets);
      setStencilWriteFootprint(op, matchResult.writeFootprint);
      if (int64_t artsId = getArtsId(matchResult.artsFor.getOperation());
          artsId > 0) {
        setStencilHaloContractId(op, artsId);
      }
    };

    stamp(matchResult.artsFor.getOperation());
    if (matchResult.parentEdt)
      stamp(matchResult.parentEdt.getOperation());
    return success();
  }

  StringRef getName() const override { return "StencilTilingNDPattern"; }
  ArtsDepPattern getFamily() const override {
    return ArtsDepPattern::stencil_tiling_nd;
  }
  int64_t getRevision() const override { return 1; }

private:
  MatchResult matchResult;
};

} // namespace

std::unique_ptr<KernelPatternTransform>
mlir::arts::createStencilTilingNDPattern() {
  return std::make_unique<StencilTilingNDPattern>();
}
