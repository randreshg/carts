#ifndef CARTS_UTILS_OPERATIONATTRIBUTES_H
#define CARTS_UTILS_OPERATIONATTRIBUTES_H

#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>

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

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_OPERATIONATTRIBUTES_H
