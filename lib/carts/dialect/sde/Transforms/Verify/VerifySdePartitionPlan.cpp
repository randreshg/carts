///==========================================================================///
/// File: VerifySdePartitionPlan.cpp
///==========================================================================///

#include "carts/dialect/sde/Analysis/StructuredOpAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/SdePlanUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEPARTITIONPLAN
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

#include <algorithm>

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool isAllowedPartitionEvidenceString(StringRef key, StringRef value) {
  if (key == "role")
    return value == "read" || value == "write" || value == "readwrite" ||
           value == "unknown";
  if (key == "edgeClass")
    return value == "aligned" || value == "layout_mismatch";
  if (key == "layoutKind" || key == "kind")
    return value == "owner_block" || value == "block_parallel" ||
           value == "block_contraction" || value == "replicated" ||
           value == "unknown_layout";
  if (key == "objective")
    return value == "max_concurrency_comm_aware";
  return true;
}

static bool isAllowedPartitionEvidenceKey(StringRef key) {
  return key == sde::AttrNames::PartitionGraphKeys::MuId ||
         key == sde::AttrNames::PartitionGraphKeys::Role ||
         key == sde::AttrNames::PartitionGraphKeys::LayoutKind ||
         key == sde::AttrNames::PartitionGraphKeys::OwnerDims ||
         key == sde::AttrNames::PartitionGraphKeys::BlockShape ||
         key == sde::AttrNames::PartitionGraphKeys::TilePayloadBytes ||
         key == sde::AttrNames::PartitionGraphKeys::MuBlockCount ||
         key == sde::AttrNames::PartitionGraphKeys::CuGroupSize ||
         key == sde::AttrNames::PartitionGraphKeys::CuGroupCount ||
         key == sde::AttrNames::PartitionGraphKeys::EdgeCommBytes ||
         key == sde::AttrNames::PartitionGraphKeys::EdgeClass ||
         key == sde::AttrNames::PartitionScoreKeys::Objective ||
         key == sde::AttrNames::PartitionScoreKeys::TargetLogicalWorkers ||
         key == sde::AttrNames::PartitionScoreKeys::ExposedCuCount ||
         key == sde::AttrNames::PartitionScoreKeys::RequestedCuCount ||
         key == sde::AttrNames::PartitionScoreKeys::ChosenCuCount ||
         key == sde::AttrNames::PartitionScoreKeys::MuBlockCount ||
         key == sde::AttrNames::PartitionScoreKeys::CuGroupSize ||
         key == sde::AttrNames::PartitionScoreKeys::CuGroupCount ||
         key == sde::AttrNames::PartitionScoreKeys::MinTileBytes ||
         key == sde::AttrNames::PartitionScoreKeys::ChosenTileBytes ||
         key == sde::AttrNames::PartitionScoreKeys::CommVolumeBytes ||
         key == sde::AttrNames::PartitionScoreKeys::OwnerDims ||
         key == sde::AttrNames::PartitionScoreKeys::BlockShape;
}

static void verifyPartitionEvidenceIsRuntimeNeutral(sde::SdeSuIterateOp op,
                                                    Attribute attr,
                                                    StringRef attrName,
                                                    StringRef attrKey,
                                                    bool &hasFailure) {
  if (!attr)
    return;

  if (auto stringAttr = dyn_cast<StringAttr>(attr)) {
    if (!isAllowedPartitionEvidenceString(attrKey, stringAttr.getValue())) {
      op.emitOpError() << attrName
                       << " must stay runtime-neutral; found non-SDE "
                          "partition evidence value '"
                       << stringAttr.getValue() << "'";
      hasFailure = true;
    }
    return;
  }

  if (auto arrayAttr = dyn_cast<ArrayAttr>(attr)) {
    for (Attribute element : arrayAttr)
      verifyPartitionEvidenceIsRuntimeNeutral(op, element, attrName, attrKey,
                                              hasFailure);
    return;
  }

  if (auto dictAttr = dyn_cast<DictionaryAttr>(attr)) {
    for (NamedAttribute namedAttr : dictAttr) {
      StringRef key = namedAttr.getName().strref();
      if (!isAllowedPartitionEvidenceKey(key)) {
        op.emitOpError()
            << attrName
            << " must carry only SDE CU/MU geometry, abstract cost, and "
               "concurrency evidence; found unknown evidence key '"
            << key << "'";
        hasFailure = true;
      }
      verifyPartitionEvidenceIsRuntimeNeutral(op, namedAttr.getValue(),
                                              attrName, key, hasFailure);
    }
  }
}

