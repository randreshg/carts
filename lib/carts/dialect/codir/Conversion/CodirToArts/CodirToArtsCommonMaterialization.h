///==========================================================================///
/// File: CodirToArtsCommonMaterialization.h
///
/// Common CODIR fact readers and block-local access rewrites.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_COMMONMATERIALIZATION_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_COMMONMATERIALIZATION_H

#include "CodirToArtsDbBackedMemref.h"
#include "carts/dialect/arts/Utils/DbLayoutPlanUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/codir/Utils/CodirConversionUtils.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <limits>
#include <optional>

namespace {

static inline arts::ArtsMode convertAccessMode(codir::CodirAccessMode mode) {
  switch (mode) {
  case codir::CodirAccessMode::read:
    return arts::ArtsMode::in;
  case codir::CodirAccessMode::write:
    return arts::ArtsMode::out;
  case codir::CodirAccessMode::readwrite:
    return arts::ArtsMode::inout;
  }
  return arts::ArtsMode::inout;
}

// Tier-1 enums (BarrierReason, ReductionStrategy, IterationTopology) have
// identical case sets and integer codes across SDE, CODIR, and ARTS
// (audit-verified). Call sites translate via static_cast guarded by these
// invariants.
static_assert(static_cast<int>(sde::SdeBarrierReason::unknown_required) ==
              static_cast<int>(arts::ArtsBarrierReason::unknown_required));
static_assert(
    static_cast<int>(codir::CodirReductionStrategy::local_accumulate) ==
    static_cast<int>(arts::ArtsReductionStrategy::local_accumulate));
static_assert(static_cast<int>(codir::CodirIterationTopology::owner_tile_2d) ==
              static_cast<int>(arts::ArtsPlanIterationTopology::owner_tile_2d));

static inline bool sameSortedI64Set(ArrayAttr lhs, ArrayAttr rhs) {
  std::optional<SmallVector<int64_t, 4>> lhsValues = readI64ArrayAttr(lhs);
  std::optional<SmallVector<int64_t, 4>> rhsValues = readI64ArrayAttr(rhs);
  if (!lhsValues || !rhsValues || lhsValues->size() != rhsValues->size())
    return false;
  llvm::sort(*lhsValues);
  llvm::sort(*rhsValues);
  return llvm::equal(*lhsValues, *rhsValues);
}

static inline ArrayAttr getCodirStencilOwnerDimsAttr(codir::CodeletOp codelet) {
  if (!codelet)
    return ArrayAttr{};
  ArrayAttr planOwnerDims = codelet.getPlanOwnerDimsAttr();
  ArrayAttr tileOwnerDims = codelet.getTileOwnerDimsAttr();
  if (!tileOwnerDims)
    return planOwnerDims;
  if (!planOwnerDims)
    return tileOwnerDims;
  if (sameSortedI64Set(planOwnerDims, tileOwnerDims))
    return tileOwnerDims;
  return planOwnerDims;
}

static inline bool hasCodirTileOwnerSlicePlan(codir::CodeletOp op) {
  return op && op.getTileShapeAttr() && op.getTileOwnerDimsAttr();
}

static inline std::optional<SmallVector<unsigned, 4>>
getCodirTileOwnerDims(codir::CodeletOp codelet) {
  if (!hasCodirTileOwnerSlicePlan(codelet))
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> rawDims =
      readI64ArrayAttr(codelet.getTileOwnerDimsAttr());
  if (!rawDims || rawDims->empty())
    return std::nullopt;

  SmallVector<unsigned, 4> dims;
  dims.reserve(rawDims->size());
  for (int64_t dim : *rawDims) {
    if (dim < 0)
      return std::nullopt;
    dims.push_back(static_cast<unsigned>(dim));
  }
  return dims;
}

static inline std::optional<unsigned>
getSingleCodirTileOwnerDim(codir::CodeletOp codelet) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirTileOwnerDims(codelet);
  if (!ownerDims || ownerDims->size() != 1)
    return std::nullopt;
  return ownerDims->front();
}

static inline SmallVector<Value, 4>
getCodirOwnerBaseArguments(codir::CodeletOp codelet, unsigned ownerDimCount) {
  SmallVector<Value, 4> bases;
  if (!codelet || codelet.getBody().empty() || ownerDimCount == 0 ||
      codelet.getParams().size() < ownerDimCount)
    return bases;

  Block &body = codelet.getBody().front();
  unsigned depCount = codelet.getDeps().size();
  unsigned paramCount = codelet.getParams().size();
  if (body.getNumArguments() < depCount + paramCount)
    return bases;

  bases.reserve(ownerDimCount);
  unsigned firstOwnerParam = paramCount - ownerDimCount;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot)
    bases.push_back(body.getArgument(depCount + firstOwnerParam + slot));
  return bases;
}

static inline SmallVector<Value, 4>
getCodirOwnerParamValues(codir::CodeletOp codelet, unsigned ownerDimCount) {
  SmallVector<Value, 4> params;
  if (!codelet || ownerDimCount == 0 ||
      codelet.getParams().size() < ownerDimCount)
    return params;

  params.reserve(ownerDimCount);
  unsigned firstOwnerParam = codelet.getParams().size() - ownerDimCount;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot)
    params.push_back(codelet.getParams()[firstOwnerParam + slot]);
  return params;
}

static inline std::optional<SmallVector<unsigned, 4>>
getCodirDepOwnerDims(codir::CodeletOp codelet, unsigned depIndex);

static inline SmallVector<Value, 4>
getCodirDepOwnerParamValues(codir::CodeletOp codelet, unsigned depIndex) {
  SmallVector<Value, 4> params;
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<unsigned, 4>> tileOwnerDims =
      getCodirTileOwnerDims(codelet);
  if (!ownerDims || !tileOwnerDims)
    return params;

  if (codelet && !codelet.getBody().empty() &&
      depIndex < codelet.getDeps().size()) {
    Block &body = codelet.getBody().front();
    unsigned depCount = codelet.getDeps().size();
    unsigned paramCount = codelet.getParams().size();
    if (depIndex < body.getNumArguments() &&
        body.getNumArguments() >= depCount + paramCount) {
      Value depArg = body.getArgument(depIndex);
      SmallVector<std::optional<unsigned>, 4> paramSlots(ownerDims->size());
      bool rejected = false;
      body.walk([&](Operation *op) {
        if (rejected)
          return WalkResult::interrupt();

        auto access = getCodirMemoryAccessInfo(op);
        if (!access || access->memref != depArg)
          return WalkResult::advance();

        for (auto [ownerSlot, ownerDim] : llvm::enumerate(*ownerDims)) {
          if (ownerDim >= access->indices.size())
            continue;
          std::optional<unsigned> selectedParam;
          for (unsigned paramSlot = 0; paramSlot < paramCount; ++paramSlot) {
            Value bodyParam = body.getArgument(depCount + paramSlot);
            if (!indexSelectsOwnerSlice(access->indices[ownerDim], bodyParam))
              continue;
            if (selectedParam && *selectedParam != paramSlot) {
              rejected = true;
              return WalkResult::interrupt();
            }
            selectedParam = paramSlot;
          }
          if (!selectedParam)
            continue;
          if (paramSlots[ownerSlot] &&
              *paramSlots[ownerSlot] != *selectedParam) {
            rejected = true;
            return WalkResult::interrupt();
          }
          paramSlots[ownerSlot] = *selectedParam;
        }
        return WalkResult::advance();
      });

      if (!rejected &&
          llvm::all_of(paramSlots, [](const std::optional<unsigned> &slot) {
            return slot.has_value();
          })) {
        params.reserve(ownerDims->size());
        for (std::optional<unsigned> slot : paramSlots)
          params.push_back(codelet.getParams()[*slot]);
        return params;
      }
    }
  }

  SmallVector<Value, 4> tileParams =
      getCodirOwnerParamValues(codelet, tileOwnerDims->size());
  if (tileParams.empty())
    return params;
  if (tileParams.size() == ownerDims->size())
    return tileParams;
  if (tileParams.size() != tileOwnerDims->size())
    return params;

  params.reserve(ownerDims->size());
  for (unsigned ownerDim : *ownerDims) {
    auto it = llvm::find(*tileOwnerDims, ownerDim);
    if (it == tileOwnerDims->end())
      return {};
    params.push_back(tileParams[std::distance(tileOwnerDims->begin(), it)]);
  }
  return params;
}

