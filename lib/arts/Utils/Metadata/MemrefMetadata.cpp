///===----------------------------------------------------------------------===///
// MemrefMetadata.cpp - Memref/Allocation Metadata Implementation
///===----------------------------------------------------------------------===///

#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "llvm/Support/JSON.h"

using namespace mlir;
using namespace mlir::arts;

MemrefMetadata::MemrefMetadata(Operation *allocOp)
    : ArtsMetadata(allocOp), allocOp_(allocOp) {
  assert(allocOp_ && "Allocation operation must not be null");
}

///===----------------------------------------------------------------------===///
// Enum Serialization Helpers
///===----------------------------------------------------------------------===///

std::string MemrefMetadata::accessPatternToString(AccessPatternType pattern) {
  switch (pattern) {
  case AccessPatternType::Sequential:
    return "sequential";
  case AccessPatternType::Strided:
    return "strided";
  case AccessPatternType::GatherScatter:
    return "gather_scatter";
  case AccessPatternType::Random:
    return "random";
  case AccessPatternType::Unknown:
    return "unknown";
  }
  return "unknown";
}

std::optional<AccessPatternType>
MemrefMetadata::stringToAccessPattern(llvm::StringRef str) {
  if (str == "sequential")
    return AccessPatternType::Sequential;
  if (str == "strided")
    return AccessPatternType::Strided;
  if (str == "gather_scatter")
    return AccessPatternType::GatherScatter;
  if (str == "random")
    return AccessPatternType::Random;
  if (str == "unknown")
    return AccessPatternType::Unknown;
  return std::nullopt;
}

std::string
MemrefMetadata::temporalLocalityToString(TemporalLocalityLevel level) {
  switch (level) {
  case TemporalLocalityLevel::Excellent:
    return "excellent";
  case TemporalLocalityLevel::Good:
    return "good";
  case TemporalLocalityLevel::Fair:
    return "fair";
  case TemporalLocalityLevel::Poor:
    return "poor";
  case TemporalLocalityLevel::Unknown:
    return "unknown";
  }
  return "unknown";
}

std::optional<TemporalLocalityLevel>
MemrefMetadata::stringToTemporalLocality(llvm::StringRef str) {
  if (str == "excellent")
    return TemporalLocalityLevel::Excellent;
  if (str == "good")
    return TemporalLocalityLevel::Good;
  if (str == "fair")
    return TemporalLocalityLevel::Fair;
  if (str == "poor")
    return TemporalLocalityLevel::Poor;
  if (str == "unknown")
    return TemporalLocalityLevel::Unknown;
  return std::nullopt;
}

std::string
MemrefMetadata::spatialLocalityToString(SpatialLocalityLevel level) {
  switch (level) {
  case SpatialLocalityLevel::Excellent:
    return "excellent";
  case SpatialLocalityLevel::Good:
    return "good";
  case SpatialLocalityLevel::Fair:
    return "fair";
  case SpatialLocalityLevel::Poor:
    return "poor";
  case SpatialLocalityLevel::Unknown:
    return "unknown";
  }
  return "unknown";
}

std::optional<SpatialLocalityLevel>
MemrefMetadata::stringToSpatialLocality(llvm::StringRef str) {
  if (str == "excellent")
    return SpatialLocalityLevel::Excellent;
  if (str == "good")
    return SpatialLocalityLevel::Good;
  if (str == "fair")
    return SpatialLocalityLevel::Fair;
  if (str == "poor")
    return SpatialLocalityLevel::Poor;
  if (str == "unknown")
    return SpatialLocalityLevel::Unknown;
  return std::nullopt;
}

///===----------------------------------------------------------------------===///
// ArtsMetadata Interface Implementation
///===----------------------------------------------------------------------===///

