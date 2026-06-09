///==========================================================================///
/// File: CodirToArtsOwnerDims.h
///
/// CODIR owner-dimension, owner-parameter, and block-shape readers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_OWNERDIMS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_OWNERDIMS_H

#include "CodirToArtsDepFacts.h"
#include "carts/dialect/arts/Utils/DbUtils.h"

namespace {

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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_OWNERDIMS_H