static inline std::optional<SmallVector<unsigned, 4>>
getPlannedCodirDepOwnerDims(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr depOwnerDims =
      codelet ? codelet.getDepOwnerDimsAttr() : ArrayAttr{};
  if (!depOwnerDims || depIndex >= depOwnerDims.size())
    return std::nullopt;

  auto dims = dyn_cast<ArrayAttr>(depOwnerDims[depIndex]);
  if (!dims)
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(dims);
  if (!values || values->empty())
    return std::nullopt;
  SmallVector<unsigned, 4> ownerDims;
  ownerDims.reserve(values->size());
  for (int64_t dim : *values) {
    if (dim < 0)
      return std::nullopt;
    ownerDims.push_back(static_cast<unsigned>(dim));
  }
  return ownerDims;
}

static inline std::optional<SmallVector<unsigned, 4>>
getCodirDepOwnerDims(codir::CodeletOp codelet, unsigned depIndex) {
  return getPlannedCodirDepOwnerDims(codelet, depIndex);
}

static inline ArrayAttr getCodirDepOwnerDimsAttr(codir::CodeletOp codelet,
                                                 unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims)
    return ArrayAttr{};
  SmallVector<int64_t, 4> values;
  values.reserve(ownerDims->size());
  for (unsigned dim : *ownerDims)
    values.push_back(dim);
  return buildI64ArrayAttr(codelet.getContext(), values);
}

static inline bool canUseCodirOwnerSliceForAlloc(codir::CodeletOp codelet,
                                                 unsigned depIndex,
                                                 arts::DbAllocOp alloc) {
  if (!hasCodirTileOwnerSlicePlan(codelet) || !alloc)
    return false;

  std::optional<arts::PartitionMode> mode =
      arts::getPartitionMode(alloc.getOperation());
  if (!mode || (*mode != arts::PartitionMode::block &&
                *mode != arts::PartitionMode::stencil))
    return false;

  ArrayAttr ownerDims = getCodirDepOwnerDimsAttr(codelet, depIndex);
  ArrayAttr blockShape = codir::getDepPhysicalBlockShapeAttr(codelet, depIndex);
  return ownerDims &&
         arts::getPlanOwnerDimsAttr(alloc.getOperation()) == ownerDims &&
         blockShape &&
         arts::getPlanPhysicalBlockShapeAttr(alloc.getOperation()) ==
             blockShape;
}

static inline std::optional<codir::CodirAccessMode>
getCodirDepAccessMode(codir::CodeletOp codelet, unsigned depIndex);

static inline bool codirAccessMayRead(codir::CodirAccessMode mode);

static inline bool codirAccessMayWrite(codir::CodirAccessMode mode);

static inline std::optional<SmallVector<int64_t, 4>>
getCodirTileOwnerBlockSizes(codir::CodeletOp codelet, unsigned depIndex,
                            unsigned memrefRank) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(codir::getDepPhysicalBlockShapeAttr(codelet, depIndex));
  if (!blockShape || blockShape->empty())
    return std::nullopt;

  SmallVector<int64_t, 4> blockSizes;
  blockSizes.reserve(ownerDims->size());
  for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
    std::optional<int64_t> blockSize;
    if (blockShape->size() == memrefRank) {
      if (ownerDim >= blockShape->size())
        return std::nullopt;
      blockSize = (*blockShape)[ownerDim];
    } else if (blockShape->size() == ownerDims->size()) {
      blockSize = (*blockShape)[slot];
    } else if (blockShape->size() == 1 && ownerDims->size() == 1) {
      blockSize = blockShape->front();
    }
    if (!blockSize || *blockSize <= 0)
      return std::nullopt;
    blockSizes.push_back(*blockSize);
  }
  return blockSizes;
}

static inline std::optional<int64_t>
getSingleCodirTileOwnerBlockSize(codir::CodeletOp codelet, unsigned depIndex,
                                 unsigned memrefRank) {
  std::optional<SmallVector<int64_t, 4>> blockSizes =
      getCodirTileOwnerBlockSizes(codelet, depIndex, memrefRank);
  if (!blockSizes || blockSizes->size() != 1)
    return std::nullopt;
  return blockSizes->front();
}

static inline scf::ForOp findCodirOwnerDispatchLoop(codir::CodeletOp codelet) {
  scf::ForOp nearestLoop;
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && !nearestLoop)
      nearestLoop = loop;
    if (loop && containsValue(codelet.getParams(), loop.getInductionVar()))
      return loop;
  }
  if (hasCodirTileOwnerSlicePlan(codelet))
    return nearestLoop;
  return {};
}

static inline scf::ForOp findCodirOwnerDispatchLoop(codir::CodeletOp codelet,
                                                    Value ownerParam) {
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && loop.getInductionVar() == ownerParam)
      return loop;
  }
  return {};
}

static inline Value getCodirOwnerDomainLower(codir::CodeletOp codelet) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet))
    return loop.getLowerBound();
  return {};
}

static inline bool dependsOnAncestorLoop(Value value,
                                         codir::CodeletOp codelet) {
  if (!value || !codelet)
    return false;
  for (Operation *parent = codelet->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop &&
        ::mlir::carts::ValueAnalysis::dependsOn(value, loop.getInductionVar()))
      return true;
  }
  return false;
}

static inline Value
deriveCodirOwnerDomainLowerFromParam(codir::CodeletOp codelet,
                                     Value ownerParam) {
  Value stripped = ::mlir::carts::ValueAnalysis::stripNumericCasts(ownerParam);
  auto add = stripped ? stripped.getDefiningOp<arith::AddIOp>() : nullptr;
  if (!add)
    return {};

  Value lhs = add.getLhs();
  Value rhs = add.getRhs();
  bool lhsDependsOnDispatch = dependsOnAncestorLoop(lhs, codelet);
  bool rhsDependsOnDispatch = dependsOnAncestorLoop(rhs, codelet);
  if (lhsDependsOnDispatch == rhsDependsOnDispatch)
    return {};
  return lhsDependsOnDispatch ? rhs : lhs;
}

static inline Value getCodirOwnerDomainLower(codir::CodeletOp codelet,
                                             Value ownerParam) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet, ownerParam))
    return loop.getLowerBound();
  if (Value lower = deriveCodirOwnerDomainLowerFromParam(codelet, ownerParam))
    return lower;
  return getCodirOwnerDomainLower(codelet);
}

static inline Value getCodirOwnerDomainUpper(codir::CodeletOp codelet) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet))
    return loop.getUpperBound();
  return {};
}

struct CodirOwnerHaloWindow {
  unsigned ownerDim = 0;
  int64_t lower = 0;
  int64_t upper = 0;

  bool empty() const { return lower <= 0 && upper <= 0; }
  int64_t width() const { return lower + upper; }
};

static inline std::optional<unsigned>
getCodirOwnerDimSlot(codir::CodeletOp codelet, unsigned ownerDim) {
  if (auto ownerDims = readI64ArrayAttr(codelet.getTileOwnerDimsAttr())) {
    for (auto [slot, rawDim] : llvm::enumerate(*ownerDims))
      if (rawDim >= 0 && static_cast<unsigned>(rawDim) == ownerDim)
        return static_cast<unsigned>(slot);
  }
  return std::nullopt;
}

static inline std::optional<int64_t>
getCodirOwnerDimValue(ArrayAttr attr, unsigned ownerDim,
                      std::optional<unsigned> ownerSlot, unsigned memrefRank) {
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(attr);
  if (!values || values->empty())
    return std::nullopt;
  if (values->size() == memrefRank && ownerDim < values->size())
    return (*values)[ownerDim];
  if (ownerSlot && *ownerSlot < values->size())
    return (*values)[*ownerSlot];
  if (values->size() == 1)
    return values->front();
  return std::nullopt;
}