bool MemrefMetadata::importFromOp() {
  /// Get the metadata attribute
  MemrefMetadataAttr attr =
      op_->getAttrOfType<MemrefMetadataAttr>(getMetadataName());
  if (!attr)
    return false;

  /// Import basic type information
  if (attr.getRank())
    rank = attr.getRank().getInt();
  allocationId = attr.getAllocationId().str();

  /// Import access pattern analysis
  if (attr.getTotalAccesses())
    totalAccesses = attr.getTotalAccesses().getInt();
  if (attr.getReadCount())
    readCount = attr.getReadCount().getInt();
  if (attr.getWriteCount())
    writeCount = attr.getWriteCount().getInt();
  if (attr.getReadWriteRatio())
    readWriteRatio = attr.getReadWriteRatio().getValueAsDouble();
  if (attr.getAllAccessesAffine())
    allAccessesAffine = attr.getAllAccessesAffine().getValue();
  if (attr.getHasAffineAccesses())
    hasAffineAccesses = attr.getHasAffineAccesses().getValue();
  if (attr.getHasNonAffineAccesses())
    hasNonAffineAccesses = attr.getHasNonAffineAccesses().getValue();

  /// Import memory characteristics
  if (attr.getMemoryFootprint())
    memoryFootprint = attr.getMemoryFootprint().getInt();
  if (attr.getIsFlattenedArray())
    isFlattenedArrayFlag = attr.getIsFlattenedArray().getValue();

  /// Import canonicalization metadata
  if (attr.getShouldCanonicalize())
    shouldCanonicalizeFlag = attr.getShouldCanonicalize().getValue();
  if (attr.getIsCanonicalized())
    isCanonicalizedFlag = attr.getIsCanonicalized().getValue();

  /// Import lifetime information
  if (attr.getFirstUseLocationKey())
    firstUseLocation =
        LocationMetadata::fromKey(attr.getFirstUseLocationKey().getValue());
  if (attr.getLastUseLocationKey())
    lastUseLocation =
        LocationMetadata::fromKey(attr.getLastUseLocationKey().getValue());

  /// Import extended access characterization
  if (attr.getHasUniformAccess())
    hasUniformAccess = attr.getHasUniformAccess().getValue();
  if (attr.getHasStrideOneAccess())
    hasStrideOneAccess = attr.getHasStrideOneAccess().getValue();
  if (attr.getDominantAccessPattern())
    dominantAccessPattern =
        stringToAccessPattern(attr.getDominantAccessPattern().getValue());
  if (attr.getNestingDepth())
    nestingDepth = attr.getNestingDepth().getInt();
  if (attr.getAccessedInParallelLoop())
    accessedInParallelLoop = attr.getAccessedInParallelLoop().getValue();
  if (attr.getHasLoopCarriedDeps())
    hasLoopCarriedDeps = attr.getHasLoopCarriedDeps().getValue();
  if (attr.getReuseDistance())
    reuseDistance = attr.getReuseDistance().getInt();
  /// Backward compatibility: convert old bool to new enum
  if (attr.getHasGoodSpatialLocality()) {
    spatialLocality = attr.getHasGoodSpatialLocality().getValue()
                          ? SpatialLocalityLevel::Good
                          : SpatialLocalityLevel::Poor;
  }

  return true;
}

Attribute MemrefMetadata::getMetadataAttr() const {
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

  /// Helper to convert optional double to FloatAttr
  auto toFloatAttr = [&](const std::optional<double> &v) -> FloatAttr {
    return v ? builder.getF64FloatAttr(*v) : FloatAttr();
  };

  /// Helper to convert optional string to StringAttr
  auto toStrAttr = [&](const std::optional<std::string> &v) -> StringAttr {
    return (v && !v->empty()) ? builder.getStringAttr(*v) : StringAttr();
  };

  /// Helper to convert LocationMetadata to StringAttr (key)
  auto toLocKeyAttr = [&](const LocationMetadata &loc) -> StringAttr {
    return loc.isValid() ? builder.getStringAttr(loc.toString()) : StringAttr();
  };

  /// Helper to convert AccessPatternType enum to StringAttr
  auto toAccessPatternAttr =
      [&](const std::optional<AccessPatternType> &v) -> StringAttr {
    return v ? builder.getStringAttr(accessPatternToString(*v)) : StringAttr();
  };

  /// Helper to convert SpatialLocalityLevel enum to BoolAttr (for backward
  /// compatibility)
  auto spatialLocalityToBool = [&]() -> BoolAttr {
    if (!spatialLocality)
      return BoolAttr();
    return builder.getBoolAttr(*spatialLocality ==
                                   SpatialLocalityLevel::Excellent ||
                               *spatialLocality == SpatialLocalityLevel::Good);
  };

  /// Build the MemrefMetadataAttr
  return MemrefMetadataAttr::get(
      ctx,
      /// Basic type information
      toIntAttr(rank), builder.getStringAttr(allocationId),
      /// Access pattern analysis
      toIntAttr(totalAccesses), toIntAttr(readCount), toIntAttr(writeCount),
      toFloatAttr(readWriteRatio), toBoolAttr(allAccessesAffine),
      toBoolAttr(hasAffineAccesses), toBoolAttr(hasNonAffineAccesses),
      /// Memory characteristics
      toIntAttr(memoryFootprint), toBoolAttr(isFlattenedArrayFlag),
      /// Canonicalization metadata
      toBoolAttr(shouldCanonicalizeFlag), toBoolAttr(isCanonicalizedFlag),
      /// Lifetime information
      toLocKeyAttr(firstUseLocation), toLocKeyAttr(lastUseLocation),
      /// Extended access characterization
      toBoolAttr(hasUniformAccess), toBoolAttr(hasStrideOneAccess),
      toAccessPatternAttr(dominantAccessPattern), toIntAttr(nestingDepth),
      toBoolAttr(accessedInParallelLoop), toBoolAttr(hasLoopCarriedDeps),
      toIntAttr(reuseDistance), spatialLocalityToBool());
}

