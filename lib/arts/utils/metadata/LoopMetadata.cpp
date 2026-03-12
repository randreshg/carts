///==========================================================================///
/// File: LoopMetadata.cpp
/// Implementation of LoopMetadata class for loop metadata management.
///==========================================================================///

#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"

using namespace mlir;
using namespace mlir::arts;

///===-------------------------------------------------------------===///
/// Enums
///===-------------------------------------------------------------===///
const char *LoopMetadata::reductionKindToString(ReductionKind kind) const {
  switch (kind) {
  case ReductionKind::Add:
    return "add";
  case ReductionKind::Mul:
    return "mul";
  case ReductionKind::Min:
    return "min";
  case ReductionKind::Max:
    return "max";
  case ReductionKind::And:
    return "and";
  case ReductionKind::Or:
    return "or";
  case ReductionKind::Xor:
    return "xor";
  case ReductionKind::Unknown:
    return "unknown";
  }
  return "unknown";
}

LoopMetadata::ReductionKind
LoopMetadata::stringToReductionKind(llvm::StringRef str) const {
  return llvm::StringSwitch<ReductionKind>(str)
      .Case("add", ReductionKind::Add)
      .Case("mul", ReductionKind::Mul)
      .Case("min", ReductionKind::Min)
      .Case("max", ReductionKind::Max)
      .Case("and", ReductionKind::And)
      .Case("or", ReductionKind::Or)
      .Case("xor", ReductionKind::Xor)
      .Default(ReductionKind::Unknown);
}

const char *LoopMetadata::parallelClassificationToString(
    ParallelClassification classification) const {
  switch (classification) {
  case ParallelClassification::ReadOnly:
    return "read_only";
  case ParallelClassification::Likely:
    return "likely";
  case ParallelClassification::Sequential:
    return "sequential";
  case ParallelClassification::Unknown:
    return "unknown";
  }
  return "unknown";
}

LoopMetadata::ParallelClassification
LoopMetadata::stringToParallelClassification(llvm::StringRef str) const {
  return llvm::StringSwitch<ParallelClassification>(str)
      .Case("read_only", ParallelClassification::ReadOnly)
      .Case("likely", ParallelClassification::Likely)
      .Case("sequential", ParallelClassification::Sequential)
      .Default(ParallelClassification::Unknown);
}

////===----------------------------------------------------------------------===////
/// Interface
////===----------------------------------------------------------------------===////
bool LoopMetadata::importFromOp() {
  importIdFromOp();

  /// Get the metadata attribute
  LoopMetadataAttr attr =
      op_->getAttrOfType<LoopMetadataAttr>(getMetadataName());
  if (!attr)
    return false;

  /// Parallelism analysis
  potentiallyParallel = getBoolFromAttr(attr.getPotentiallyParallel());
  hasReductions = getBoolFromAttr(attr.getHasReductions());
  ArrayAttr reductionKindsAttr = attr.getReductionKinds();
  reductionKinds.clear();
  if (reductionKindsAttr) {
    for (auto kind : reductionKindsAttr.getValue())
      reductionKinds.push_back(
          stringToReductionKind(kind.cast<StringAttr>().str()));
  }

  /// Loop structure information
  tripCount = getIntFromAttr(attr.getTripCount()).value_or(0);
  nestingLevel = getIntFromAttr(attr.getNestingLevel()).value_or(0);

  /// Dependency information
  hasInterIterationDeps = getBoolFromAttr(attr.getHasInterIterationDeps());

  if (auto value = getIntFromAttr(attr.getMemrefsWithLoopCarriedDeps()))
    memrefsWithLoopCarriedDeps = *value;
  else
    memrefsWithLoopCarriedDeps.reset();
  if (auto value = getIntFromAttr(attr.getParallelClassification()))
    parallelClassification = static_cast<ParallelClassification>(value.value());
  else
    parallelClassification.reset();
  return true;
}