static inline std::optional<unsigned>
getSingleCodirDepOwnerDim(codir::CodeletOp codelet, unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->size() != 1)
    return std::nullopt;
  return ownerDims->front();
}

static inline CodirOwnerHaloWindow
getCodirOwnerHaloWindowForDim(codir::CodeletOp codelet, unsigned depIndex,
                              unsigned ownerDim, unsigned memrefRank,
                              bool requireReadOnly = true) {
  CodirOwnerHaloWindow window;
  window.ownerDim = ownerDim;

  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (requireReadOnly &&
      (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode)))
    return window;
  if (ownerDim >= memrefRank)
    return window;

  std::optional<unsigned> ownerSlot = getCodirOwnerDimSlot(codelet, ownerDim);

  if (auto minOffset = getCodirOwnerDimValue(codelet.getAccessMinOffsetsAttr(),
                                             ownerDim, ownerSlot, memrefRank))
    window.lower = std::max<int64_t>(0, -*minOffset);
  if (auto maxOffset = getCodirOwnerDimValue(codelet.getAccessMaxOffsetsAttr(),
                                             ownerDim, ownerSlot, memrefRank))
    window.upper = std::max<int64_t>(0, *maxOffset);

  if (!window.empty())
    return window;

  if (auto halo = getCodirOwnerDimValue(codelet.getHaloShapeAttr(), ownerDim,
                                        ownerSlot, memrefRank)) {
    int64_t radius = std::max<int64_t>(0, *halo);
    window.lower = radius;
    window.upper = radius;
  }

  return window;
}

static inline SmallVector<CodirOwnerHaloWindow, 4>
getCodirOwnerHaloWindows(codir::CodeletOp codelet, unsigned depIndex,
                         unsigned memrefRank, bool requireReadOnly = true) {
  SmallVector<CodirOwnerHaloWindow, 4> windows;
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims)
    return windows;

  windows.reserve(ownerDims->size());
  for (unsigned ownerDim : *ownerDims)
    windows.push_back(getCodirOwnerHaloWindowForDim(
        codelet, depIndex, ownerDim, memrefRank, requireReadOnly));
  return windows;
}

static inline CodirOwnerHaloWindow
getCodirOwnerHaloWindow(codir::CodeletOp codelet, unsigned depIndex,
                        unsigned memrefRank) {
  std::optional<unsigned> ownerDim =
      getSingleCodirDepOwnerDim(codelet, depIndex);
  if (!ownerDim)
    return {};
  return getCodirOwnerHaloWindowForDim(codelet, depIndex, *ownerDim,
                                       memrefRank);
}

// Defined below (after findBackingDbAlloc); forward-declared so
// createDbBackedMemref can use the unioned per-buffer halo window.
static inline SmallVector<CodirOwnerHaloWindow, 4>
codirBackingBufferHaloWindows(Value rootMemref, unsigned memrefRank);

// Predicates defined later in this header; forward-declared so the union helper
// can replicate the exact block-vs-coarse materialization decision per read
// dep.
static inline bool
codirDepRequiresPhaseRedistributionBridge(codir::CodeletOp codelet,
                                          unsigned depIndex);
static inline bool codirDepUsesHaloStencilStorage(codir::CodeletOp codelet,
                                                  unsigned depIndex);
static inline bool
canMaterializeRawCodirDependencyWithPlan(Value root,
                                         codir::CodeletOp planSource);
static inline bool rawCodirDependencyNeedsHostBridge(Value root);

static inline Value subtractClampZero(OpBuilder &builder, Location loc,
                                      Value value, int64_t amount) {
  if (amount <= 0)
    return value;
  Value offset = createConstantIndex(builder, loc, amount);
  Value zero = createZeroIndex(builder, loc);
  Value canSubtract = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::uge, value, offset);
  Value shifted = arith::SubIOp::create(builder, loc, value, offset);
  return arith::SelectOp::create(builder, loc, canSubtract, shifted, zero);
}

static inline Value materializePositiveDifferenceOrZero(OpBuilder &builder,
                                                        Location loc, Value end,
                                                        Value start) {
  Value zero = createZeroIndex(builder, loc);
  Value hasPositiveExtent = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::ugt, end, start);
  Value difference = arith::SubIOp::create(builder, loc, end, start);
  return arith::SelectOp::create(builder, loc, hasPositiveExtent, difference,
                                 zero);
}

static inline Value materializeCodirOwnerDomainBase(OpBuilder &builder,
                                                    Location loc,
                                                    codir::CodeletOp codelet) {
  if (Value lower = getCodirOwnerDomainLower(codelet))
    return lower;
  return createZeroIndex(builder, loc);
}

static inline Value materializeCodirOwnerDomainBase(OpBuilder &builder,
                                                    Location loc,
                                                    codir::CodeletOp codelet,
                                                    Value ownerParam) {
  if (Value lower = getCodirOwnerDomainLower(codelet, ownerParam))
    return lower;
  return createZeroIndex(builder, loc);
}

static inline Value
materializeCodirBlockLocalBase(OpBuilder &builder, Location loc,
                               codir::CodeletOp codelet, unsigned depIndex,
                               Value ownerBase, unsigned memrefRank) {
  CodirOwnerHaloWindow halo =
      getCodirOwnerHaloWindow(codelet, depIndex, memrefRank);
  if (halo.empty())
    return ownerBase;
  return subtractClampZero(builder, loc, ownerBase, halo.lower);
}

static inline bool
codirDepAccessesStayWithinSingleOwnerSlice(codir::CodeletOp codelet,
                                           unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty() || !codelet || codelet.getBody().empty())
    return false;

  Block &body = codelet.getBody().front();
  if (depIndex >= codelet.getDeps().size() ||
      depIndex >= body.getNumArguments())
    return false;

  Value depArg = body.getArgument(depIndex);
  auto depType = dyn_cast<MemRefType>(depArg.getType());
  if (!depType || depType.getRank() == 0)
    return false;
  for (unsigned ownerDim : *ownerDims)
    if (ownerDim >= depType.getRank())
      return false;

  std::optional<SmallVector<unsigned, 4>> tileOwnerDims =
      getCodirTileOwnerDims(codelet);
  if (!tileOwnerDims)
    return false;
  SmallVector<Value, 4> ownerBases =
      getCodirOwnerBaseArguments(codelet, tileOwnerDims->size());
  if (ownerBases.size() != tileOwnerDims->size())
    return false;

  bool sawDepAccess = false;
  bool rejected = false;
  body.walk([&](Operation *op) {
    if (rejected)
      return WalkResult::interrupt();

    auto access = getCodirMemoryAccessInfo(op);
    if (!access)
      return WalkResult::advance();

    SmallVector<unsigned, 4> accessDims;
    bool sawRootedAccess = false;
    for (Value ownerBase : ownerBases) {
      CodirAccessOwnerDims traced = traceCodirAccessToRoot(
          access->memref, access->indices, depArg, ownerBase);
      if (traced.status == CodirAccessTraceStatus::NotRooted)
        continue;
      sawRootedAccess = true;
      if (traced.status == CodirAccessTraceStatus::Unsupported ||
          traced.ownerDims.size() > 1) {
        rejected = true;
        return WalkResult::interrupt();
      }
      if (!traced.ownerDims.empty())
        accessDims.push_back(traced.ownerDims.front());
    }
    if (!sawRootedAccess)
      return WalkResult::advance();
    sawDepAccess = true;
    if (accessDims != *ownerDims) {
      rejected = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });

  return sawDepAccess && !rejected;
}

struct PlannedBlockLocalAccessRewrite {
  Value localMemref;
  unsigned ownerDim = 0;
  Value ownerBase;
  Value localOrigin;
  int64_t lowerHalo = 0;
  Value groupedSourcePtr;
  unsigned ownerSlot = 0;
  int64_t blockSize = 1;
  int64_t groupBlockCount = 1;
  int64_t sourceDimExtent = ShapedType::kDynamic;
  bool grouped = false;
  bool allowFullWindowAccess = false;
};

