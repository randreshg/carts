#ifndef CARTS_UTILS_OPERATIONATTRIBUTES_H
#define CARTS_UTILS_OPERATIONATTRIBUTES_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <optional>

namespace mlir {
namespace arts {
namespace AttrNames {

/// Module-level attributes used across ARTS passes/codegen.
namespace Module {
using namespace llvm;
constexpr StringLiteral RuntimeConfigPath = "arts.runtime_config_path";
constexpr StringLiteral RuntimeConfigData = "arts.runtime_config_data";
} // namespace Module

/// Operation-level attributes used across ARTS passes
namespace Operation {
using namespace llvm;

/// Common ARTS attributes
constexpr StringLiteral Workers = "workers";
constexpr StringLiteral ArtsId = "arts.id";
constexpr StringLiteral ArtsCreateId = "arts.create_id";
constexpr StringLiteral OutlinedFunc = "outlined_func";
constexpr StringLiteral Nowait = "nowait";

/// Partition-related attributes (TableGen-generated names)
constexpr StringLiteral PartitionMode = "partition_mode";
constexpr StringLiteral PartitionHint = "arts.partition_hint";
constexpr StringLiteral AccessPattern = "access_pattern";
constexpr StringLiteral Distributed = "distributed";
constexpr StringLiteral DistributionKind = "distribution_kind";
constexpr StringLiteral DistributionPattern = "distribution_pattern";
constexpr StringLiteral DistributionVersion = "distribution_version";
constexpr StringLiteral StencilCenterOffset = "stencil_center_offset";
constexpr StringLiteral ElementStride = "element_stride";
constexpr StringLiteral LeftHaloArgIdx = "left_halo_arg_idx";
constexpr StringLiteral RightHaloArgIdx = "right_halo_arg_idx";

/// DbAllocOp attributes (TableGen-generated names)
constexpr StringLiteral Mode = "mode";
constexpr StringLiteral AllocType = "allocType";
constexpr StringLiteral DbMode = "dbMode";
constexpr StringLiteral ElementType = "elementType";

/// EdtOp attributes (TableGen-generated names)
constexpr StringLiteral Type = "type";
constexpr StringLiteral Concurrency = "concurrency";

} // namespace Operation

} // namespace AttrNames

inline std::optional<StringRef> getRuntimeConfigPath(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr = module->getAttrOfType<StringAttr>(
          AttrNames::Module::RuntimeConfigPath))
    return attr.getValue();
  return std::nullopt;
}

inline void setRuntimeConfigPath(ModuleOp module, StringRef path) {
  if (!module || path.empty())
    return;
  module->setAttr(AttrNames::Module::RuntimeConfigPath,
                  StringAttr::get(module.getContext(), path));
}

inline std::optional<StringRef> getRuntimeConfigData(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr = module->getAttrOfType<StringAttr>(
          AttrNames::Module::RuntimeConfigData))
    return attr.getValue();
  return std::nullopt;
}

inline void setRuntimeConfigData(ModuleOp module, StringRef data) {
  if (!module || data.empty())
    return;
  module->setAttr(AttrNames::Module::RuntimeConfigData,
                  StringAttr::get(module.getContext(), data));
}

// Forward declaration - defined in HeuristicsConfig.h
struct PartitioningHint;

/// Merge two DbAccessPattern values, keeping the higher-priority pattern.
/// Priority: stencil > indexed > uniform > unknown.
inline DbAccessPattern mergeDbAccessPattern(DbAccessPattern lhs,
                                            DbAccessPattern rhs) {
  auto rank = [](DbAccessPattern p) -> unsigned {
    switch (p) {
    case DbAccessPattern::stencil:
      return 3;
    case DbAccessPattern::indexed:
      return 2;
    case DbAccessPattern::uniform:
      return 1;
    case DbAccessPattern::unknown:
      return 0;
    }
    return 0;
  };
  return rank(rhs) > rank(lhs) ? rhs : lhs;
}

inline int64_t getArtsId(Operation *op) {
  if (!op)
    return 0;
  if (auto attr = op->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsId))
    return attr.getInt();
  return 0;
}

inline void setArtsId(Operation *op, int64_t id) {
  if (!op || id <= 0)
    return;
  auto *ctx = op->getContext();
  auto type = IntegerType::get(ctx, 64);
  op->setAttr(AttrNames::Operation::ArtsId, IntegerAttr::get(type, id));
}

inline std::optional<PartitionMode> getPartitionMode(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<PartitionModeAttr>(
          AttrNames::Operation::PartitionMode))
    return attr.getValue();
  return std::nullopt;
}

inline void setPartitionMode(Operation *op, PartitionMode mode) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::PartitionMode,
              PartitionModeAttr::get(op->getContext(), mode));
}

