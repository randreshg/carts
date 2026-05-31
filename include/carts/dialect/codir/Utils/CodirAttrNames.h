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
inline constexpr llvm::StringLiteral MuBlockCount = "muBlockCount";
inline constexpr llvm::StringLiteral EdgeClass = "edgeClass";
} // namespace PartitionGraphKeys

namespace PartitionGraphValues {
inline constexpr llvm::StringLiteral EdgeLayoutMismatch = "layout_mismatch";
} // namespace PartitionGraphValues

namespace PartitionScoreKeys {
inline constexpr llvm::StringLiteral TargetLogicalWorkers =
    "targetLogicalWorkers";
inline constexpr llvm::StringLiteral ExposedCuCount = "exposedCuCount";
inline constexpr llvm::StringLiteral ChosenCuCount = "chosenCuCount";
inline constexpr llvm::StringLiteral MuBlockCount = "muBlockCount";
} // namespace PartitionScoreKeys

} // namespace mlir::carts::codir::AttrNames

#endif // CARTS_DIALECT_CODIR_UTILS_CODIRATTRNAMES_H