static bool readI64ArrayForPartition(sde::SdeSuIterateOp op, Attribute attr,
                                     StringRef attrName, StringRef key,
                                     SmallVectorImpl<int64_t> &values,
                                     bool &hasFailure,
                                     bool requireNonEmpty = false) {
  if (!attr) {
    if (requireNonEmpty) {
      op.emitOpError() << attrName << "." << key
                       << " must be a non-empty i64 array attribute";
      hasFailure = true;
    }
    return false;
  }
  auto arrayAttr = dyn_cast<ArrayAttr>(attr);
  if (!arrayAttr) {
    op.emitOpError() << attrName << "." << key
                     << " must be an i64 array attribute";
    hasFailure = true;
    return false;
  }
  std::optional<SmallVector<int64_t, 4>> parsed = readI64ArrayAttr(arrayAttr);
  if (!parsed) {
    op.emitOpError() << attrName << "." << key
                     << " must contain only integer attributes";
    hasFailure = true;
    return false;
  }
  values.assign(parsed->begin(), parsed->end());
  if (requireNonEmpty && values.empty()) {
    op.emitOpError() << attrName << "." << key
                     << " must be a non-empty i64 array attribute";
    hasFailure = true;
    return false;
  }
  return true;
}

static bool arraysEqual(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

static void verifyArrayMatchesPhysicalPlan(sde::SdeSuIterateOp op,
                                           StringRef attrName, StringRef key,
                                           ArrayRef<int64_t> evidence,
                                           ArrayRef<int64_t> physical,
                                           bool &hasFailure) {
  if (physical.empty() || arraysEqual(evidence, physical))
    return;
  op.emitOpError() << attrName << "." << key
                   << " no longer matches the SDE physical plan; committed "
                      "CU/MU partition evidence must be preserved by later "
                      "SDE passes";
  hasFailure = true;
}

static void verifyPositiveI64ScoreField(sde::SdeSuIterateOp op,
                                        DictionaryAttr score, StringRef key,
                                        bool &hasFailure,
                                        bool *wasPresent = nullptr) {
  Attribute value = score.get(key);
  if (wasPresent)
    *wasPresent |= static_cast<bool>(value);
  if (!value)
    return;
  auto intAttr = dyn_cast<IntegerAttr>(value);
  if (intAttr && intAttr.getInt() > 0)
    return;
  op.emitOpError() << sde::AttrNames::PartitionScore << "." << key
                   << " must be a positive integer attribute";
  hasFailure = true;
}

static void verifyNonNegativeI64ScoreField(sde::SdeSuIterateOp op,
                                           DictionaryAttr score, StringRef key,
                                           bool &hasFailure) {
  Attribute value = score.get(key);
  if (!value)
    return;
  auto intAttr = dyn_cast<IntegerAttr>(value);
  if (intAttr && intAttr.getInt() >= 0)
    return;
  op.emitOpError() << sde::AttrNames::PartitionScore << "." << key
                   << " must be a non-negative integer attribute";
  hasFailure = true;
}

static void verifyCommittedPartitionScore(sde::SdeSuIterateOp op,
                                          ArrayRef<int64_t> physicalOwnerDims,
                                          ArrayRef<int64_t> physicalBlockShape,
                                          bool &hasFailure) {
  Attribute scoreAttr = op->getAttr(sde::AttrNames::PartitionScore);
  if (!scoreAttr)
    return;

  auto score = dyn_cast<DictionaryAttr>(scoreAttr);
  if (!score) {
    op.emitOpError() << sde::AttrNames::PartitionScore
                     << " must be a dictionary attribute";
    hasFailure = true;
    return;
  }

  bool hasConcurrencyEvidence = false;
  verifyPositiveI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::TargetLogicalWorkers,
      hasFailure, &hasConcurrencyEvidence);
  verifyPositiveI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::ExposedCuCount, hasFailure,
      &hasConcurrencyEvidence);
  verifyPositiveI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::RequestedCuCount,
      hasFailure);
  verifyPositiveI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::ChosenCuCount, hasFailure);
  verifyPositiveI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::MuBlockCount, hasFailure);
  verifyPositiveI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::CuGroupSize, hasFailure);
  verifyPositiveI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::CuGroupCount, hasFailure);
  verifyPositiveI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::ChosenTileBytes,
      hasFailure);
  verifyNonNegativeI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::MinTileBytes, hasFailure);
  verifyNonNegativeI64ScoreField(
      op, score, sde::AttrNames::PartitionScoreKeys::CommVolumeBytes,
      hasFailure);

  if (!hasConcurrencyEvidence) {
    op.emitOpError() << sde::AttrNames::PartitionScore << " must contain "
                     << sde::AttrNames::PartitionScoreKeys::TargetLogicalWorkers
                     << " or "
                     << sde::AttrNames::PartitionScoreKeys::ExposedCuCount;
    hasFailure = true;
  }

  SmallVector<int64_t, 4> ownerDims;
  if (readI64ArrayForPartition(
          op, score.get(sde::AttrNames::PartitionScoreKeys::OwnerDims),
          sde::AttrNames::PartitionScore,
          sde::AttrNames::PartitionScoreKeys::OwnerDims, ownerDims, hasFailure,
          /*requireNonEmpty=*/true))
    verifyArrayMatchesPhysicalPlan(
        op, sde::AttrNames::PartitionScore,
        sde::AttrNames::PartitionScoreKeys::OwnerDims, ownerDims,
        physicalOwnerDims, hasFailure);

  SmallVector<int64_t, 4> blockShape;
  if (readI64ArrayForPartition(
          op, score.get(sde::AttrNames::PartitionScoreKeys::BlockShape),
          sde::AttrNames::PartitionScore,
          sde::AttrNames::PartitionScoreKeys::BlockShape, blockShape,
          hasFailure, /*requireNonEmpty=*/true))
    verifyArrayMatchesPhysicalPlan(
        op, sde::AttrNames::PartitionScore,
        sde::AttrNames::PartitionScoreKeys::BlockShape, blockShape,
        physicalBlockShape, hasFailure);
}

