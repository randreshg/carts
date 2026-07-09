///==========================================================================///
/// File: SdeCommVolumeCost.cpp
///
/// Phase B substrate: communication volume cost per owner-dim choice.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SdeCommVolumeCost.h"

#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/STLExtras.h"

#include <algorithm>

namespace mlir::carts::sde {
namespace {

static int64_t elementBytes(Value root) {
  auto type = dyn_cast_or_null<ShapedType>(root ? root.getType() : Type{});
  if (!type)
    return 0;
  Type elementType = type.getElementType();
  if (!elementType.isIntOrFloat())
    return 0;
  return std::max<int64_t>(
      1, llvm::divideCeil(elementType.getIntOrFloatBitWidth(), 8));
}

static int64_t fullArrayBytes(const ArrayAccessProfile &profile) {
  return tilePayloadBytes(profile.staticShape, elementBytes(profile.root));
}

static int64_t ownerFaceBytes(ArrayRef<int64_t> blockShape, int64_t ownerDim,
                              int64_t bytesPerElement) {
  if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= blockShape.size())
    return 0;
  SmallVector<int64_t, 4> face(blockShape.begin(), blockShape.end());
  face[ownerDim] = 1;
  return tilePayloadBytes(face, bytesPerElement);
}

static bool ownerContains(ArrayRef<int64_t> owners, unsigned position) {
  return llvm::is_contained(owners, static_cast<int64_t>(position));
}

static std::optional<SdeAccessMode> modeForRole(LayoutGraphRole role) {
  switch (role) {
  case LayoutGraphRole::read:
    return SdeAccessMode::read;
  case LayoutGraphRole::write:
    return SdeAccessMode::write;
  case LayoutGraphRole::unknown:
    return std::nullopt;
  }
  return std::nullopt;
}

static const ArrayAccessProfile *
findProfileForRoot(const ModuleSuAccessRelations &relations, Value root) {
  root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root);
  auto direct = relations.profiles.find(root);
  if (direct != relations.profiles.end())
    return &direct->second;
  for (const auto &entry : relations.profiles)
    if (::mlir::carts::ValueAnalysis::sameMemrefRoot(entry.first, root))
      return &entry.second;
  return nullptr;
}

} // namespace

SdeCommVolumeCost::SdeCommVolumeCost(Operation *op) : operation(op) {
  compute();
}

void SdeCommVolumeCost::compute() {
  committedBytesBySuAndArray.clear();
  if (!operation)
    return;

  ModuleSuAccessRelations relations = buildModuleSuAccessRelations(operation);
  operation->walk([&](SdeSuIterateOp su) {
    ArrayAttr layout = su.getArrayLayoutAttr();
    if (!layout)
      return;

    SmallVector<SdeCommittedLayoutCostRecord, 4> records;
    for (const LayoutGraphFact &fact : parseArrayLayoutFacts(layout)) {
      std::optional<SdeAccessMode> mode = modeForRole(fact.role);
      if (!mode || fact.id < 0 || fact.blockShape.empty())
        continue;
      Value root = findArrayLayoutRoot(su, fact.id, *mode);
      const ArrayAccessProfile *profile = findProfileForRoot(relations, root);
      if (!profile)
        continue;

      ArrayLayoutCandidate candidate;
      candidate.kind = fact.layoutKind;
      candidate.ownerPositions.assign(fact.ownerDims.begin(),
                                      fact.ownerDims.end());
      ArrayRef<int64_t> blockShape = fact.budgetBlockShape.empty()
                                         ? ArrayRef<int64_t>(fact.blockShape)
                                         : ArrayRef<int64_t>(fact.budgetBlockShape);
      candidate.blockShape.assign(blockShape.begin(), blockShape.end());
      records.push_back(
          {fact.id, fact.role, estimateCandidateBytes(*profile, candidate)});
    }
    if (!records.empty())
      committedBytesBySuAndArray[su.getOperation()] = std::move(records);
  });
}

int64_t SdeCommVolumeCost::estimateCandidateBytes(
    const ArrayAccessProfile &profile,
    const ArrayLayoutCandidate &candidate) const {
  int64_t bytesPerElement = elementBytes(profile.root);
  if (bytesPerElement <= 0 || profile.staticShape.empty())
    return 0;
  if (candidate.kind == ArrayLayoutKind::replicated ||
      candidate.ownerPositions.empty())
    return fullArrayBytes(profile);
  if (candidate.blockShape.size() != profile.staticShape.size())
    return fullArrayBytes(profile);

  int64_t cost = 0;
  int64_t tileBytes = tilePayloadBytes(candidate.blockShape, bytesPerElement);
  for (auto [position, uses] : llvm::enumerate(profile.positionUses)) {
    if (!ownerContains(candidate.ownerPositions, position))
      continue;
    for (const ArrayPositionUse &use : uses) {
      if (use.isWrite)
        continue;
      switch (use.kind) {
      case ArrayDimKind::parallelIndexed:
        break;
      case ArrayDimKind::parallelHalo:
        cost += ownerFaceBytes(candidate.blockShape, position, bytesPerElement);
        break;
      case ArrayDimKind::reductionIndexed:
      case ArrayDimKind::broadcast:
        cost += tileBytes;
        break;
      }
    }
  }
  return cost;
}

bool SdeCommVolumeCost::isInvalidated(
    const AnalysisManager::PreservedAnalyses &pa) {
  return !pa.isPreserved<SdeCommVolumeCost>();
}

std::optional<int64_t>
SdeCommVolumeCost::getCommittedLayoutBytes(SdeSuIterateOp op, int64_t arrayId,
                                           LayoutGraphRole role) const {
  auto it = committedBytesBySuAndArray.find(op.getOperation());
  if (it == committedBytesBySuAndArray.end())
    return std::nullopt;
  for (const SdeCommittedLayoutCostRecord &record : it->second)
    if (record.arrayId == arrayId && record.role == role)
      return record.bytes;
  return std::nullopt;
}

} // namespace mlir::carts::sde