static inline Value materializeBlockLocalOrigin(OpBuilder &builder,
                                                Location loc, Value ownerBase,
                                                Value ownerDomainBase,
                                                int64_t blockSize) {
  Value localOrigin = ownerBase;
  if (blockSize > 1) {
    if (!ownerDomainBase)
      ownerDomainBase = createZeroIndex(builder, loc);
    Value relativeBase =
        ::mlir::carts::ValueAnalysis::sameValue(ownerBase, ownerDomainBase)
            ? createZeroIndex(builder, loc)
            : arith::SubIOp::create(builder, loc, ownerBase, ownerDomainBase)
                  .getResult();
    Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
    Value blockIndex =
        arith::DivUIOp::create(builder, loc, relativeBase, blockSizeValue);
    Value blockOffset =
        arith::MulIOp::create(builder, loc, blockIndex, blockSizeValue);
    localOrigin =
        arith::AddIOp::create(builder, loc, ownerDomainBase, blockOffset);
  }
  return localOrigin;
}

static inline FailureOr<Value>
materializeBlockLocalIndex(OpBuilder &builder, Location loc, Value index,
                           Value ownerBase, Value localOrigin,
                           int64_t lowerHalo) {
  if (!index || !ownerBase || !localOrigin)
    return failure();
  if (lowerHalo > 0) {
    Value halo = createConstantIndex(builder, loc, lowerHalo);
    Value zero = createZeroIndex(builder, loc);
    Value canSubtract = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, localOrigin, halo);
    Value shifted = arith::SubIOp::create(builder, loc, localOrigin, halo);
    localOrigin =
        arith::SelectOp::create(builder, loc, canSubtract, shifted, zero);
  }
  if (::mlir::carts::ValueAnalysis::sameValue(index, localOrigin))
    return createZeroIndex(builder, loc);
  if (auto sub = index.getDefiningOp<arith::SubIOp>())
    if (::mlir::carts::ValueAnalysis::sameValue(sub.getRhs(), localOrigin))
      return index;
  if (!indexSelectsOwnerSlice(index, ownerBase))
    return failure();
  return arith::SubIOp::create(builder, loc, index, localOrigin).getResult();
}