static bool graphEntryDescribesPrimaryMu(DictionaryAttr entry) {
  auto layoutKind = dyn_cast_or_null<StringAttr>(
      entry.get(sde::AttrNames::PartitionGraphKeys::LayoutKind));
  return layoutKind && layoutKind.getValue() ==
                           sde::AttrNames::PartitionGraphValues::OwnerBlock;
}

static void verifyCommittedPartitionGraph(sde::SdeSuIterateOp op,
                                          ArrayRef<int64_t> physicalOwnerDims,
                                          ArrayRef<int64_t> physicalBlockShape,
                                          bool &hasFailure) {
  Attribute graphAttr = op->getAttr(sde::AttrNames::PartitionGraph);
  if (!graphAttr)
    return;

  auto graph = dyn_cast<ArrayAttr>(graphAttr);
  if (!graph) {
    op.emitOpError() << sde::AttrNames::PartitionGraph
                     << " must be an array attribute";
    hasFailure = true;
    return;
  }

  for (Attribute attr : graph) {
    auto entry = dyn_cast<DictionaryAttr>(attr);
    if (!entry) {
      op.emitOpError() << sde::AttrNames::PartitionGraph
                       << " entries must be dictionary attributes";
      hasFailure = true;
      continue;
    }

    if (auto blocks = dyn_cast_or_null<IntegerAttr>(
            entry.get(sde::AttrNames::PartitionGraphKeys::MuBlockCount))) {
      if (blocks.getInt() <= 0) {
        op.emitOpError() << sde::AttrNames::PartitionGraph << "."
                         << sde::AttrNames::PartitionGraphKeys::MuBlockCount
                         << " must be positive";
        hasFailure = true;
      }
    }
    if (auto group = dyn_cast_or_null<IntegerAttr>(
            entry.get(sde::AttrNames::PartitionGraphKeys::CuGroupSize))) {
      if (group.getInt() <= 0) {
        op.emitOpError() << sde::AttrNames::PartitionGraph << "."
                         << sde::AttrNames::PartitionGraphKeys::CuGroupSize
                         << " must be positive";
        hasFailure = true;
      }
    }
    if (auto groups = dyn_cast_or_null<IntegerAttr>(
            entry.get(sde::AttrNames::PartitionGraphKeys::CuGroupCount))) {
      if (groups.getInt() <= 0) {
        op.emitOpError() << sde::AttrNames::PartitionGraph << "."
                         << sde::AttrNames::PartitionGraphKeys::CuGroupCount
                         << " must be positive";
        hasFailure = true;
      }
    }

    if (!graphEntryDescribesPrimaryMu(entry))
      continue;

    SmallVector<int64_t, 4> ownerDims;
    if (readI64ArrayForPartition(
            op, entry.get(sde::AttrNames::PartitionGraphKeys::OwnerDims),
            sde::AttrNames::PartitionGraph,
            sde::AttrNames::PartitionGraphKeys::OwnerDims, ownerDims,
            hasFailure, /*requireNonEmpty=*/true))
      verifyArrayMatchesPhysicalPlan(
          op, sde::AttrNames::PartitionGraph,
          sde::AttrNames::PartitionGraphKeys::OwnerDims, ownerDims,
          physicalOwnerDims, hasFailure);

    SmallVector<int64_t, 4> blockShape;
    if (readI64ArrayForPartition(
            op, entry.get(sde::AttrNames::PartitionGraphKeys::BlockShape),
            sde::AttrNames::PartitionGraph,
            sde::AttrNames::PartitionGraphKeys::BlockShape, blockShape,
            hasFailure, /*requireNonEmpty=*/true))
      verifyArrayMatchesPhysicalPlan(
          op, sde::AttrNames::PartitionGraph,
          sde::AttrNames::PartitionGraphKeys::BlockShape, blockShape,
          physicalBlockShape, hasFailure);
  }
}

