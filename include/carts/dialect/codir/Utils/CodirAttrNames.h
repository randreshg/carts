///==========================================================================///
/// File: CodirAttrNames.h
///
/// Shared CODIR attribute names for generic attrs attached to CODIR ops.
///==========================================================================///

#ifndef CARTS_DIALECT_CODIR_UTILS_CODIRATTRNAMES_H
#define CARTS_DIALECT_CODIR_UTILS_CODIRATTRNAMES_H

#include "llvm/ADT/StringRef.h"

namespace mlir::carts::codir::AttrNames {

/// Runtime-neutral CU/MU graph evidence forwarded from SDE. CODIR may inspect
/// these geometry/cost facts when selecting storage/collective families, but
/// the attrs themselves are still not collective names.
inline constexpr llvm::StringLiteral PartitionGraph = "partition_graph";
inline constexpr llvm::StringLiteral PartitionScore = "partition_score";

namespace PartitionGraphKeys {
inline constexpr llvm::StringLiteral MuId = "muId";
inline constexpr llvm::StringLiteral Role = "role";
inline constexpr llvm::StringLiteral LayoutKind = "layoutKind";
inline constexpr llvm::StringLiteral OwnerDims = "ownerDims";
inline constexpr llvm::StringLiteral BlockShape = "blockShape";
inline constexpr llvm::StringLiteral TilePayloadBytes = "tilePayloadBytes";
inline constexpr llvm::StringLiteral MuBlockCount = "muBlockCount";
inline constexpr llvm::StringLiteral CuGroupSize = "cuGroupSize";
inline constexpr llvm::StringLiteral CuGroupCount = "cuGroupCount";
inline constexpr llvm::StringLiteral EdgeCommBytes = "edgeCommBytes";
inline constexpr llvm::StringLiteral EdgeClass = "edgeClass";
} // namespace PartitionGraphKeys

namespace PartitionGraphValues {
inline constexpr llvm::StringLiteral EdgeLayoutMismatch = "layout_mismatch";
inline constexpr llvm::StringLiteral OwnerBlock = "owner_block";
} // namespace PartitionGraphValues

namespace LayoutGraphKeys {
inline constexpr llvm::StringLiteral ArrayId = "arrayId";
inline constexpr llvm::StringLiteral Role = "role";
inline constexpr llvm::StringLiteral Kind = "kind";
inline constexpr llvm::StringLiteral OwnerDims = "ownerDims";
inline constexpr llvm::StringLiteral BlockShape = "blockShape";
inline constexpr llvm::StringLiteral MuBlockCount = "muBlockCount";
inline constexpr llvm::StringLiteral CommVolumeBytes = "commVolumeBytes";
} // namespace LayoutGraphKeys

namespace LayoutGraphValues {
inline constexpr llvm::StringLiteral RoleWrite = "write";
inline constexpr llvm::StringLiteral RoleRead = "read";
} // namespace LayoutGraphValues

namespace PartitionScoreKeys {
inline constexpr llvm::StringLiteral TargetLogicalWorkers =
    "targetLogicalWorkers";
inline constexpr llvm::StringLiteral ExposedCuCount = "exposedCuCount";
} // namespace PartitionScoreKeys

} // namespace mlir::carts::codir::AttrNames

#endif // CARTS_DIALECT_CODIR_UTILS_CODIRATTRNAMES_H
