///===----------------------------------------------------------------------===///
/// MemrefMetadata.cpp - Memref/Allocation Metadata Implementation
///===----------------------------------------------------------------------===///

#include "arts/utils/metadata/MemrefMetadata.h"
#include "arts/ArtsDialect.h"
#include "arts/utils/metadata/Metadata.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "llvm/Support/JSON.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(memref_metadata);

using namespace mlir;
using namespace mlir::arts;

MemrefMetadata::MemrefMetadata(Operation *allocOp)
    : ArtsMetadata(allocOp), allocOp_(allocOp) {
  assert(allocOp_ && "Allocation operation must not be null");
}

///===----------------------------------------------------------------------===///
/// Enum Serialization Helpers
///===----------------------------------------------------------------------===///

std::string MemrefMetadata::accessPatternToString(AccessPatternType pattern) {
  switch (pattern) {
  case AccessPatternType::Sequential:
    return "sequential";
  case AccessPatternType::Strided:
    return "strided";
  case AccessPatternType::Stencil:
    return "stencil";
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
  if (str == "stencil")
    return AccessPatternType::Stencil;
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

std::string
MemrefMetadata::dimAccessPatternToString(DimAccessPatternType pattern) {
  switch (pattern) {
  case DimAccessPatternType::Constant:
    return "constant";
  case DimAccessPatternType::UnitStride:
    return "unit_stride";
  case DimAccessPatternType::Affine:
    return "affine";
  case DimAccessPatternType::NonAffine:
    return "non_affine";
  case DimAccessPatternType::Unknown:
    return "unknown";
  }
  return "unknown";
}

std::optional<MemrefMetadata::DimAccessPatternType>
MemrefMetadata::stringToDimAccessPattern(llvm::StringRef str) {
  if (str == "constant")
    return DimAccessPatternType::Constant;
  if (str == "unit_stride")
    return DimAccessPatternType::UnitStride;
  if (str == "affine")
    return DimAccessPatternType::Affine;
  if (str == "non_affine")
    return DimAccessPatternType::NonAffine;
  if (str == "unknown")
    return DimAccessPatternType::Unknown;
  return std::nullopt;
}

///===----------------------------------------------------------------------===///
/// ArtsMetadata Interface Implementation
///===----------------------------------------------------------------------===///

bool MemrefMetadata::importFromOp() {
  importIdFromOp();
  /// Get the metadata attribute
  MemrefMetadataAttr attr =
      op_->getAttrOfType<MemrefMetadataAttr>(getMetadataName());
  if (!attr)
    return false;

  /// Import basic type information
  if (auto rankVal = getIntFromAttr(attr.getRank()))
    rank = *rankVal;
  allocationId = attr.getAllocationId().str();

  /// Import access pattern analysis
  accessStats.totalAccesses = getIntFromAttr(attr.getTotalAccesses());
  accessStats.readCount = getIntFromAttr(attr.getReadCount());
  accessStats.writeCount = getIntFromAttr(attr.getWriteCount());
  allAccessesAffine = getBoolFromAttr(attr.getAllAccessesAffine());
  hasAffineAccesses = getBoolFromAttr(attr.getHasAffineAccesses());
  hasNonAffineAccesses = getBoolFromAttr(attr.getHasNonAffineAccesses());

  /// Import memory characteristics
  if (auto memoryFootprintVal = getIntFromAttr(attr.getMemoryFootprint()))
    memoryFootprint = *memoryFootprintVal;
  isFlattenedArrayFlag = getBoolFromAttr(attr.getIsFlattenedArray());

  /// Import canonicalization metadata
  shouldCanonicalizeFlag = getBoolFromAttr(attr.getShouldCanonicalize());
  isCanonicalizedFlag = getBoolFromAttr(attr.getIsCanonicalized());

  /// Import lifetime information
  firstUseId = getIntFromAttr(attr.getFirstUseId());
  lastUseId = getIntFromAttr(attr.getLastUseId());

  /// Import extended access characterization
  hasUniformAccess = getBoolFromAttr(attr.getHasUniformAccess());
  hasStrideOneAccess = getBoolFromAttr(attr.getHasStrideOneAccess());
  if (auto value = getIntFromAttr(attr.getDominantAccessPattern())) {
    dominantAccessPattern = static_cast<AccessPatternType>(value.value());
    ARTS_DEBUG("importFromOp: dominantAccessPattern = "
               << accessPatternToString(*dominantAccessPattern)
               << " (enum value: " << static_cast<int>(*dominantAccessPattern)
               << ")");
  } else {
    dominantAccessPattern.reset();
    ARTS_DEBUG("importFromOp: dominantAccessPattern is not set");
  }
  if (auto nestingDepthVal = getIntFromAttr(attr.getNestingDepth()))
    nestingDepth = *nestingDepthVal;
  accessedInParallelLoop = getBoolFromAttr(attr.getAccessedInParallelLoop());
  hasLoopCarriedDeps = getBoolFromAttr(attr.getHasLoopCarriedDeps());
  if (auto reuseDistanceVal = getIntFromAttr(attr.getReuseDistance()))
    reuseDistance = *reuseDistanceVal;
  /// Backward compatibility: convert old bool to new enum
  if (attr.getHasGoodSpatialLocality()) {
    spatialLocality = getBoolFromAttr(attr.getHasGoodSpatialLocality())
                          ? SpatialLocalityLevel::Good
                          : SpatialLocalityLevel::Poor;
  }
  if (auto dimAttr = attr.getDimAccessPatterns()) {
    dimAccessPatterns.clear();
    for (Attribute entry : dimAttr.getValue()) {
      if (auto intAttr = dyn_cast<IntegerAttr>(entry))
        dimAccessPatterns.push_back(
            static_cast<DimAccessPatternType>(intAttr.getInt()));
    }
  }
  if (auto est = getIntFromAttr(attr.getEstimatedAccessBytes()))
    estimatedAccessBytes = *est;

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

  /// Helper to convert AccessPatternType enum to IntegerAttr
  auto toAccessPatternAttr =
      [&](const std::optional<AccessPatternType> &v) -> IntegerAttr {
    return v ? builder.getI64IntegerAttr(static_cast<int64_t>(*v))
             : IntegerAttr();
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

  ArrayAttr dimPatternAttr;
  if (!dimAccessPatterns.empty()) {
    SmallVector<Attribute> patterns;
    patterns.reserve(dimAccessPatterns.size());
    for (DimAccessPatternType pattern : dimAccessPatterns)
      patterns.push_back(
          builder.getI64IntegerAttr(static_cast<int64_t>(pattern)));
    dimPatternAttr = builder.getArrayAttr(patterns);
  }

  /// Build the MemrefMetadataAttr
  return MemrefMetadataAttr::get(
      ctx,
      /// Basic type information
      toIntAttr(rank), builder.getStringAttr(allocationId),
      /// Access pattern analysis
      toIntAttr(accessStats.totalAccesses), toIntAttr(accessStats.readCount),
      toIntAttr(accessStats.writeCount), toBoolAttr(allAccessesAffine),
      toBoolAttr(hasAffineAccesses), toBoolAttr(hasNonAffineAccesses),
      /// Memory characteristics
      toIntAttr(memoryFootprint), toBoolAttr(isFlattenedArrayFlag),
      /// Canonicalization metadata
      toBoolAttr(shouldCanonicalizeFlag), toBoolAttr(isCanonicalizedFlag),
      /// Lifetime information
      toIntAttr(firstUseId), toIntAttr(lastUseId),
      /// Extended access characterization
      toBoolAttr(hasUniformAccess), toBoolAttr(hasStrideOneAccess),
      toAccessPatternAttr(dominantAccessPattern), toIntAttr(nestingDepth),
      toBoolAttr(accessedInParallelLoop), toBoolAttr(hasLoopCarriedDeps),
      toIntAttr(reuseDistance), spatialLocalityToBool(), dimPatternAttr,
      toIntAttr(estimatedAccessBytes));
}

void MemrefMetadata::exportToJson(llvm::json::Object &json) const {
  exportIdToJson(json);
  auto setI64 = [&](const std::optional<int64_t> &v, llvm::StringRef key) {
    if (v)
      json[key] = static_cast<int64_t>(*v);
  };
  auto setBool = [&](const std::optional<bool> &v, llvm::StringRef key) {
    if (v)
      json[key] = static_cast<bool>(*v);
  };
  setI64(rank, AttrNames::MemrefMetadata::Rank);
  if (!allocationId.empty())
    json[AttrNames::MemrefMetadata::AllocationId.str()] = allocationId;
  accessStats.exportToJson(json);
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
        static_cast<int64_t>(*dominantAccessPattern);
  setI64(nestingDepth, AttrNames::MemrefMetadata::NestingDepth);
  setBool(accessedInParallelLoop,
          AttrNames::MemrefMetadata::AccessedInParallelLoop);
  setBool(hasLoopCarriedDeps, AttrNames::MemrefMetadata::HasLoopCarriedDeps);
  setI64(reuseDistance, AttrNames::MemrefMetadata::ReuseDistance);
  if (temporalLocality)
    json[AttrNames::MemrefMetadata::TemporalLocality.str()] =
        static_cast<int64_t>(*temporalLocality);
  if (spatialLocality)
    json[AttrNames::MemrefMetadata::HasGoodSpatialLocality.str()] =
        static_cast<int64_t>(*spatialLocality);
  if (!dimAccessPatterns.empty()) {
    llvm::json::Array dims;
    for (DimAccessPatternType pattern : dimAccessPatterns)
      dims.emplace_back(static_cast<int64_t>(pattern));
    json[AttrNames::MemrefMetadata::DimAccessPatterns.str()] = std::move(dims);
  }
  setI64(estimatedAccessBytes, AttrNames::MemrefMetadata::EstimatedAccessBytes);
  /// Export lifetime information
  if (firstUseId)
    json[AttrNames::MemrefMetadata::FirstUseId.str()] =
        static_cast<int64_t>(*firstUseId);
  if (lastUseId)
    json[AttrNames::MemrefMetadata::LastUseId.str()] =
        static_cast<int64_t>(*lastUseId);
}

void MemrefMetadata::importFromJson(const llvm::json::Object &json) {
  importIdFromJson(json);
  auto getI64 = [&](llvm::StringRef key, std::optional<int64_t> &out) {
    if (auto v = json.getInteger(key))
      out = static_cast<int64_t>(*v);
  };
  auto getBool = [&](llvm::StringRef key, std::optional<bool> &out) {
    if (auto v = json.getBoolean(key))
      out = static_cast<bool>(*v);
  };
  getI64(AttrNames::MemrefMetadata::Rank, rank);
  if (auto str = json.getString(AttrNames::MemrefMetadata::AllocationId))
    allocationId = str->str();
  accessStats.importFromJson(json);
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
  if (auto v = json.getInteger(AttrNames::MemrefMetadata::FirstUseId))
    firstUseId = static_cast<int64_t>(*v);
  if (auto v = json.getInteger(AttrNames::MemrefMetadata::LastUseId))
    lastUseId = static_cast<int64_t>(*v);

  getBool(AttrNames::MemrefMetadata::HasUniformAccess, hasUniformAccess);
  getBool(AttrNames::MemrefMetadata::HasStrideOneAccess, hasStrideOneAccess);
  if (auto i =
          json.getInteger(AttrNames::MemrefMetadata::DominantAccessPattern))
    dominantAccessPattern = static_cast<AccessPatternType>(*i);
  getI64(AttrNames::MemrefMetadata::NestingDepth, nestingDepth);
  getBool(AttrNames::MemrefMetadata::AccessedInParallelLoop,
          accessedInParallelLoop);
  getBool(AttrNames::MemrefMetadata::HasLoopCarriedDeps, hasLoopCarriedDeps);
  getI64(AttrNames::MemrefMetadata::ReuseDistance, reuseDistance);

  /// Import temporal locality
  if (auto i = json.getInteger(AttrNames::MemrefMetadata::TemporalLocality))
    temporalLocality = static_cast<TemporalLocalityLevel>(*i);

  /// Import spatial locality
  if (auto i =
          json.getInteger(AttrNames::MemrefMetadata::HasGoodSpatialLocality))
    spatialLocality = static_cast<SpatialLocalityLevel>(*i);
  /// Backward compatibility: convert old bool format to enum
  else if (auto b = json.getBoolean(
               AttrNames::MemrefMetadata::HasGoodSpatialLocality))
    spatialLocality =
        *b ? SpatialLocalityLevel::Good : SpatialLocalityLevel::Poor;

  if (auto arr = json.getArray(AttrNames::MemrefMetadata::DimAccessPatterns)) {
    dimAccessPatterns.clear();
    for (const llvm::json::Value &entry : *arr) {
      if (auto i = entry.getAsInteger())
        dimAccessPatterns.push_back(static_cast<DimAccessPatternType>(*i));
    }
  }

  getI64(AttrNames::MemrefMetadata::EstimatedAccessBytes, estimatedAccessBytes);
}
