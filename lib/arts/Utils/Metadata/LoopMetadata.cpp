///==========================================================================///
/// File: LoopMetadata.cpp
/// Implementation of LoopMetadata class for loop metadata management.
///==========================================================================///

#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"

using namespace mlir;
using namespace mlir::arts;

///===-------------------------------------------------------------===///
/// Enums
//===-------------------------------------------------------------===///
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

const char *LoopMetadata::dataMovementToString(DataMovement pattern) const {
  switch (pattern) {
  case DataMovement::Streaming:
    return "streaming";
  case DataMovement::Tiled:
    return "tiled";
  case DataMovement::Random:
    return "random";
  case DataMovement::Stencil:
    return "stencil";
  case DataMovement::Gather:
    return "gather";
  case DataMovement::Scatter:
    return "scatter";
  case DataMovement::Unknown:
    return "unknown";
  }
  return "unknown";
}

LoopMetadata::DataMovement
LoopMetadata::stringToDataMovement(llvm::StringRef str) const {
  return llvm::StringSwitch<DataMovement>(str)
      .Case("streaming", DataMovement::Streaming)
      .Case("tiled", DataMovement::Tiled)
      .Case("random", DataMovement::Random)
      .Case("stencil", DataMovement::Stencil)
      .Case("gather", DataMovement::Gather)
      .Case("scatter", DataMovement::Scatter)
      .Default(DataMovement::Unknown);
}

const char *LoopMetadata::partitioningToString(Partitioning strategy) const {
  switch (strategy) {
  case Partitioning::Block:
    return "block";
  case Partitioning::Dynamic:
    return "dynamic";
  case Partitioning::Guided:
    return "guided";
  case Partitioning::Auto:
    return "auto";
  case Partitioning::Unknown:
    return "unknown";
  }
  return "unknown";
}