static inline FailureOr<Value> materializeGroupedBlockLocalIndex(
    OpBuilder &builder, Location loc, Value index, Value ownerBase,
    int64_t lowerHalo, int64_t blockSize, int64_t groupBlockCount,
    int64_t sourceDimExtent, Value &relativeBlock,
    bool allowFullWindowAccess = false,
    const DenseMap<Value, Value> *sourceByBlockArgument = nullptr) {
  if (!index || !ownerBase || blockSize <= 0 || groupBlockCount <= 0)
    return failure();
  if (groupBlockCount > std::numeric_limits<int64_t>::max() / blockSize)
    return failure();
  if (!allowFullWindowAccess && !indexSelectsOwnerSlice(index, ownerBase))
    return failure();

  int64_t windowExtent = blockSize * groupBlockCount;
  struct WindowProof {
    Value ownerBase;
    int64_t windowExtent = 0;
    const DenseMap<Value, Value> *sourceByBlockArgument = nullptr;

    struct ConstantRange {
      int64_t lower = 0;
      int64_t upper = 0;
    };

    Value getSourceValue(Value candidate) const {
      if (!sourceByBlockArgument || !candidate)
        return {};
      auto it = sourceByBlockArgument->find(candidate);
      if (it == sourceByBlockArgument->end() || it->second == candidate)
        return {};
      return it->second;
    }

    static std::optional<int64_t> checkedAdd(int64_t lhs, int64_t rhs) {
      if (lhs < 0 || rhs < 0 || lhs > std::numeric_limits<int64_t>::max() - rhs)
        return std::nullopt;
      return lhs + rhs;
    }

    static std::optional<int64_t> checkedMul(int64_t lhs, int64_t rhs) {
      if (lhs < 0 || rhs < 0)
        return std::nullopt;
      if (lhs != 0 && rhs > std::numeric_limits<int64_t>::max() / lhs)
        return std::nullopt;
      return lhs * rhs;
    }

    static std::optional<int64_t> ceilDivNonNegative(int64_t lhs, int64_t rhs) {
      if (lhs < 0 || rhs <= 0)
        return std::nullopt;
      return llvm::divideCeil(lhs, rhs);
    }

    bool hasZeroOwnerBase() const {
      return ::mlir::carts::ValueAnalysis::isZeroConstant(ownerBase);
    }

    bool absoluteRangeStaysInWindow(Value candidate,
                                    bool allowEnd = false) const {
      if (!hasZeroOwnerBase())
        return false;
      std::optional<ConstantRange> range = getUnsignedRange(candidate);
      if (!range || range->lower < 0)
        return false;
      return allowEnd ? range->upper <= windowExtent
                      : range->upper < windowExtent;
    }

    std::optional<ConstantRange> getUnsignedRange(Value candidate,
                                                  unsigned depth = 0) const {
      if (!candidate || depth > 12)
        return std::nullopt;
      candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
      if (std::optional<int64_t> constant =
              ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(candidate)) {
        if (*constant < 0)
          return std::nullopt;
        return ConstantRange{*constant, *constant};
      }
      if (Value source = getSourceValue(candidate))
        if (std::optional<ConstantRange> range =
                getUnsignedRange(source, depth + 1))
          return range;

      if (auto blockArg = dyn_cast<BlockArgument>(candidate)) {
        auto loop =
            dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
        if (!loop || loop.getInductionVar() != candidate)
          return std::nullopt;
        if (!::mlir::carts::ValueAnalysis::isConstantAtLeastOne(loop.getStep()))
          return std::nullopt;
        std::optional<ConstantRange> lower =
            getUnsignedRange(loop.getLowerBound(), depth + 1);
        std::optional<ConstantRange> upper =
            getUnsignedRange(loop.getUpperBound(), depth + 1);
        if (!lower || !upper || upper->upper == 0)
          return std::nullopt;
        return ConstantRange{lower->lower, upper->upper - 1};
      }

      Operation *def = candidate.getDefiningOp();
      if (!def)
        return std::nullopt;
      auto rangeOfOperand = [&](Value operand) -> std::optional<ConstantRange> {
        return getUnsignedRange(operand, depth + 1);
      };

      if (auto add = dyn_cast<arith::AddIOp>(def)) {
        auto lhs = rangeOfOperand(add.getLhs());
        auto rhs = rangeOfOperand(add.getRhs());
        if (!lhs || !rhs)
          return std::nullopt;
        auto lower = checkedAdd(lhs->lower, rhs->lower);
        auto upper = checkedAdd(lhs->upper, rhs->upper);
        if (!lower || !upper)
          return std::nullopt;
        return ConstantRange{*lower, *upper};
      }
      if (auto sub = dyn_cast<arith::SubIOp>(def)) {
        auto lhs = rangeOfOperand(sub.getLhs());
        auto rhs = rangeOfOperand(sub.getRhs());
        if (!lhs || !rhs || lhs->lower < rhs->upper || lhs->upper < rhs->lower)
          return std::nullopt;
        return ConstantRange{lhs->lower - rhs->upper, lhs->upper - rhs->lower};
      }
      if (auto mul = dyn_cast<arith::MulIOp>(def)) {
        auto lhs = rangeOfOperand(mul.getLhs());
        auto rhs = rangeOfOperand(mul.getRhs());
        if (!lhs || !rhs)
          return std::nullopt;
        auto lower = checkedMul(lhs->lower, rhs->lower);
        auto upper = checkedMul(lhs->upper, rhs->upper);
        if (!lower || !upper)
          return std::nullopt;
        return ConstantRange{*lower, *upper};
      }
      if (auto div = dyn_cast<arith::DivUIOp>(def)) {
        auto lhs = rangeOfOperand(div.getLhs());
        auto rhs = rangeOfOperand(div.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        return ConstantRange{lhs->lower / rhs->upper, lhs->upper / rhs->lower};
      }
      if (auto div = dyn_cast<arith::DivSIOp>(def)) {
        auto lhs = rangeOfOperand(div.getLhs());
        auto rhs = rangeOfOperand(div.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        return ConstantRange{lhs->lower / rhs->upper, lhs->upper / rhs->lower};
      }
      if (auto div = dyn_cast<arith::CeilDivUIOp>(def)) {
        auto lhs = rangeOfOperand(div.getLhs());
        auto rhs = rangeOfOperand(div.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        auto lower = ceilDivNonNegative(lhs->lower, rhs->upper);
        auto upper = ceilDivNonNegative(lhs->upper, rhs->lower);
        if (!lower || !upper)
          return std::nullopt;
        return ConstantRange{*lower, *upper};
      }
      if (auto div = dyn_cast<arith::CeilDivSIOp>(def)) {
        auto lhs = rangeOfOperand(div.getLhs());
        auto rhs = rangeOfOperand(div.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        auto lower = ceilDivNonNegative(lhs->lower, rhs->upper);
        auto upper = ceilDivNonNegative(lhs->upper, rhs->lower);
        if (!lower || !upper)
          return std::nullopt;
        return ConstantRange{*lower, *upper};
      }
      if (auto rem = dyn_cast<arith::RemUIOp>(def)) {
        auto lhs = rangeOfOperand(rem.getLhs());
        auto rhs = rangeOfOperand(rem.getRhs());
        if (!lhs || !rhs || rhs->lower <= 0)
          return std::nullopt;
        return ConstantRange{0, std::min(lhs->upper, rhs->upper - 1)};
      }
      if (auto min = dyn_cast<arith::MinUIOp>(def)) {
        auto lhs = rangeOfOperand(min.getLhs());
        auto rhs = rangeOfOperand(min.getRhs());
        if (!lhs || !rhs)
          return std::nullopt;
        return ConstantRange{std::min(lhs->lower, rhs->lower),
                             std::min(lhs->upper, rhs->upper)};
      }
      if (auto max = dyn_cast<arith::MaxUIOp>(def)) {
        auto lhs = rangeOfOperand(max.getLhs());
        auto rhs = rangeOfOperand(max.getRhs());
        if (!lhs || !rhs)
          return std::nullopt;
        return ConstantRange{std::max(lhs->lower, rhs->lower),
                             std::max(lhs->upper, rhs->upper)};
      }
      if (auto select = dyn_cast<arith::SelectOp>(def)) {
        auto trueRange = rangeOfOperand(select.getTrueValue());
        auto falseRange = rangeOfOperand(select.getFalseValue());
        if (!trueRange || !falseRange)
          return std::nullopt;
        return ConstantRange{std::min(trueRange->lower, falseRange->lower),
                             std::max(trueRange->upper, falseRange->upper)};
      }
      return std::nullopt;
    }

    std::optional<int64_t> getOwnerRelativeConstant(Value candidate) const {
      std::optional<int64_t> candidateConst =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(candidate);
      std::optional<int64_t> ownerConst =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(ownerBase);
      if (candidateConst && ownerConst)
        return *candidateConst - *ownerConst;

      int64_t offset = 0;
      Value base =
          ::mlir::carts::ValueAnalysis::stripConstantOffset(candidate, &offset);
      if (::mlir::carts::ValueAnalysis::sameValue(base, ownerBase))
        return offset;
      return std::nullopt;
    }

    std::optional<std::pair<Value, int64_t>>
    splitAddConstant(Value candidate) const {
      candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
      if (auto add = candidate.getDefiningOp<arith::AddIOp>()) {
        if (std::optional<int64_t> rhs =
                ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
                    add.getRhs()))
          return std::make_pair(add.getLhs(), *rhs);
        if (std::optional<int64_t> lhs =
                ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
                    add.getLhs()))
          return std::make_pair(add.getRhs(), *lhs);
      }
      return std::nullopt;
    }

    bool pointStaysInWindow(Value candidate, unsigned depth = 0) const {
      if (!candidate || depth > 8)
        return false;
      if (absoluteRangeStaysInWindow(candidate))
        return true;
      std::optional<int64_t> offset = getOwnerRelativeConstant(candidate);
      if (offset)
        return *offset >= 0 && *offset < windowExtent;

      candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
      if (auto blockArg = dyn_cast<BlockArgument>(candidate)) {
        auto loop =
            dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
        if (!loop || loop.getInductionVar() != candidate)
          return false;
        if (!::mlir::carts::ValueAnalysis::isConstantAtLeastOne(loop.getStep()))
          return false;
        return pointStaysInWindow(loop.getLowerBound(), depth + 1) &&
               upperStaysInWindow(loop.getUpperBound(), depth + 1);
      }
      return false;
    }

    bool upperOffsetStaysInWindow(Value candidate) const {
      if (absoluteRangeStaysInWindow(candidate, /*allowEnd=*/true))
        return true;
      std::optional<int64_t> offset = getOwnerRelativeConstant(candidate);
      return offset && *offset >= 0 && *offset <= windowExtent;
    }

    bool pointPlusOffsetStaysInWindow(Value base, int64_t offset,
                                      unsigned depth) const {
      if (!base || offset < 0 || depth > 8)
        return false;
      if (hasZeroOwnerBase()) {
        std::optional<ConstantRange> range = getUnsignedRange(base);
        if (range && range->lower >= 0 && range->upper <= windowExtent - offset)
          return true;
      }
      if (std::optional<int64_t> baseOffset = getOwnerRelativeConstant(base))
        return *baseOffset >= 0 && *baseOffset + offset <= windowExtent;

      base = ::mlir::carts::ValueAnalysis::stripNumericCasts(base);
      auto blockArg = dyn_cast<BlockArgument>(base);
      if (!blockArg)
        return false;
      auto loop =
          dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
      if (!loop || loop.getInductionVar() != base)
        return false;
      std::optional<int64_t> step =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(loop.getStep());
      return step && *step > 0 && offset <= *step &&
             upperStaysInWindow(loop.getUpperBound(), depth + 1);
    }

    bool upperStaysInWindow(Value candidate, unsigned depth = 0) const {
      if (!candidate || depth > 8)
        return false;
      if (upperOffsetStaysInWindow(candidate))
        return true;
      if (auto add = splitAddConstant(candidate))
        if (pointPlusOffsetStaysInWindow(add->first, add->second, depth + 1))
          return true;
      candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
      if (auto min = candidate.getDefiningOp<arith::MinUIOp>())
        return upperStaysInWindow(min.getLhs(), depth + 1) ||
               upperStaysInWindow(min.getRhs(), depth + 1);
      if (auto min = candidate.getDefiningOp<arith::MinSIOp>())
        return upperStaysInWindow(min.getLhs(), depth + 1) ||
               upperStaysInWindow(min.getRhs(), depth + 1);
      return false;
    }
  };
  WindowProof proof{ownerBase, windowExtent, sourceByBlockArgument};
  auto pointStaysInWindow = [&](Value candidate) {
    return proof.pointStaysInWindow(candidate);
  };
  auto upperStaysInWindow = [&](Value candidate) {
    return proof.upperStaysInWindow(candidate);
  };

  bool provenInWindow = pointStaysInWindow(index);
  if (!provenInWindow) {
    if (auto blockArg = dyn_cast<BlockArgument>(index)) {
      auto loop =
          dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
      if (loop && loop.getInductionVar() == index &&
          ::mlir::carts::ValueAnalysis::isConstantAtLeastOne(loop.getStep())) {
        provenInWindow = pointStaysInWindow(loop.getLowerBound()) &&
                         upperStaysInWindow(loop.getUpperBound());
      }
    }
  }
  // Full-window read acquires every backing block. A static source extent that
  // fits in that window proves every in-bounds source index selects one of
  // them.
  if (!provenInWindow && allowFullWindowAccess && sourceDimExtent >= 0 &&
      sourceDimExtent <= windowExtent)
    provenInWindow = true;
  if (!provenInWindow)
    return failure();

  Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
  Value relativeIndex =
      ::mlir::carts::ValueAnalysis::sameValue(index, ownerBase)
          ? createZeroIndex(builder, loc)
          : arith::SubIOp::create(builder, loc, index, ownerBase).getResult();
  relativeBlock =
      arith::DivUIOp::create(builder, loc, relativeIndex, blockSizeValue);
  Value blockOffset =
      arith::MulIOp::create(builder, loc, relativeBlock, blockSizeValue);
  Value blockBase = arith::AddIOp::create(builder, loc, ownerBase, blockOffset);

  Value blockPayloadBase = blockBase;
  if (lowerHalo > 0)
    blockPayloadBase = subtractClampZero(builder, loc, blockBase, lowerHalo);

  if (::mlir::carts::ValueAnalysis::sameValue(index, blockPayloadBase))
    return createZeroIndex(builder, loc);
  return arith::SubIOp::create(builder, loc, index, blockPayloadBase)
      .getResult();
}

static inline LogicalResult rewritePlannedBlockLocalAccesses(
    arts::EdtOp task, ArrayRef<PlannedBlockLocalAccessRewrite> rewrites,
    const DenseMap<Value, Value> *sourceByBlockArgument = nullptr) {
  if (rewrites.empty())
    return success();

  auto rewriteIndices = [&](Operation *op, MutableOperandRange memrefOperand,
                            MutableOperandRange indices) -> WalkResult {
    Value memref = memrefOperand[0].get();
    SmallVector<const PlannedBlockLocalAccessRewrite *, 4> matching;
    for (const PlannedBlockLocalAccessRewrite &rewrite : rewrites)
      if (memref == rewrite.localMemref)
        matching.push_back(&rewrite);
    if (matching.empty())
      return WalkResult::advance();

    bool hasGrouped = llvm::any_of(
        matching, [](const PlannedBlockLocalAccessRewrite *rewrite) {
          return rewrite->grouped;
        });
    if (hasGrouped) {
      Value sourcePtr;
      unsigned sourceRank = 0;
      for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
        if (!rewrite->grouped || !rewrite->groupedSourcePtr ||
            rewrite->blockSize <= 0 || rewrite->groupBlockCount <= 0) {
          op->emitError("grouped planned block-local access requires "
                        "block-window facts for every owner dimension");
          return WalkResult::interrupt();
        }
        if (!sourcePtr)
          sourcePtr = rewrite->groupedSourcePtr;
        if (sourcePtr != rewrite->groupedSourcePtr) {
          op->emitError("grouped planned block-local access mixes dependency "
                        "sources");
          return WalkResult::interrupt();
        }
        sourceRank = std::max<unsigned>(sourceRank, rewrite->ownerSlot + 1);
      }
      if (auto sourceType = dyn_cast<MemRefType>(sourcePtr.getType()))
        sourceRank = std::max<unsigned>(sourceRank, sourceType.getRank());
      if (sourceRank == 0)
        sourceRank = 1;

      OpBuilder builder(op);
      SmallVector<Value, 4> dbRefIndices(
          sourceRank, createZeroIndex(builder, op->getLoc()));
      for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
        if (indices.size() <= rewrite->ownerDim ||
            rewrite->ownerSlot >= dbRefIndices.size()) {
          op->emitError("grouped planned block-local access has malformed "
                        "owner-dimension facts");
          return WalkResult::interrupt();
        }
        Value relativeBlock;
        FailureOr<Value> localIndex = materializeGroupedBlockLocalIndex(
            builder, op->getLoc(), indices[rewrite->ownerDim].get(),
            rewrite->ownerBase, rewrite->lowerHalo, rewrite->blockSize,
            rewrite->groupBlockCount, rewrite->sourceDimExtent, relativeBlock,
            rewrite->allowFullWindowAccess, sourceByBlockArgument);
        if (failed(localIndex)) {
          op->emitError("grouped planned block-local access does not stay "
                        "within the block window");
          return WalkResult::interrupt();
        }
        dbRefIndices[rewrite->ownerSlot] = relativeBlock;
        indices[rewrite->ownerDim].set(*localIndex);
      }
      Value selectedPayload =
          arts::DbRefOp::create(builder, op->getLoc(), sourcePtr, dbRefIndices);
      memrefOperand.assign(ValueRange{selectedPayload});
      return WalkResult::advance();
    }

    for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
      if (indices.size() <= rewrite->ownerDim) {
        op->emitError("planned block-local access is missing the owner "
                      "dimension index");
        return WalkResult::interrupt();
      }

      OpBuilder builder(op);
      FailureOr<Value> localIndex = materializeBlockLocalIndex(
          builder, op->getLoc(), indices[rewrite->ownerDim].get(),
          rewrite->ownerBase, rewrite->localOrigin, rewrite->lowerHalo);
      if (failed(localIndex)) {
        op->emitError("planned block-local access does not stay within the "
                      "owner slice");
        return WalkResult::interrupt();
      }
      indices[rewrite->ownerDim].set(*localIndex);
    }
    return WalkResult::advance();
  };

  Block &body = task.getBody().front();
  WalkResult result = body.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteIndices(op, load.getMemrefMutable(),
                            load.getIndicesMutable());
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteIndices(op, store.getMemrefMutable(),
                            store.getIndicesMutable());
    return WalkResult::advance();
  });

  return result.wasInterrupted() ? failure() : success();
}