void LoopMetadata::importFromJson(const llvm::json::Object &json) {
  importIdFromJson(json);
  potentiallyParallel =
      json.getBoolean(AttrNames::LoopMetadata::PotentiallyParallel)
          .value_or(false);
  hasReductions =
      json.getBoolean(AttrNames::LoopMetadata::HasReductions).value_or(false);
  reductionKinds.clear();
  if (auto *arr = json.getArray(AttrNames::LoopMetadata::ReductionKinds)) {
    for (const auto &e : *arr)
      if (auto i = e.getAsInteger())
        reductionKinds.push_back(static_cast<ReductionKind>(*i));
  }
  tripCount = json.getInteger(AttrNames::LoopMetadata::TripCount).value_or(0);
  nestingLevel =
      json.getInteger(AttrNames::LoopMetadata::NestingLevel).value_or(0);
  hasInterIterationDeps =
      json.getBoolean(AttrNames::LoopMetadata::HasInterIterationDeps)
          .value_or(false);
  memrefsWithLoopCarriedDeps =
      json.getInteger(AttrNames::LoopMetadata::MemrefsWithLoopCarriedDeps)
          .value_or(0);
  parallelClassification = ParallelClassification::Unknown;
  if (auto i = json.getInteger(AttrNames::LoopMetadata::ParallelClassification))
    parallelClassification = static_cast<ParallelClassification>(*i);
  locationMetadata = LocationMetadata::fromKey(
      json.getString(AttrNames::LoopMetadata::LocationKey).value_or(""));

  /// Loop reordering: import target order (arts.ids)
  reorderNestTo.clear();
  if (auto *arr = json.getArray(AttrNames::LoopMetadata::ReorderNestTo)) {
    for (const auto &e : *arr)
      if (auto id = e.getAsInteger())
        reorderNestTo.push_back(*id);
  }

  /// Per-dimension dependency analysis
  dimensionDeps.clear();
  if (auto *arr = json.getArray(AttrNames::LoopMetadata::DimensionDeps)) {
    for (const auto &e : *arr) {
      if (auto *obj = e.getAsObject()) {
        DimensionDependency dep;
        dep.importFromJson(*obj);
        dimensionDeps.push_back(dep);
      }
    }
  }

  /// Outermost parallelizable dimension
  if (auto dim = json.getInteger(AttrNames::LoopMetadata::OutermostParallelDim))
    outermostParallelDim = *dim;
  else
    outermostParallelDim.reset();
}

void LoopMetadata::exportToJson(llvm::json::Object &json) const {
  exportIdToJson(json);
  json[AttrNames::LoopMetadata::PotentiallyParallel.str()] =
      potentiallyParallel;
  json[AttrNames::LoopMetadata::HasReductions.str()] = hasReductions;
  if (!reductionKinds.empty()) {
    llvm::json::Array kinds;
    for (auto k : reductionKinds)
      kinds.push_back(static_cast<int64_t>(k));
    json[AttrNames::LoopMetadata::ReductionKinds.str()] = std::move(kinds);
  }
  json[AttrNames::LoopMetadata::TripCount.str()] = tripCount;
  json[AttrNames::LoopMetadata::NestingLevel.str()] = nestingLevel;
  json[AttrNames::LoopMetadata::HasInterIterationDeps.str()] =
      hasInterIterationDeps;
  json[AttrNames::LoopMetadata::MemrefsWithLoopCarriedDeps.str()] =
      memrefsWithLoopCarriedDeps.value_or(0);
  if (parallelClassification)
    json[AttrNames::LoopMetadata::ParallelClassification.str()] =
        static_cast<int64_t>(*parallelClassification);
  json[AttrNames::LoopMetadata::LocationKey.str()] =
      locationMetadata.getKey().str();

  /// Loop reordering: export target order only if non-empty
  if (!reorderNestTo.empty()) {
    llvm::json::Array arr;
    for (auto id : reorderNestTo)
      arr.push_back(id);
    json[AttrNames::LoopMetadata::ReorderNestTo.str()] = std::move(arr);
  }

  /// Per-dimension dependency analysis: export only if non-empty
  if (!dimensionDeps.empty()) {
    llvm::json::Array arr;
    for (const auto &dep : dimensionDeps) {
      llvm::json::Object obj;
      dep.exportToJson(obj);
      arr.push_back(std::move(obj));
    }
    json[AttrNames::LoopMetadata::DimensionDeps.str()] = std::move(arr);
  }

  /// Outermost parallelizable dimension: export only if set
  if (outermostParallelDim)
    json[AttrNames::LoopMetadata::OutermostParallelDim.str()] =
        *outermostParallelDim;
}

void LoopMetadata::exportToOp() {
  /// Use base class implementation which calls getMetadataAttr()
  ArtsMetadata::exportToOp();
}