LoopMetadata::Partitioning
LoopMetadata::stringToPartitioning(llvm::StringRef str) const {
  return llvm::StringSwitch<Partitioning>(str)
      .Case("block", Partitioning::Block)
      .Case("dynamic", Partitioning::Dynamic)
      .Case("guided", Partitioning::Guided)
      .Case("auto", Partitioning::Auto)
      .Default(Partitioning::Unknown);
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

  /// Memory access patterns
  readCount = getIntFromAttr(attr.getReadCount()).value_or(0);
  writeCount = getIntFromAttr(attr.getWriteCount()).value_or(0);

  /// Loop structure information
  tripCount = getIntFromAttr(attr.getTripCount()).value_or(0);
  nestingLevel = getIntFromAttr(attr.getNestingLevel()).value_or(0);

  /// Access pattern analysis
  hasUniformStride = getBoolFromAttr(attr.getHasUniformStride());
  hasGatherScatter = getBoolFromAttr(attr.getHasGatherScatter());
  dataMovementPattern =
      DataMovement(getIntFromAttr(attr.getDataMovementPattern()).value_or(0));

  /// Partitioning hints
  suggestedPartitioning =
      Partitioning(getIntFromAttr(attr.getSuggestedPartitioning()).value_or(0));
  suggestedChunkSize = getIntFromAttr(attr.getSuggestedChunkSize()).value_or(0);
  memoryFootprintPerIter =
      getIntFromAttr(attr.getMemoryFootprintPerIter()).value_or(0);

  /// Dependency information
  hasInterIterationDeps = getBoolFromAttr(attr.getHasInterIterationDeps());
  dependenceDistance = getIntFromAttr(attr.getDependenceDistance()).value_or(0);

  auto assignOptionalInt = [&](const std::optional<int64_t> &value,
                               std::optional<int64_t> &target) {
    if (value)
      target = *value;
    else
      target.reset();
  };

  assignOptionalInt(getIntFromAttr(attr.getMemrefCount()), memrefCount);
  assignOptionalInt(getIntFromAttr(attr.getReadOnlyMemrefCount()),
                    readOnlyMemrefCount);
  assignOptionalInt(getIntFromAttr(attr.getWriteOnlyMemrefCount()),
                    writeOnlyMemrefCount);
  assignOptionalInt(getIntFromAttr(attr.getReadWriteMemrefCount()),
                    readWriteMemrefCount);
  assignOptionalInt(getIntFromAttr(attr.getMemrefsWithLoopCarriedDeps()),
                    memrefsWithLoopCarriedDeps);
  assignOptionalInt(getIntFromAttr(attr.getPoorTemporalLocalityMemrefs()),
                    poorTemporalLocalityMemrefCount);
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
  readCount = json.getInteger(AttrNames::LoopMetadata::ReadCount).value_or(0);
  writeCount = json.getInteger(AttrNames::LoopMetadata::WriteCount).value_or(0);
  tripCount = json.getInteger(AttrNames::LoopMetadata::TripCount).value_or(0);
  nestingLevel =
      json.getInteger(AttrNames::LoopMetadata::NestingLevel).value_or(0);
  hasUniformStride = json.getBoolean(AttrNames::LoopMetadata::HasUniformStride)
                         .value_or(false);
  hasGatherScatter = json.getBoolean(AttrNames::LoopMetadata::HasGatherScatter)
                         .value_or(false);
  dataMovementPattern =
      DataMovement(json.getInteger(AttrNames::LoopMetadata::DataMovementPattern)
                       .value_or(0));
  suggestedPartitioning = Partitioning(
      json.getInteger(AttrNames::LoopMetadata::SuggestedPartitioning)
          .value_or(0));
  suggestedChunkSize =
      json.getInteger(AttrNames::LoopMetadata::SuggestedChunkSize).value_or(0);
  memoryFootprintPerIter =
      json.getInteger(AttrNames::LoopMetadata::MemoryFootprintPerIter)
          .value_or(0);
  hasInterIterationDeps =
      json.getBoolean(AttrNames::LoopMetadata::HasInterIterationDeps)
          .value_or(false);
  dependenceDistance =
      json.getInteger(AttrNames::LoopMetadata::DependenceDistance).value_or(0);
  memrefCount =
      json.getInteger(AttrNames::LoopMetadata::MemrefCount).value_or(0);
  readOnlyMemrefCount =
      json.getInteger(AttrNames::LoopMetadata::ReadOnlyMemrefCount).value_or(0);
  writeOnlyMemrefCount =
      json.getInteger(AttrNames::LoopMetadata::WriteOnlyMemrefCount)
          .value_or(0);
  readWriteMemrefCount =
      json.getInteger(AttrNames::LoopMetadata::ReadWriteMemrefCount)
          .value_or(0);
  memrefsWithLoopCarriedDeps =
      json.getInteger(AttrNames::LoopMetadata::MemrefsWithLoopCarriedDeps)
          .value_or(0);
  poorTemporalLocalityMemrefCount =
      json.getInteger(AttrNames::LoopMetadata::PoorTemporalLocalityMemrefs)
          .value_or(0);
  parallelClassification = ParallelClassification::Unknown;
  if (auto i = json.getInteger(AttrNames::LoopMetadata::ParallelClassification))
    parallelClassification = static_cast<ParallelClassification>(*i);
  locationMetadata.fromKey(
      json.getString(AttrNames::LoopMetadata::LocationKey).value_or(""));
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
  json[AttrNames::LoopMetadata::ReadCount.str()] = readCount;
  json[AttrNames::LoopMetadata::WriteCount.str()] = writeCount;
  json[AttrNames::LoopMetadata::TripCount.str()] = tripCount;
  json[AttrNames::LoopMetadata::NestingLevel.str()] = nestingLevel;
  json[AttrNames::LoopMetadata::HasUniformStride.str()] = hasUniformStride;
  json[AttrNames::LoopMetadata::HasGatherScatter.str()] = hasGatherScatter;
  json[AttrNames::LoopMetadata::DataMovementPattern.str()] =
      static_cast<int64_t>(dataMovementPattern.value_or(DataMovement::Unknown));
  json[AttrNames::LoopMetadata::SuggestedPartitioning.str()] =
      static_cast<int64_t>(
          suggestedPartitioning.value_or(Partitioning::Unknown));
  json[AttrNames::LoopMetadata::SuggestedChunkSize.str()] = suggestedChunkSize;
  json[AttrNames::LoopMetadata::MemoryFootprintPerIter.str()] =
      memoryFootprintPerIter;
  json[AttrNames::LoopMetadata::HasInterIterationDeps.str()] =
      hasInterIterationDeps;
  json[AttrNames::LoopMetadata::DependenceDistance.str()] = dependenceDistance;
  json[AttrNames::LoopMetadata::MemrefCount.str()] = memrefCount.value_or(0);
  json[AttrNames::LoopMetadata::ReadOnlyMemrefCount.str()] =
      readOnlyMemrefCount.value_or(0);
  json[AttrNames::LoopMetadata::WriteOnlyMemrefCount.str()] =
      writeOnlyMemrefCount.value_or(0);
  json[AttrNames::LoopMetadata::ReadWriteMemrefCount.str()] =
      readWriteMemrefCount.value_or(0);
  json[AttrNames::LoopMetadata::MemrefsWithLoopCarriedDeps.str()] =
      memrefsWithLoopCarriedDeps.value_or(0);
  json[AttrNames::LoopMetadata::PoorTemporalLocalityMemrefs.str()] =
      poorTemporalLocalityMemrefCount.value_or(0);
  if (parallelClassification)
    json[AttrNames::LoopMetadata::ParallelClassification.str()] =
        static_cast<int64_t>(*parallelClassification);
  json[AttrNames::LoopMetadata::LocationKey.str()] =
      locationMetadata.toString();
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
      /// Memory access patterns
      builder.getI64IntegerAttr(readCount),
      builder.getI64IntegerAttr(writeCount),
      /// Loop structure information
      toIntAttr(tripCount), toIntAttr(nestingLevel),
      /// Access pattern analysis
      toBoolAttr(hasUniformStride), toBoolAttr(hasGatherScatter),
      builder.getI64IntegerAttr(static_cast<int64_t>(
          dataMovementPattern.value_or(DataMovement::Unknown))),
      /// Partitioning hints
      builder.getI64IntegerAttr(static_cast<int64_t>(
          suggestedPartitioning.value_or(Partitioning::Unknown))),
      toIntAttr(suggestedChunkSize), toIntAttr(memoryFootprintPerIter),
      /// Dependency information
      toBoolAttr(hasInterIterationDeps), toIntAttr(dependenceDistance),
      toIntAttr(memrefCount), toIntAttr(readOnlyMemrefCount),
      toIntAttr(writeOnlyMemrefCount), toIntAttr(readWriteMemrefCount),
      toIntAttr(memrefsWithLoopCarriedDeps),
      toIntAttr(poorTemporalLocalityMemrefCount),
      toEnumAttr(parallelClassification, ParallelClassification::Unknown),
      /// Source location
      builder.getStringAttr(locationMetadata.toString()));
}