static inline LogicalResult
createDbBackedMemref(OpBuilder &builder, Location loc, MemRefType memrefType,
                     ValueRange dynamicSizes, Value &memref,
                     codir::CodeletOp planSource,
                     std::optional<unsigned> depIndex = std::nullopt) {
  if (!hasCodirTileOwnerSlicePlan(planSource))
    return createDbBackedMemref(builder, loc, memrefType, dynamicSizes, memref,
                                sde::SdeSuIterateOp{});

  FailureOr<SmallVector<Value>> elementSizes =
      buildElementSizes(builder, loc, memrefType, dynamicSizes);
  if (failed(elementSizes))
    return failure();

  SmallVector<Value> dbElementSizes = std::move(*elementSizes);
  ArrayAttr ownerDims = depIndex
                            ? getCodirDepOwnerDimsAttr(planSource, *depIndex)
                            : planSource.getTileOwnerDimsAttr();
  ArrayAttr blockShape =
      depIndex ? codir::getDepPhysicalBlockShapeAttr(planSource, *depIndex)
               : planSource.getTileShapeAttr();
  if (!ownerDims)
    return failure();
  FailureOr<arts::DbPhysicalLayoutPlan> physicalPlan =
      arts::resolvePhysicalDbLayoutPlan(ownerDims, blockShape, dbElementSizes,
                                        builder, loc);
  if (failed(physicalPlan))
    return failure();

  SmallVector<CodirOwnerHaloWindow, 4> ownerHalos;
  if (depIndex) {
    Value backingRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
        planSource.getDeps()[*depIndex]);
    ownerHalos = codirBackingBufferHaloWindows(
        backingRoot, static_cast<unsigned>(memrefType.getRank()));
    for (const CodirOwnerHaloWindow &ownerHalo : ownerHalos) {
      if (ownerHalo.empty() ||
          ownerHalo.ownerDim >= physicalPlan->innerSizes.size())
        continue;
      Value haloWidth = createConstantIndex(builder, loc, ownerHalo.width());
      physicalPlan->innerSizes[ownerHalo.ownerDim] = arith::AddIOp::create(
          builder, loc, physicalPlan->innerSizes[ownerHalo.ownerDim],
          haloWidth);
    }
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto dbAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::heap,
      arts::DbMode::write, memrefType.getElementType(),
      SmallVector<Value>(physicalPlan->outerSizes.begin(),
                         physicalPlan->outerSizes.end()),
      SmallVector<Value>(physicalPlan->innerSizes.begin(),
                         physicalPlan->innerSizes.end()),
      physicalPlan->mode);
  arts::setPlanOwnerDimsAttr(dbAlloc.getOperation(), ownerDims);
  if (blockShape)
    arts::setPlanPhysicalBlockShapeAttr(dbAlloc.getOperation(), blockShape);
  if (auto haloShape = planSource.getHaloShapeAttr())
    arts::setPlanHaloShapeAttr(dbAlloc.getOperation(), haloShape);
  if (llvm::any_of(ownerHalos, [](const CodirOwnerHaloWindow &halo) {
        return !halo.empty();
      }))
    dbAlloc->setAttr(dbAlloc.getStencilSupportedBlockHaloAttrName(),
                     UnitAttr::get(dbAlloc.getContext()));

  memref = materializeInnerPayload(builder, loc, dbAlloc.getPtr());
  return success();
}

