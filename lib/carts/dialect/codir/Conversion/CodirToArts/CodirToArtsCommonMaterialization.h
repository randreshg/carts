///==========================================================================///
/// File: CodirToArtsCommonMaterialization.h
///
/// Common CODIR fact readers and materialization predicates.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_COMMONMATERIALIZATION_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_COMMONMATERIALIZATION_H

#include "CodirToArtsBlockLocalAccess.h"
#include "CodirToArtsDbBackedMemref.h"
#include "carts/dialect/arts/Utils/DbLayoutPlanUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/codir/Utils/CodirConversionUtils.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/STLExtras.h"
#include <algorithm>
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