Attribute LoopMetadata::getMetadataAttr() const {
  MLIRContext *ctx = op_->getContext();
  OpBuilder builder(ctx);

  /// Helper to convert optional int64 to IntegerAttr
  auto toIntAttr = [&](const std::optional<int64_t> &v,
                       int64_t defaultValue = 0) -> IntegerAttr {
    return builder.getI64IntegerAttr(v.value_or(defaultValue));
  };

  /// Helper to convert optional bool to BoolAttr
  auto toBoolAttr = [&](const std::optional<bool> &v) -> BoolAttr {
    return builder.getBoolAttr(v.value_or(false));
  };

  auto toEnumAttr = [&](const std::optional<ParallelClassification> &v,
                        ParallelClassification defaultValue) -> IntegerAttr {
    return builder.getI64IntegerAttr(
        static_cast<int64_t>(v.value_or(defaultValue)));
  };

  /// Convert reduction kinds to ArrayAttr
  ArrayAttr reductionKindsAttr;
  if (!reductionKinds.empty()) {
    SmallVector<Attribute> kinds;
    for (auto kind : reductionKinds)
      kinds.push_back(builder.getStringAttr(reductionKindToString(kind)));
    reductionKindsAttr = builder.getArrayAttr(kinds);
  }

  /// Build the LoopMetadataAttr
  return LoopMetadataAttr::get(
      ctx,
      /// Parallelism analysis
      builder.getBoolAttr(potentiallyParallel),
      builder.getBoolAttr(hasReductions), reductionKindsAttr,
      /// Loop structure information
      toIntAttr(tripCount), toIntAttr(nestingLevel),
      /// Dependency information
      toBoolAttr(hasInterIterationDeps), toIntAttr(memrefsWithLoopCarriedDeps),
      toEnumAttr(parallelClassification, ParallelClassification::Unknown),
      /// Source location
      builder.getStringAttr(locationMetadata.getKey()));
}

LoopMetadataAttr LoopMetadata::toAttribute(MLIRContext *ctx) const {
  OpBuilder builder(ctx);

  /// Helper to convert optional int64 to IntegerAttr
  auto toIntAttr = [&](const std::optional<int64_t> &v,
                       int64_t defaultValue = 0) -> IntegerAttr {
    return builder.getI64IntegerAttr(v.value_or(defaultValue));
  };

  /// Helper to convert optional bool to BoolAttr
  auto toBoolAttr = [&](const std::optional<bool> &v) -> BoolAttr {
    return builder.getBoolAttr(v.value_or(false));
  };

  auto toEnumAttr = [&](const std::optional<ParallelClassification> &v,
                        ParallelClassification defaultValue) -> IntegerAttr {
    return builder.getI64IntegerAttr(
        static_cast<int64_t>(v.value_or(defaultValue)));
  };

  /// Convert reduction kinds to ArrayAttr
  ArrayAttr reductionKindsAttr;
  if (!reductionKinds.empty()) {
    SmallVector<Attribute> kinds;
    for (auto kind : reductionKinds)
      kinds.push_back(builder.getStringAttr(reductionKindToString(kind)));
    reductionKindsAttr = builder.getArrayAttr(kinds);
  }

  return LoopMetadataAttr::get(
      ctx, builder.getBoolAttr(potentiallyParallel),
      builder.getBoolAttr(hasReductions), reductionKindsAttr,
      toIntAttr(tripCount), toIntAttr(nestingLevel),
      toBoolAttr(hasInterIterationDeps), toIntAttr(memrefsWithLoopCarriedDeps),
      toEnumAttr(parallelClassification, ParallelClassification::Unknown),
      builder.getStringAttr(locationMetadata.getKey()));
}

///===----------------------------------------------------------------------===///
/// DimensionDependency JSON serialization
///===----------------------------------------------------------------------===///
void DimensionDependency::importFromJson(const llvm::json::Object &json) {
  dimension = json.getInteger("dimension").value_or(0);
  hasCarriedDep = json.getBoolean("has_carried_dep").value_or(false);
  if (auto dist = json.getInteger("distance"))
    distance = *dist;
  else
    distance.reset();
}

void DimensionDependency::exportToJson(llvm::json::Object &json) const {
  json["dimension"] = dimension;
  json["has_carried_dep"] = hasCarriedDep;
  if (distance)
    json["distance"] = *distance;
}

LoopMetadataAttr
LoopMetadata::createParallelizedMetadata(MLIRContext *ctx,
                                         const LoopMetadataAttr &base) {
  OpBuilder builder(ctx);

  /// Create new metadata with reduction handling applied:
  /// - potentiallyParallel = true (loop is now parallel after reduction)
  /// - hasReductions = false (handled via partial accumulators)
  /// - hasInterIterationDeps = false (broken by partial accumulators)
  /// - memrefsWithLoopCarriedDeps = 0
  /// - parallelClassification = Likely
  return LoopMetadataAttr::get(
      ctx, builder.getBoolAttr(true), builder.getBoolAttr(false), nullptr,
      base.getTripCount(), base.getNestingLevel(), builder.getBoolAttr(false),
      builder.getI64IntegerAttr(0),
      builder.getI64IntegerAttr(
          static_cast<int64_t>(ParallelClassification::Likely)),
      base.getLocationKey());
}
