#ifndef CARTS_UTILS_OPERATIONATTRIBUTES_H
#define CARTS_UTILS_OPERATIONATTRIBUTES_H

#include "llvm/ADT/StringRef.h"

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
} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_OPERATIONATTRIBUTES_H
