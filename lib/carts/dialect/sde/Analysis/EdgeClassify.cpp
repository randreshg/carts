///==========================================================================///
/// File: EdgeClassify.cpp
///
/// SDE redistribution edge classification (movement family decision + A1
/// same-owner re-tile fail-closed gate). Logic carved verbatim from the
/// correctness-base @782988ad1 RedistributionEdges analysis.
///==========================================================================///

#include "carts/dialect/sde/Analysis/EdgeClassify.h"

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/STLExtras.h"

#include <cstdlib>

using namespace mlir;

namespace mlir::carts::sde::redist {

void recordHomeLayout(llvm::DenseMap<int64_t, HomeLayout> &homeByArrayId,
                      llvm::DenseSet<int64_t> &conflictingHome, int64_t arrayId,
                      HomeLayout home) {
  auto it = homeByArrayId.find(arrayId);
  if (it == homeByArrayId.end()) {
    homeByArrayId[arrayId] = std::move(home);
    return;
  }
  if (it->second.ownerDims != home.ownerDims ||
      it->second.blockShape != home.blockShape)
    conflictingHome.insert(arrayId);
}

std::optional<LayoutGraphFact> findLayoutFact(SdeSuIterateOp su,
                                              int64_t arrayId) {
  if (ArrayAttr layout = su.getArrayLayoutAttr())
    for (const LayoutGraphFact &fact : parseArrayLayoutFacts(layout))
      if (fact.id == arrayId)
        return fact;
  return std::nullopt;
}

const ArrayAccessProfile *
findProfileForRoot(const ModuleSuAccessRelations &relations, Value root) {
  auto direct = relations.profiles.find(root);
  if (direct != relations.profiles.end())
    return &direct->second;
  for (const auto &entry : relations.profiles)
    if (::mlir::carts::ValueAnalysis::sameMemrefRoot(entry.first, root))
      return &entry.second;
  return nullptr;
}

std::optional<SmallVector<int64_t, 4>>
getCommittedHaloShape(SdeSuIterateOp reader) {
  if (std::optional<SmallVector<int64_t, 4>> halo =
          deriveCommittedHaloShape(reader))
    return halo;

  std::optional<SmallVector<int64_t, 4>> mins =
      readI64ArrayAttr(reader.getAccessMinOffsetsAttr());
  std::optional<SmallVector<int64_t, 4>> maxs =
      readI64ArrayAttr(reader.getAccessMaxOffsetsAttr());
  if (!mins || !maxs || mins->size() != maxs->size())
    return std::nullopt;

  SmallVector<int64_t, 4> halo;
  halo.reserve(mins->size());
  bool nonZero = false;
  for (auto [minOffset, maxOffset] : llvm::zip_equal(*mins, *maxs)) {
    int64_t width =
        std::max<int64_t>(std::llabs(minOffset), std::llabs(maxOffset));
    nonZero |= width != 0;
    halo.push_back(width);
  }
  if (!nonZero)
    return std::nullopt;
  return halo;
}

bool sameOwnerDimSet(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
  if (lhs.size() != rhs.size())
    return false;
  SmallVector<int64_t, 4> lhsSorted(lhs.begin(), lhs.end());
  SmallVector<int64_t, 4> rhsSorted(rhs.begin(), rhs.end());
  llvm::sort(lhsSorted);
  llvm::sort(rhsSorted);
  return lhsSorted == rhsSorted;
}

ArrayRef<int64_t> committedBlockShape(const LayoutGraphFact &fact) {
  return fact.budgetBlockShape.empty()
             ? ArrayRef<int64_t>(fact.blockShape)
             : ArrayRef<int64_t>(fact.budgetBlockShape);
}

bool readerCommittedLayoutDisagreesWithHome(
    const std::optional<LayoutGraphFact> &readerFact, const HomeLayout &home) {
  if (!readerFact || readerFact->role != LayoutGraphRole::read)
    return false;
  if (!sameOwnerDimSet(readerFact->ownerDims, home.ownerDims))
    return true;
  return committedBlockShape(*readerFact) != ArrayRef<int64_t>(home.blockShape);
}

bool readerOnlyRetilesSameOwners(
    const std::optional<LayoutGraphFact> &readerFact, const HomeLayout &home) {
  if (!readerFact || readerFact->role != LayoutGraphRole::read)
    return false;
  if (!sameOwnerDimSet(readerFact->ownerDims, home.ownerDims))
    return false;
  return committedBlockShape(*readerFact) != ArrayRef<int64_t>(home.blockShape);
}

EdgeClassify classifyRedistributionEdge(
    const HomeLayout &home, const std::optional<LayoutGraphFact> &readerFact,
    bool hasOwnerReduction, bool committedContractionLayout,
    bool committedHaloLayout) {
  if (hasOwnerReduction || committedContractionLayout)
    return EdgeClassify::ReduceScatter;
  if (committedHaloLayout)
    return EdgeClassify::Halo;
  if (!readerFact || readerFact->role != LayoutGraphRole::read)
    return EdgeClassify::None;
  if (!sameOwnerDimSet(home.ownerDims, readerFact->ownerDims))
    return EdgeClassify::AllToAll;
  return EdgeClassify::None;
}

bool producerConsumerMuTypesDisagree(Value writerRoot, Value readerRoot) {
  writerRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(writerRoot);
  readerRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(readerRoot);
  if (!writerRoot || !readerRoot)
    return false;
  auto writerType = dyn_cast<MemRefType>(writerRoot.getType());
  auto readerType = dyn_cast<MemRefType>(readerRoot.getType());
  if (!writerType || !readerType || !writerType.hasStaticShape() ||
      !readerType.hasStaticShape())
    return false;
  return writerType != readerType;
}

bool readerNeedsRedistributionMovement(SdeSuIterateOp reader, int64_t arrayId,
                                       const HomeLayout &home,
                                       const ModuleSuAccessRelations &relations,
                                       std::optional<unsigned> readerSuId,
                                       Value root) {
  if (home.writer == reader)
    return false;

  std::optional<LayoutGraphFact> readerFact = findLayoutFact(reader, arrayId);
  if (readerCommittedLayoutDisagreesWithHome(readerFact, home))
    return true;

  if (getCommittedHaloShape(reader) && readerFact &&
      sameOwnerDimSet(home.ownerDims, readerFact->ownerDims))
    return true;

  if (readerSuId) {
    if (const ArrayAccessProfile *profile = findProfileForRoot(relations, root))
      for (auto [pos, posUses] : llvm::enumerate(profile->positionUses))
        for (const ArrayPositionUse &use : posUses)
          if (use.suId == *readerSuId && !use.isWrite &&
              use.kind == ArrayDimKind::reductionIndexed &&
              llvm::is_contained(home.ownerDims, static_cast<int64_t>(pos)))
            return true;
  }

  if (readerFact && readerFact->ownerDims == home.ownerDims &&
      reader.getPartialReductionAttr())
    return true;

  Value writerRoot =
      findArrayLayoutRoot(home.writer, arrayId, SdeAccessMode::write);
  Value readerRoot = findArrayLayoutRoot(reader, arrayId, SdeAccessMode::read);
  return producerConsumerMuTypesDisagree(writerRoot, readerRoot);
}

void collectReaderRedistributionCandidates(
    SdeSuIterateOp reader, llvm::SmallVector<int64_t, 4> &arrayIds) {
  llvm::DenseSet<int64_t> seen;
  if (reader.getBody().empty())
    return;
  for (SdeArrayLayoutRootOp root :
       reader.getBody().front().getOps<SdeArrayLayoutRootOp>()) {
    if (root.getMode() != SdeAccessMode::read)
      continue;
    int64_t id = root.getArrayId();
    if (seen.insert(id).second)
      arrayIds.push_back(id);
  }
  if (ArrayAttr layout = reader.getArrayLayoutAttr()) {
    for (const LayoutGraphFact &fact : parseArrayLayoutFacts(layout)) {
      if (fact.role != LayoutGraphRole::read)
        continue;
      if (seen.insert(fact.id).second)
        arrayIds.push_back(fact.id);
    }
  }
}

} // namespace mlir::carts::sde::redist