static inline arts::DbAllocOp findBackingDbAlloc(Value storage) {
  return dyn_cast_or_null<arts::DbAllocOp>(arts::DbUtils::getUnderlyingDbAlloc(
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storage)));
}

static inline std::optional<unsigned>
findCodirDependencyIndexForRoot(codir::CodeletOp codelet, Value root) {
  for (auto [idx, dep] : llvm::enumerate(codelet.getDeps()))
    if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) == root)
      return static_cast<unsigned>(idx);
  return std::nullopt;
}

static inline std::optional<codir::CodirStorageViewKind>
getCodirDepStorageViewKind(codir::CodeletOp codelet, unsigned depIndex);
static inline std::optional<codir::CodirAccessMode>
getCodirDepAccessMode(codir::CodeletOp codelet, unsigned depIndex);
static inline std::optional<codir::CodirCollectiveKind>
getCodirDepCollectiveKind(codir::CodeletOp codelet, unsigned depIndex);
static inline codir::CodirCollectiveKind
getFinalizedCodirDepCollectiveKind(codir::CodeletOp codelet, unsigned depIndex);
static inline bool codirDepRequiresComputeBlockStorage(codir::CodeletOp codelet,
                                                       unsigned depIndex);
static inline bool codirAccessMayRead(codir::CodirAccessMode mode);
static inline bool codirAccessMayWrite(codir::CodirAccessMode mode);

// DepStorageAssignment stamps collective selection onto `dep_collectives`; this
// file reads that carrier instead of re-running CODIR predicates.

static inline void
mergeCodirOwnerHaloWindow(SmallVectorImpl<CodirOwnerHaloWindow> &windows,
                          CodirOwnerHaloWindow incoming) {
  if (incoming.empty())
    return;
  for (CodirOwnerHaloWindow &window : windows) {
    if (window.ownerDim != incoming.ownerDim)
      continue;
    window.lower = std::max(window.lower, incoming.lower);
    window.upper = std::max(window.upper, incoming.upper);
    return;
  }
  windows.push_back(incoming);
}

static inline SmallVector<CodirOwnerHaloWindow, 4>
codirBackingBufferHaloWindows(Value rootMemref, unsigned memrefRank) {
  SmallVector<CodirOwnerHaloWindow, 4> unionWindows;
  if (!rootMemref)
    return unionWindows;

  Operation *defining = rootMemref.getDefiningOp();
  Operation *scope =
      defining ? defining : rootMemref.getParentBlock()->getParentOp();
  ModuleOp module = scope ? scope->getParentOfType<ModuleOp>() : ModuleOp{};
  if (!module)
    return unionWindows;

  module.walk([&](codir::CodeletOp codelet) {
    for (auto [idx, dep] : llvm::enumerate(codelet.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != rootMemref)
        continue;
      unsigned depIdx = static_cast<unsigned>(idx);
      if (!codirDepUsesHaloStencilStorage(codelet, depIdx))
        continue;
      if (!canMaterializeRawCodirDependencyWithPlan(rootMemref, codelet))
        continue;
      if (rawCodirDependencyNeedsHostBridge(rootMemref) &&
          !codirDepRequiresComputeBlockStorage(codelet, depIdx))
        continue;
      for (CodirOwnerHaloWindow window :
           getCodirOwnerHaloWindows(codelet, depIdx, memrefRank,
                                    /*requireReadOnly=*/false))
        mergeCodirOwnerHaloWindow(unionWindows, window);
    }
  });

  return unionWindows;
}

static inline SmallVector<CodirOwnerHaloWindow, 4>
getCodirBlockStorageHaloWindows(codir::CodeletOp codelet, unsigned depIndex,
                                unsigned memrefRank) {
  SmallVector<CodirOwnerHaloWindow, 4> windows;
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!codelet || depIndex >= codelet.getDeps().size() || !ownerDims)
    return windows;

  SmallVector<CodirOwnerHaloWindow, 4> storageWindows =
      codirBackingBufferHaloWindows(
          ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
              codelet.getDeps()[depIndex]),
          memrefRank);
  windows.reserve(ownerDims->size());
  for (unsigned ownerDim : *ownerDims) {
    CodirOwnerHaloWindow resolved;
    resolved.ownerDim = ownerDim;
    for (const CodirOwnerHaloWindow &candidate : storageWindows) {
      if (candidate.ownerDim == ownerDim) {
        resolved = candidate;
        break;
      }
    }
    windows.push_back(resolved);
  }
  return windows;
}

static inline CodirOwnerHaloWindow
getCodirBlockStorageHaloWindowForDim(codir::CodeletOp codelet,
                                     unsigned depIndex, unsigned ownerDim,
                                     unsigned memrefRank) {
  for (const CodirOwnerHaloWindow &window :
       getCodirBlockStorageHaloWindows(codelet, depIndex, memrefRank)) {
    if (window.ownerDim == ownerDim)
      return window;
  }
  CodirOwnerHaloWindow empty;
  empty.ownerDim = ownerDim;
  return empty;
}

// Use the DB allocation padding as the storage-halo authority when read/write
// dependency values are split before CODIR.
static inline CodirOwnerHaloWindow
blockAllocStorageHaloForDim(arts::DbAllocOp blockAlloc, unsigned ownerDim) {
  CodirOwnerHaloWindow window;
  window.ownerDim = ownerDim;
  if (!blockAlloc)
    return window;
  Operation *op = blockAlloc.getOperation();
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(arts::getPlanOwnerDimsAttr(op));
  std::optional<SmallVector<int64_t, 4>> haloShape =
      readI64ArrayAttr(arts::getPlanHaloShapeAttr(op));
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(arts::getPlanPhysicalBlockShapeAttr(op));
  if (!ownerDims || !haloShape || !blockShape)
    return window;
  int slot = -1;
  for (auto [i, d] : llvm::enumerate(*ownerDims))
    if (d >= 0 && static_cast<unsigned>(d) == ownerDim) {
      slot = static_cast<int>(i);
      break;
    }
  if (slot < 0 || static_cast<size_t>(slot) >= haloShape->size() ||
      static_cast<size_t>(slot) >= blockShape->size())
    return window;
  int64_t halo = (*haloShape)[slot];
  int64_t block = (*blockShape)[slot];
  if (halo <= 0)
    return window;
  ValueRange elementSizes = blockAlloc.getElementSizes();
  if (ownerDim >= elementSizes.size())
    return window;
  std::optional<int64_t> elem =
      ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
          elementSizes[ownerDim]);
  if (!elem || *elem < block + 2 * halo)
    return window;
  window.lower = halo;
  window.upper = halo;
  return window;
}

static inline std::optional<codir::CodirStorageViewKind>
getCodirDepStorageViewKind(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr views = codelet ? codelet.getDepStorageViewsAttr() : ArrayAttr{};
  if (!views || depIndex >= views.size())
    return std::nullopt;
  auto view = dyn_cast<codir::CodirStorageViewKindAttr>(views[depIndex]);
  if (!view)
    return std::nullopt;
  return view.getValue();
}

/// First-class collective family for |codelet|'s |depIndex|. Absence means the
/// CODIR storage/collective facts have not been finalized.
static inline std::optional<codir::CodirCollectiveKind>
getCodirDepCollectiveKind(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr collectives =
      codelet ? codelet.getDepCollectivesAttr() : ArrayAttr{};
  if (!collectives || depIndex >= collectives.size())
    return std::nullopt;
  auto kind = dyn_cast<codir::CodirCollectiveKindAttr>(collectives[depIndex]);
  if (!kind)
    return std::nullopt;
  return kind.getValue();
}

static inline codir::CodirCollectiveKind
getFinalizedCodirDepCollectiveKind(codir::CodeletOp codelet,
                                   unsigned depIndex) {
  return getCodirDepCollectiveKind(codelet, depIndex)
      .value_or(codir::CodirCollectiveKind::none);
}

