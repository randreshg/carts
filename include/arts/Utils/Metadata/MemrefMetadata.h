///==========================================================================///
/// File: MemrefMetadata.h
///
/// This file defines the MemrefMetadata class for managing allocation metadata
/// operations. It provides a clean, object-oriented interface for querying and
/// attaching metadata to memref allocations.
///==========================================================================///

#ifndef ARTS_UTILS_MEMREFMETADATA_H
#define ARTS_UTILS_MEMREFMETADATA_H

#include "arts/Utils/Metadata/AccessStats.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "arts/Utils/Metadata/ValueMetadata.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/StringRef.h"
#include <cassert>
#include <optional>
#include <string>

namespace mlir {
namespace arts {

/// Forward declarations
enum class DimensionSourceType;

///===----------------------------------------------------------------------===///
/// Memory Analysis Enums
///===----------------------------------------------------------------------===///

/// Memory access pattern classification
enum class AccessPatternType {
  Sequential,
  Strided,
  GatherScatter,
  Random,
  Unknown
};

/// Temporal locality classification based on reuse distance
enum class TemporalLocalityLevel { Excellent, Good, Fair, Poor, Unknown };

/// Spatial locality classification
enum class SpatialLocalityLevel { Excellent, Good, Fair, Poor, Unknown };

/// Attribute name constants for allocation metadata
/// Used to attach analysis results to memref.alloc/alloca operations
namespace AttrNames {
namespace MemrefMetadata {
using namespace llvm;

/// Basic type information
constexpr StringLiteral Name = "arts.memref";
constexpr StringLiteral Rank = "rank";
constexpr StringLiteral ElementType = "element_type";
constexpr StringLiteral AllocationId = "allocation_id";

/// Access pattern analysis
constexpr StringLiteral TotalAccesses = "total_accesses";
constexpr StringLiteral ReadCount = "read_count";
constexpr StringLiteral WriteCount = "write_count";
constexpr StringLiteral AllAccessesAffine = "all_accesses_affine";
constexpr StringLiteral HasAffineAccesses = "has_affine_accesses";
constexpr StringLiteral HasNonAffineAccesses = "has_non_affine_accesses";

/// Memory characteristics
constexpr StringLiteral MemoryFootprint = "memory_footprint";
constexpr StringLiteral IsFlattenedArray = "is_flattened_array";

/// Canonicalization metadata
constexpr StringLiteral ShouldCanonicalize = "should_canonicalize";
constexpr StringLiteral IsCanonicalized = "is_canonicalized";

/// Extended access characterization
constexpr StringLiteral HasUniformAccess = "has_uniform_access";
constexpr StringLiteral HasStrideOneAccess = "has_stride_one_access";
constexpr StringLiteral DominantAccessPattern = "dominant_access_pattern";
constexpr StringLiteral AccessedInParallelLoop = "accessed_in_parallel_loop";
constexpr StringLiteral HasLoopCarriedDeps = "has_loop_carried_deps";
constexpr StringLiteral ReuseDistance = "reuse_distance";
constexpr StringLiteral TemporalLocality = "temporal_locality";
constexpr StringLiteral HasGoodSpatialLocality = "has_good_spatial_locality";
constexpr StringLiteral AverageStride = "average_stride";
constexpr StringLiteral DimAccessPatterns = "dim_access_patterns";
constexpr StringLiteral EstimatedAccessBytes = "estimated_access_bytes";

/// Lifetime information
constexpr StringLiteral FirstUseId = "first_use_id";
constexpr StringLiteral LastUseId = "last_use_id";

/// Dynamic dimension tracking
constexpr StringLiteral DynamicStrideInfo = "dynamic_stride_info";
constexpr StringLiteral DynamicAllocSizeInfo = "dynamic_alloc_size_info";

/// Shape information
constexpr StringLiteral Shape = "shape";
constexpr StringLiteral InferredDims = "inferred_dims";
constexpr StringLiteral InferredDimSources = "inferred_dim_sources";
constexpr StringLiteral InferredElementType = "inferred_element_type";

/// Nested allocation metadata
constexpr StringLiteral IsNestedAlloc = "is_nested_alloc";
constexpr StringLiteral NestingDepth = "nesting_depth";
} // namespace MemrefMetadata
} // namespace AttrNames

///===--------------------------------------------------------------------===///
/// MemrefMetadata - Manages metadata for memref allocations
///
/// This class provides a clean interface for querying and attaching metadata
/// to memref.alloc and memref.alloca operations. It encapsulates all
/// allocation-related metadata operations in one place.
///
/// Usage:
///   auto memrefMeta = MemrefMetadata(allocOp);
///   if (memrefMeta.hasMetadata()) {
///     auto info = memrefMeta.getInfo();
///     auto footprint = memrefMeta.getMemoryFootprint();
///   }
///===--------------------------------------------------------------------===///
class MemrefMetadata : public ArtsMetadata {
public:
  enum class DimAccessPatternType {
    Unknown = 0,
    Constant = 1,
    UnitStride = 2,
    Affine = 3,
    NonAffine = 4
  };