inline std::optional<DbAccessPattern> getDbAccessPattern(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<DbAccessPatternAttr>(
          AttrNames::Operation::AccessPattern))
    return attr.getValue();
  return std::nullopt;
}

inline void setDbAccessPattern(Operation *op, DbAccessPattern pattern) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::AccessPattern,
              DbAccessPatternAttr::get(op->getContext(), pattern));
}

inline bool hasDistributedDbAllocation(Operation *op) {
  if (!op)
    return false;
  return op->hasAttr(AttrNames::Operation::Distributed);
}

inline void setDistributedDbAllocation(Operation *op, bool enabled) {
  if (!op)
    return;
  if (enabled) {
    op->setAttr(AttrNames::Operation::Distributed,
                UnitAttr::get(op->getContext()));
    return;
  }
  op->removeAttr(AttrNames::Operation::Distributed);
}

inline std::optional<EdtDistributionKind>
getEdtDistributionKind(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<EdtDistributionKindAttr>(
          AttrNames::Operation::DistributionKind))
    return attr.getValue();
  return std::nullopt;
}

inline void setEdtDistributionKind(Operation *op, EdtDistributionKind kind) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::DistributionKind,
              EdtDistributionKindAttr::get(op->getContext(), kind));
}

inline std::optional<EdtDistributionPattern>
getEdtDistributionPattern(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<EdtDistributionPatternAttr>(
          AttrNames::Operation::DistributionPattern))
    return attr.getValue();
  return std::nullopt;
}

inline void setEdtDistributionPattern(Operation *op,
                                      EdtDistributionPattern pattern) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::DistributionPattern,
              EdtDistributionPatternAttr::get(op->getContext(), pattern));
}

/// Copy distribution_* attributes between operations.
/// This intentionally transfers only distribution contracts:
///   - distribution_kind
///   - distribution_pattern
///   - distribution_version
inline void copyDistributionAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;

  if (auto kind = getEdtDistributionKind(source))
    setEdtDistributionKind(dest, *kind);
  else
    dest->removeAttr(AttrNames::Operation::DistributionKind);

  if (auto pattern = getEdtDistributionPattern(source))
    setEdtDistributionPattern(dest, *pattern);
  else
    dest->removeAttr(AttrNames::Operation::DistributionPattern);

  if (auto version = source->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::DistributionVersion))
    dest->setAttr(AttrNames::Operation::DistributionVersion, version);
  else
    dest->removeAttr(AttrNames::Operation::DistributionVersion);
}

inline std::optional<int64_t> getStencilCenterOffset(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::StencilCenterOffset))
    return attr.getInt();
  return std::nullopt;
}

inline void setStencilCenterOffset(Operation *op, int64_t centerOffset) {
  if (!op)
    return;
  op->setAttr(
      AttrNames::Operation::StencilCenterOffset,
      IntegerAttr::get(IntegerType::get(op->getContext(), 64), centerOffset));
}

inline std::optional<int64_t> getElementStride(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr =
          op->getAttrOfType<IntegerAttr>(AttrNames::Operation::ElementStride))
    return attr.getInt();
  return std::nullopt;
}

inline void setElementStride(Operation *op, int64_t stride) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::ElementStride,
              IntegerAttr::get(IndexType::get(op->getContext()), stride));
}

inline std::optional<unsigned> getLeftHaloArgIndex(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::LeftHaloArgIdx)) {
    int64_t value = attr.getInt();
    if (value >= 0)
      return static_cast<unsigned>(value);
  }
  return std::nullopt;
}

inline std::optional<unsigned> getRightHaloArgIndex(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::RightHaloArgIdx)) {
    int64_t value = attr.getInt();
    if (value >= 0)
      return static_cast<unsigned>(value);
  }
  return std::nullopt;
}

inline void setLeftHaloArgIndex(Operation *op, unsigned argIndex) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::LeftHaloArgIdx,
              IntegerAttr::get(IndexType::get(op->getContext()), argIndex));
}

inline void setRightHaloArgIndex(Operation *op, unsigned argIndex) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::RightHaloArgIdx,
              IntegerAttr::get(IndexType::get(op->getContext()), argIndex));
}

/// Full implementation in HeuristicsConfig.cpp (uses DictionaryAttr).
std::optional<PartitioningHint> getPartitioningHint(Operation *op);
void setPartitioningHint(Operation *op, const PartitioningHint &hint);

/// Copy ARTS-specific metadata attributes from source to dest operation.
/// Copies: arts.id, partition_mode, arts.partition_hint, arts.loop.
/// Unlike transferAttributes in ArtsUtils.h which copies ALL attributes, this
/// only copies ARTS-specific metadata attributes.
void copyArtsMetadataAttrs(Operation *source, Operation *dest);

} // namespace arts
} // namespace mlir

#endif /// CARTS_UTILS_OPERATIONATTRIBUTES_H