static void verifyCommittedEvidenceHasPhysicalPlan(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> physicalOwnerDims,
    ArrayRef<int64_t> physicalBlockShape, bool &hasFailure) {
  if (!sde::hasCommittedCuMuPartitionEvidence(op.getOperation()))
    return;

  if (physicalOwnerDims.empty()) {
    op.emitOpError()
        << "committed CU/MU partition evidence requires physicalOwnerDims";
    hasFailure = true;
  }
  if (physicalBlockShape.empty()) {
    op.emitOpError()
        << "committed CU/MU partition evidence requires physicalBlockShape";
    hasFailure = true;
  }
}

static void verifyPhysicalPlanRealizability(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> physicalOwnerDims,
    ArrayRef<int64_t> physicalBlockShape, bool &hasFailure) {
  if (!op.getPhysicalOwnerDimsAttr() && !op.getPhysicalBlockShapeAttr())
    return;

  if (!op.getPhysicalOwnerDimsAttr() || !op.getPhysicalBlockShapeAttr()) {
    op.emitOpError()
        << "physical plan requires physicalOwnerDims and physicalBlockShape "
           "together";
    hasFailure = true;
    return;
  }

  if (physicalOwnerDims.empty()) {
    op.emitOpError() << "physicalOwnerDims must be non-empty when a physical "
                        "plan is present";
    hasFailure = true;
  }
  if (physicalBlockShape.empty()) {
    op.emitOpError() << "physicalBlockShape must be non-empty when a physical "
                        "plan is present";
    hasFailure = true;
  }

  for (int64_t ownerDim : physicalOwnerDims) {
    if (ownerDim >= 0 &&
        static_cast<size_t>(ownerDim) < physicalBlockShape.size())
      continue;
    op.emitOpError()
        << "physicalOwnerDims must reference physicalBlockShape dimensions";
    hasFailure = true;
    break;
  }

  for (int64_t extent : physicalBlockShape) {
    if (extent > 0)
      continue;
    op.emitOpError() << "physicalBlockShape entries must be positive";
    hasFailure = true;
    break;
  }

  if (physicalOwnerDims.size() > op.getSteps().size()) {
    op.emitOpError()
        << "physical plan has more owner dimensions than realized SDE loop "
           "dimensions";
    hasFailure = true;
  }

  if (!sde::hasCommittedCuMuPartitionEvidence(op.getOperation()) ||
      !sde::hasNestedStencilOwnerContract(op) ||
      sde::hasRealizableOwnerStripPlan(op))
    return;

  op.emitOpError()
      << "committed physical plan is not realizable by current SDE loop rank; "
         "promote nested stencil owner dimensions before stamping CU/MU "
         "partition evidence";
  hasFailure = true;
}