void MemrefMetadata::exportToJson(llvm::json::Object &json) const {
  auto setI64 = [&](const std::optional<int64_t> &v, llvm::StringRef key) {
    if (v)
      json[key] = static_cast<int64_t>(*v);
  };
  auto setBool = [&](const std::optional<bool> &v, llvm::StringRef key) {
    if (v)
      json[key] = static_cast<bool>(*v);
  };
  auto setF64 = [&](const std::optional<double> &v, llvm::StringRef key) {
    if (v)
      json[key] = static_cast<double>(*v);
  };
  auto setStr = [&](const std::string &v, llvm::StringRef key) {
    if (!v.empty())
      json[key] = v;
  };

  setI64(rank, AttrNames::MemrefMetadata::Rank);
  if (!allocationId.empty())
    json[AttrNames::MemrefMetadata::AllocationId.str()] = allocationId;
  setI64(totalAccesses, AttrNames::MemrefMetadata::TotalAccesses);
  setI64(readCount, AttrNames::MemrefMetadata::ReadCount);
  setI64(writeCount, AttrNames::MemrefMetadata::WriteCount);
  setF64(readWriteRatio, AttrNames::MemrefMetadata::ReadWriteRatio);
  setBool(allAccessesAffine, AttrNames::MemrefMetadata::AllAccessesAffine);
  setBool(hasAffineAccesses, AttrNames::MemrefMetadata::HasAffineAccesses);
  setBool(hasNonAffineAccesses,
          AttrNames::MemrefMetadata::HasNonAffineAccesses);
  setI64(memoryFootprint, AttrNames::MemrefMetadata::MemoryFootprint);
  setBool(isFlattenedArrayFlag, AttrNames::MemrefMetadata::IsFlattenedArray);
  setBool(shouldCanonicalizeFlag,
          AttrNames::MemrefMetadata::ShouldCanonicalize);
  setBool(isCanonicalizedFlag, AttrNames::MemrefMetadata::IsCanonicalized);
  setBool(hasUniformAccess, AttrNames::MemrefMetadata::HasUniformAccess);
  setBool(hasStrideOneAccess, AttrNames::MemrefMetadata::HasStrideOneAccess);
  if (dominantAccessPattern)
    json[AttrNames::MemrefMetadata::DominantAccessPattern.str()] =
        accessPatternToString(*dominantAccessPattern);
  setI64(nestingDepth, AttrNames::MemrefMetadata::NestingDepth);
  setBool(accessedInParallelLoop,
          AttrNames::MemrefMetadata::AccessedInParallelLoop);
  setBool(hasLoopCarriedDeps, AttrNames::MemrefMetadata::HasLoopCarriedDeps);
  setI64(reuseDistance, AttrNames::MemrefMetadata::ReuseDistance);
  if (temporalLocality)
    json["temporal_locality"] = temporalLocalityToString(*temporalLocality);
  if (spatialLocality)
    json[AttrNames::MemrefMetadata::HasGoodSpatialLocality.str()] =
        spatialLocalityToString(*spatialLocality);
  /// Export lifetime information
  if (firstUseLocation.isValid())
    json["first_use_location"] = firstUseLocation.toString().str();
  if (lastUseLocation.isValid())
    json["last_use_location"] = lastUseLocation.toString().str();
}