  ///===-------------------------------------------------------------===///
  /// Attributes
  ///===-------------------------------------------------------------===///
  /// Basic type information
  std::optional<int64_t> rank;
  std::string allocationId;

  /// Access pattern analysis
  AccessStats accessStats;
  std::optional<bool> allAccessesAffine, hasAffineAccesses,
      hasNonAffineAccesses;

  /// Memory characteristics
  std::optional<int64_t> memoryFootprint;
  std::optional<bool> isFlattenedArrayFlag;

  /// Canonicalization metadata
  std::optional<bool> shouldCanonicalizeFlag, isCanonicalizedFlag;

  /// Lifetime information
  std::optional<int64_t> firstUseId, lastUseId;

  /// Extended analysis
  std::optional<bool> hasUniformAccess, hasStrideOneAccess;
  std::optional<AccessPatternType> dominantAccessPattern;
  std::optional<int64_t> nestingDepth;
  std::optional<bool> accessedInParallelLoop, hasLoopCarriedDeps;
  std::optional<int64_t> reuseDistance;
  std::optional<TemporalLocalityLevel> temporalLocality;
  std::optional<SpatialLocalityLevel> spatialLocality;
  std::optional<int64_t> estimatedAccessBytes;
  SmallVector<DimAccessPatternType> dimAccessPatterns;

  ///===--------------------------------------------------------------------===///
  /// Constructors
  ///===--------------------------------------------------------------------===///

  /// Construct from an allocation operation
  explicit MemrefMetadata(Operation *allocOp);

  ///===------------------------------------------------------------------===///
  /// ArtsMetadata interface
  ///===------------------------------------------------------------------===///
  StringRef getMetadataName() const override {
    return AttrNames::MemrefMetadata::Name;
  }
  bool importFromOp() override;
  void importFromJson(const llvm::json::Object &json) override;
  void exportToJson(llvm::json::Object &json) const override;
  StringRef toString() const override {
    return allocationId.empty() ? "-" : StringRef(allocationId);
  }
  Attribute getMetadataAttr() const override;

  ///===------------------------------------------------------------------===///
  /// Enum serialization helpers
  ///===------------------------------------------------------------------===///
  static std::string accessPatternToString(AccessPatternType pattern);
  static std::optional<AccessPatternType>
  stringToAccessPattern(llvm::StringRef str);

  static std::string temporalLocalityToString(TemporalLocalityLevel level);
  static std::optional<TemporalLocalityLevel>
  stringToTemporalLocality(llvm::StringRef str);

  static std::string spatialLocalityToString(SpatialLocalityLevel level);
  static std::optional<SpatialLocalityLevel>
  stringToSpatialLocality(llvm::StringRef str);
  static std::string dimAccessPatternToString(DimAccessPatternType pattern);
  static std::optional<DimAccessPatternType>
  stringToDimAccessPattern(llvm::StringRef str);

private:
  Operation *allocOp_;
};

} // namespace arts
} // namespace mlir

#endif /// ARTS_UTILS_MEMREFMETADATA_H