static inline std::optional<codir::CodirAccessMode>
getCodirDepAccessMode(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr modes = codelet ? codelet.getDepModesAttr() : ArrayAttr{};
  if (!modes || depIndex >= modes.size())
    return std::nullopt;
  auto mode = dyn_cast<codir::CodirAccessModeAttr>(modes[depIndex]);
  if (!mode)
    return std::nullopt;
  return mode.getValue();
}

static inline bool
codirStorageViewUsesComputeBlock(codir::CodirStorageViewKind view) {
  return view == codir::CodirStorageViewKind::compute_block ||
         view == codir::CodirStorageViewKind::phase_redistributed;
}

static inline bool codirDepRequiresPlannedOwnerDims(codir::CodeletOp codelet,
                                                    unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  if (view && codirStorageViewUsesComputeBlock(*view))
    return true;

  std::optional<codir::CodirCollectiveKind> collective =
      getCodirDepCollectiveKind(codelet, depIndex);
  if (!collective)
    return false;
  switch (*collective) {
  case codir::CodirCollectiveKind::all_gather:
  case codir::CodirCollectiveKind::reduce_scatter:
  case codir::CodirCollectiveKind::halo:
    return true;
  case codir::CodirCollectiveKind::none:
  case codir::CodirCollectiveKind::all_to_all:
  case codir::CodirCollectiveKind::allreduce:
  case codir::CodirCollectiveKind::broadcast:
    return false;
  }
  return false;
}

static inline LogicalResult
requireFinalizedCodirDepOwnerDimsForMaterialization(codir::CodeletOp codelet,
                                                    unsigned depIndex) {
  if (!codirDepRequiresPlannedOwnerDims(codelet, depIndex))
    return success();
  if (getCodirDepOwnerDims(codelet, depIndex))
    return success();
  return codelet.emitOpError()
         << "dependency #" << depIndex
         << " requires finalized non-empty dep_owner_dims for "
            "block/stencil/compute materialization";
}

static inline bool codirDepAllowsComputeBlockStorage(codir::CodeletOp codelet,
                                                     unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  return view && codirStorageViewUsesComputeBlock(*view);
}

// An iterative-stencil dep carrying a committed `halo` collective legitimately
// reads neighbor tiles OUTSIDE its owner slice; the per-block halo exchange
// supplies those neighbors, so it qualifies for block-native distributed
// storage even though its accesses cross the owner slice. This is the ARTS dual
// of DepStorageAssignment's stencil-halo compute_block demotion: without it
// ARTS re-derives single-owner-slice containment, rejects the committed
// compute_block+halo plan, and coarse-falls-back the stencil buffer to a single
// local_only block (correct-but-not-distributed).
static inline bool codirDepIsHaloStencilStorage(codir::CodeletOp codelet,
                                                unsigned depIndex) {
  return getFinalizedCodirDepCollectiveKind(codelet, depIndex) ==
         codir::CodirCollectiveKind::halo;
}

static inline bool codirDepRequiresComputeBlockStorage(codir::CodeletOp codelet,
                                                       unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  return view && codirStorageViewUsesComputeBlock(*view);
}

static inline bool
codirDepRequiresPhaseRedistributionBridge(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  return view && *view == codir::CodirStorageViewKind::phase_redistributed;
}

static inline bool codirDepHasHaloWindow(codir::CodeletOp codelet,
                                         unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  auto memrefType = dyn_cast<MemRefType>(codelet.getDeps()[depIndex].getType());
  if (!memrefType || memrefType.getRank() == 0)
    return false;
  SmallVector<CodirOwnerHaloWindow, 4> windows = getCodirOwnerHaloWindows(
      codelet, depIndex, static_cast<unsigned>(memrefType.getRank()),
      /*requireReadOnly=*/false);
  return llvm::any_of(windows, [](const CodirOwnerHaloWindow &window) {
    return !window.empty();
  });
}

// Halo-block storage is decided from the committed structure alone: a
// compute-block storage view, a committed `halo` collective, and an
// access-window halo. The committed halo collective is what marks the edge as
// reading neighbor tiles, so no source pattern is consulted.
static inline bool codirDepUsesHaloStencilStorage(codir::CodeletOp codelet,
                                                  unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return false;
  return getFinalizedCodirDepCollectiveKind(codelet, depIndex) ==
             codir::CodirCollectiveKind::halo &&
         codirDepHasHaloWindow(codelet, depIndex);
}

static inline bool codirDepUsesBlockNativeSettle(codir::CodeletOp codelet,
                                                 unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  if (getFinalizedCodirDepCollectiveKind(codelet, depIndex) !=
      codir::CodirCollectiveKind::reduce_scatter)
    return false;
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  if (!view || *view != codir::CodirStorageViewKind::phase_redistributed)
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return false;
  auto factor = codelet.getPartialReductionSplitFactorAttr();
  return factor && factor.getInt() > 0;
}

static inline bool
codirDepHasNoStencilReachAlongOwnerDims(codir::CodeletOp codelet,
                                        unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  auto memrefType = dyn_cast<MemRefType>(codelet.getDeps()[depIndex].getType());
  if (!memrefType || memrefType.getRank() == 0)
    return false;
  if (!codelet.getAccessMinOffsetsAttr() || !codelet.getAccessMaxOffsetsAttr())
    return false;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return false;

  unsigned memrefRank = static_cast<unsigned>(memrefType.getRank());
  for (unsigned ownerDim : *ownerDims) {
    std::optional<unsigned> ownerSlot = getCodirOwnerDimSlot(codelet, ownerDim);
    std::optional<int64_t> minOffset = getCodirOwnerDimValue(
        codelet.getAccessMinOffsetsAttr(), ownerDim, ownerSlot, memrefRank);
    std::optional<int64_t> maxOffset = getCodirOwnerDimValue(
        codelet.getAccessMaxOffsetsAttr(), ownerDim, ownerSlot, memrefRank);
    if (!minOffset || !maxOffset)
      return false;
    if (*minOffset != 0 || *maxOffset != 0)
      return false;
  }
  return true;
}

static inline bool
codirDepUsesOwnerLocalStencilStorage(codir::CodeletOp codelet,
                                     unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  // Owner-local stencil storage is proven structurally: a compute-block view, a
  // read-only access mode, no committed collective, and an access window that
  // never reaches outside the owner slice. The committed access-window offsets
  // (required by codirDepHasNoStencilReachAlongOwnerDims) are the stencil
  // evidence, not the source pattern.
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return false;
  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode))
    return false;
  if (getFinalizedCodirDepCollectiveKind(codelet, depIndex) !=
      codir::CodirCollectiveKind::none)
    return false;
  return codirDepHasNoStencilReachAlongOwnerDims(codelet, depIndex);
}

static inline bool
codirRootHasHaloStencilStorageParticipant(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
      codelet.getDeps()[depIndex]);
  Operation *scope = codelet->getParentOfType<ModuleOp>();
  if (!root || !scope)
    return false;

  bool found = false;
  scope->walk([&](codir::CodeletOp candidate) {
    if (found)
      return;
    for (auto [idx, dep] : llvm::enumerate(candidate.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != root)
        continue;
      if (codirDepUsesHaloStencilStorage(candidate,
                                         static_cast<unsigned>(idx))) {
        found = true;
        return;
      }
    }
  });
  return found;
}

static inline bool codirDepCanUseBlockStorageAccess(codir::CodeletOp codelet,
                                                    unsigned depIndex) {
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      !hasCodirTileOwnerSlicePlan(codelet) ||
      !getCodirDepOwnerDims(codelet, depIndex))
    return false;

  // CODIR's storage plan is the committed layout fact. Do not silently
  // coarse-fallback here because the local access tracer is conservative; the
  // block-local index rewrite below is the fail-closed verifier for the actual
  // transformed body.
  return true;
}

static inline bool codirAccessMayWrite(codir::CodirAccessMode mode) {
  return mode == codir::CodirAccessMode::write ||
         mode == codir::CodirAccessMode::readwrite;
}

static inline bool codirAccessMayRead(codir::CodirAccessMode mode) {
  return mode == codir::CodirAccessMode::read ||
         mode == codir::CodirAccessMode::readwrite;
}


} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_COMMONMATERIALIZATION_H