void MemrefMetadata::importFromJson(const llvm::json::Object &json) {
  auto getI64 = [&](llvm::StringRef key, std::optional<int64_t> &out) {
    if (auto v = json.getInteger(key))
      out = static_cast<int64_t>(*v);
  };
  auto getBool = [&](llvm::StringRef key, std::optional<bool> &out) {
    if (auto v = json.getBoolean(key))
      out = static_cast<bool>(*v);
  };
  auto getF64 = [&](llvm::StringRef key, std::optional<double> &out) {
    if (auto v = json.getNumber(key))
      out = static_cast<double>(*v);
  };
  auto getStr = [&](llvm::StringRef key, std::string &out) {
    if (auto v = json.getString(key))
      out = v->str();
  };

  getI64(AttrNames::MemrefMetadata::Rank, rank);
  if (auto str = json.getString(AttrNames::MemrefMetadata::AllocationId))
    allocationId = str->str();
  getI64(AttrNames::MemrefMetadata::TotalAccesses, totalAccesses);
  getI64(AttrNames::MemrefMetadata::ReadCount, readCount);
  getI64(AttrNames::MemrefMetadata::WriteCount, writeCount);
  getF64(AttrNames::MemrefMetadata::ReadWriteRatio, readWriteRatio);
  getBool(AttrNames::MemrefMetadata::AllAccessesAffine, allAccessesAffine);
  getBool(AttrNames::MemrefMetadata::HasAffineAccesses, hasAffineAccesses);
  getBool(AttrNames::MemrefMetadata::HasNonAffineAccesses,
          hasNonAffineAccesses);
  getI64(AttrNames::MemrefMetadata::MemoryFootprint, memoryFootprint);
  getBool(AttrNames::MemrefMetadata::IsFlattenedArray, isFlattenedArrayFlag);
  getBool(AttrNames::MemrefMetadata::ShouldCanonicalize,
          shouldCanonicalizeFlag);
  getBool(AttrNames::MemrefMetadata::IsCanonicalized, isCanonicalizedFlag);

  /// Import lifetime information
  if (auto *locObj = json.getObject("first_use_location"))
    firstUseLocation.importFromJson(*locObj);
  else if (auto locStr = json.getString("first_use_location"))
    firstUseLocation = LocationMetadata::fromKey(*locStr);

  if (auto *locObj = json.getObject("last_use_location"))
    lastUseLocation.importFromJson(*locObj);
  else if (auto locStr = json.getString("last_use_location"))
    lastUseLocation = LocationMetadata::fromKey(*locStr);

  getBool(AttrNames::MemrefMetadata::HasUniformAccess, hasUniformAccess);
  getBool(AttrNames::MemrefMetadata::HasStrideOneAccess, hasStrideOneAccess);
  if (auto s = json.getString(AttrNames::MemrefMetadata::DominantAccessPattern))
    dominantAccessPattern = stringToAccessPattern(*s);
  getI64(AttrNames::MemrefMetadata::NestingDepth, nestingDepth);
  getBool(AttrNames::MemrefMetadata::AccessedInParallelLoop,
          accessedInParallelLoop);
  getBool(AttrNames::MemrefMetadata::HasLoopCarriedDeps, hasLoopCarriedDeps);
  getI64(AttrNames::MemrefMetadata::ReuseDistance, reuseDistance);

  /// Import temporal locality
  if (auto s = json.getString("temporal_locality"))
    temporalLocality = stringToTemporalLocality(*s);

  /// Import spatial locality (new enum format)
  if (auto s =
          json.getString(AttrNames::MemrefMetadata::HasGoodSpatialLocality))
    spatialLocality = stringToSpatialLocality(*s);
  /// Backward compatibility: convert old bool format to enum
  else if (auto b = json.getBoolean(
               AttrNames::MemrefMetadata::HasGoodSpatialLocality))
    spatialLocality =
        *b ? SpatialLocalityLevel::Good : SpatialLocalityLevel::Poor;
}
