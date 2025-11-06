///==========================================================================
/// File: LoopMetadata.cpp
/// Implementation of LoopMetadata class for loop metadata management.
///==========================================================================

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

///===----------------------------------------------------------------------===///
/// Interface
///===----------------------------------------------------------------------===///
bool LoopMetadata::importFromOp() {
  MLIRContext *ctx = op_->getContext();
  OpBuilder builder(ctx);

  /// Get the metadata attribute
  LoopMetadataAttr attr =
      op_->getAttrOfType<LoopMetadataAttr>(getMetadataName());
  if (!attr)
    return false;

  /// Parallelism analysis
  potentiallyParallel = attr.getPotentiallyParallel().getValue();
  hasReductions = attr.getHasReductions().getValue();
  ArrayAttr reductionKindsAttr = attr.getReductionKinds();
  for (auto kind : reductionKindsAttr.getValue())
    reductionKinds.push_back(
        stringToReductionKind(kind.cast<StringAttr>().str()));

  /// Memory access patterns
  readCount = attr.getReadCount().getInt();
  writeCount = attr.getWriteCount().getInt();

  /// Loop structure information
  tripCount = attr.getTripCount().getInt();
  nestingLevel = attr.getNestingLevel().getInt();

  /// Access pattern analysis
  hasUniformStride = attr.getHasUniformStride().getValue();
  hasGatherScatter = attr.getHasGatherScatter().getValue();
  dataMovementPattern = DataMovement(attr.getDataMovementPattern().getInt());

  /// Partitioning hints
  suggestedPartitioning =
      Partitioning(attr.getSuggestedPartitioning().getInt());
  suggestedChunkSize = attr.getSuggestedChunkSize().getInt();
  memoryFootprintPerIter = attr.getMemoryFootprintPerIter().getInt();

  /// Dependency information
  hasInterIterationDeps = attr.getHasInterIterationDeps().getValue();
  dependenceDistance = attr.getDependenceDistance().getInt();
  return true;
}

void LoopMetadata::importFromJson(const llvm::json::Object &json) {
  potentiallyParallel =
      json.getBoolean(AttrNames::LoopMetadata::PotentiallyParallel)
          .value_or(false);
  hasReductions =
      json.getBoolean(AttrNames::LoopMetadata::HasReductions).value_or(false);
  reductionKinds.clear();
  if (auto *arr = json.getArray(AttrNames::LoopMetadata::ReductionKinds)) {
    for (const auto &e : *arr)
      if (auto s = e.getAsString())
        reductionKinds.push_back(stringToReductionKind(*s));
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
  locationMetadata.fromKey(
      json.getString(AttrNames::LoopMetadata::LocationKey).value_or(""));
}

void LoopMetadata::exportToJson(llvm::json::Object &json) const {
  json[AttrNames::LoopMetadata::PotentiallyParallel.str()] =
      potentiallyParallel;
  json[AttrNames::LoopMetadata::HasReductions.str()] = hasReductions;
  if (!reductionKinds.empty()) {
    llvm::json::Array kinds;
    for (auto k : reductionKinds)
      kinds.push_back(reductionKindToString(k));
    json[AttrNames::LoopMetadata::ReductionKinds.str()] = std::move(kinds);
  }
  json[AttrNames::LoopMetadata::ReadCount.str()] = readCount;
  json[AttrNames::LoopMetadata::WriteCount.str()] = writeCount;
  json[AttrNames::LoopMetadata::TripCount.str()] = tripCount;
  json[AttrNames::LoopMetadata::NestingLevel.str()] = nestingLevel;
  json[AttrNames::LoopMetadata::HasUniformStride.str()] = hasUniformStride;
  json[AttrNames::LoopMetadata::HasGatherScatter.str()] = hasGatherScatter;
  json[AttrNames::LoopMetadata::DataMovementPattern.str()] =
      dataMovementPattern ? dataMovementToString(*dataMovementPattern)
                          : "unknown";
  json[AttrNames::LoopMetadata::SuggestedPartitioning.str()] =
      suggestedPartitioning ? partitioningToString(*suggestedPartitioning)
                            : "unknown";
  json[AttrNames::LoopMetadata::SuggestedChunkSize.str()] = suggestedChunkSize;
  json[AttrNames::LoopMetadata::MemoryFootprintPerIter.str()] =
      memoryFootprintPerIter;
  json[AttrNames::LoopMetadata::HasInterIterationDeps.str()] =
      hasInterIterationDeps;
  json[AttrNames::LoopMetadata::DependenceDistance.str()] = dependenceDistance;
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
  auto toIntAttr = [&](const std::optional<int64_t> &v) -> IntegerAttr {
    return v ? builder.getI64IntegerAttr(*v) : IntegerAttr();
  };

  /// Helper to convert optional bool to BoolAttr
  auto toBoolAttr = [&](const std::optional<bool> &v) -> BoolAttr {
    return v ? builder.getBoolAttr(*v) : BoolAttr();
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
      dataMovementPattern ? builder.getI64IntegerAttr(
                                static_cast<int64_t>(*dataMovementPattern))
                          : IntegerAttr(),
      /// Partitioning hints
      suggestedPartitioning ? builder.getI64IntegerAttr(
                                  static_cast<int64_t>(*suggestedPartitioning))
                            : IntegerAttr(),
      toIntAttr(suggestedChunkSize), toIntAttr(memoryFootprintPerIter),
      /// Dependency information
      toBoolAttr(hasInterIterationDeps), toIntAttr(dependenceDistance),
      /// Source location
      builder.getStringAttr(locationMetadata.toString()));
}
