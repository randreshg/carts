#ifndef CARTS_UTILS_OPERATIONATTRIBUTES_H
#define CARTS_UTILS_OPERATIONATTRIBUTES_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <optional>

namespace mlir {
namespace arts {
namespace AttrNames {

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

// Forward declaration - defined in HeuristicsConfig.h
struct PartitioningHint;

/// Helper accessors for arts.id on arbitrary operations.
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

/// Helper accessors for partition_mode attribute (PartitionMode).
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

/// Helper accessors for access_pattern attribute (DbAccessPattern).
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

/// Helper accessors for arts.partition_hint attribute (PartitioningHint).
/// These use DictionaryAttr for serialization - full implementation in
/// HeuristicsConfig.cpp
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
