///==========================================================================///
/// File: CodirAttrNames.h
///
/// Shared CODIR attribute names for generic attrs attached to CODIR ops.
///==========================================================================///

#ifndef CARTS_DIALECT_CODIR_UTILS_CODIRATTRNAMES_H
#define CARTS_DIALECT_CODIR_UTILS_CODIRATTRNAMES_H

#include "llvm/ADT/StringRef.h"

namespace mlir::carts::codir::AttrNames {

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

} // namespace mlir::carts::codir::AttrNames

#endif // CARTS_DIALECT_CODIR_UTILS_CODIRATTRNAMES_H
