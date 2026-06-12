#ifndef CARTS_DIALECT_SDE_UTILS_SDEATTRNAMES_H
#define CARTS_DIALECT_SDE_UTILS_SDEATTRNAMES_H

#include "llvm/ADT/StringRef.h"

namespace mlir::carts::sde::AttrNames {

/// SDE marker attached to host-side OpenMP regions that intentionally remain
/// unconverted by OmpToSde. Verifiers and downstream passes test for this
/// marker to allow the region to survive past the SDE objects-only check.
inline constexpr llvm::StringLiteral KeepHostOpenMP = "sde.keep_host_openmp";

namespace LayoutGraph {
inline constexpr llvm::StringLiteral ArrayId = "arrayId";
inline constexpr llvm::StringLiteral Role = "role";
inline constexpr llvm::StringLiteral Kind = "kind";
inline constexpr llvm::StringLiteral OwnerDims = "ownerDims";
inline constexpr llvm::StringLiteral BlockShape = "blockShape";
inline constexpr llvm::StringLiteral MuBlockCount = "muBlockCount";
inline constexpr llvm::StringLiteral CommVolumeBytes = "commVolumeBytes";
// Node-agnostic, budget-sized DB/MU block grain: a function of problem size and
// a target block-byte budget, NOT of node/worker count. BudgetBlockShape is
// consumed by SDE loop tiling and distribution transforms to choose physical
// tile block shape.
inline constexpr llvm::StringLiteral BudgetBlockShape = "budgetBlockShape";
inline constexpr llvm::StringLiteral BlockParallel = "block_parallel";
inline constexpr llvm::StringLiteral BlockContraction = "block_contraction";
inline constexpr llvm::StringLiteral Replicated = "replicated";
} // namespace LayoutGraph

namespace LayoutGraphValues {
inline constexpr llvm::StringLiteral RoleWrite = "write";
inline constexpr llvm::StringLiteral RoleRead = "read";
inline constexpr llvm::StringLiteral RoleUnknown = "unknown";
} // namespace LayoutGraphValues

} // namespace mlir::carts::sde::AttrNames

#endif /// CARTS_DIALECT_SDE_UTILS_SDEATTRNAMES_H
