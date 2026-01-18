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
constexpr StringLiteral Workers = "workers";
constexpr StringLiteral ChunkSize = "chunk_size";
constexpr StringLiteral ArtsId = "arts.id";
constexpr StringLiteral ArtsCreateId = "arts.create_id";
constexpr StringLiteral ArtsTwinDiff = "arts.twin_diff";
constexpr StringLiteral OutlinedFunc = "outlined_func";
constexpr StringLiteral Nowait = "nowait";
constexpr StringLiteral Partition = "arts.partition";
} // namespace Operation

} // namespace AttrNames

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

/// Helper accessors for arts.partition attribute (PartitionMode).
inline std::optional<PartitionMode> getPartitionMode(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr =
          op->getAttrOfType<PartitionModeAttr>(AttrNames::Operation::Partition))
    return attr.getValue();
  return std::nullopt;
}

inline void setPartitionMode(Operation *op, PartitionMode mode) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::Partition,
              PartitionModeAttr::get(op->getContext(), mode));
}

} // namespace arts
} // namespace mlir

#endif /// CARTS_UTILS_OPERATIONATTRIBUTES_H
