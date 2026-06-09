///==========================================================================///
/// File: CodirToArtsOwnerSliceContainment.h
///
/// Owner-slice containment checks for CODIR dependency accesses.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_OWNERSLICECONTAINMENT_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_OWNERSLICECONTAINMENT_H

#include "CodirToArtsOwnerDims.h"

namespace {

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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_OWNERSLICECONTAINMENT_H