static void verifyCommittedCuMuPartitionPlan(sde::SdeSuIterateOp op,
                                             bool &hasFailure) {
  if (!sde::hasCommittedCuMuPartitionPlan(op.getOperation()))
    return;

  verifyPartitionEvidenceIsRuntimeNeutral(
      op, op->getAttr(sde::AttrNames::PartitionGraph),
      sde::AttrNames::PartitionGraph, /*attrKey=*/"", hasFailure);
  verifyPartitionEvidenceIsRuntimeNeutral(
      op, op->getAttr(sde::AttrNames::PartitionScore),
      sde::AttrNames::PartitionScore, /*attrKey=*/"", hasFailure);

  SmallVector<int64_t, 4> physicalOwnerDims;
  if (auto parsed = readI64ArrayAttr(op.getPhysicalOwnerDimsAttr()))
    physicalOwnerDims.assign(parsed->begin(), parsed->end());
  SmallVector<int64_t, 4> physicalBlockShape;
  if (auto parsed = readI64ArrayAttr(op.getPhysicalBlockShapeAttr()))
    physicalBlockShape.assign(parsed->begin(), parsed->end());

  verifyCommittedEvidenceHasPhysicalPlan(op, physicalOwnerDims,
                                         physicalBlockShape, hasFailure);
  verifyPhysicalPlanRealizability(op, physicalOwnerDims, physicalBlockShape,
                                  hasFailure);
  verifyCommittedPartitionScore(op, physicalOwnerDims, physicalBlockShape,
                                hasFailure);
  verifyCommittedPartitionGraph(op, physicalOwnerDims, physicalBlockShape,
                                hasFailure);
}

struct VerifySdePartitionPlanPass
    : public sde::impl::VerifySdePartitionPlanBase<VerifySdePartitionPlanPass> {
  void runOnOperation() override {
    bool hasFailure = false;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      verifyCommittedCuMuPartitionPlan(op, hasFailure);
    });

    if (hasFailure)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::sde::createVerifySdePartitionPlanPass() {
  return std::make_unique<VerifySdePartitionPlanPass>();
}
