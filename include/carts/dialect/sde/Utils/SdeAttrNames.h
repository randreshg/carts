#ifndef CARTS_DIALECT_SDE_UTILS_SDEATTRNAMES_H
#define CARTS_DIALECT_SDE_UTILS_SDEATTRNAMES_H

#include "llvm/ADT/StringRef.h"

namespace mlir::carts::sde::AttrNames {

/// SDE marker attached to host-side OpenMP regions that intentionally remain
/// unconverted by OmpToSde. Verifiers and downstream passes test for this
/// marker to allow the region to survive past the SDE objects-only check.
inline constexpr llvm::StringLiteral KeepHostOpenMP = "sde.keep_host_openmp";

/// CPS candidate annotations attached to SDE ops before CPS plan
/// materialization. Producers (BarrierElimination) and verifiers
/// (VerifySdeCpsPlan) share these names. The four attrs are set together as a
/// group; partial sets are an error.
inline constexpr llvm::StringLiteral CpsCandidateGroupId =
    "cps_candidate_group_id";
inline constexpr llvm::StringLiteral CpsCandidateStageIndex =
    "cps_candidate_stage_index";
inline constexpr llvm::StringLiteral CpsCandidateStageCount =
    "cps_candidate_stage_count";
inline constexpr llvm::StringLiteral CpsCandidateRequiresTokenizedDataflow =
    "cps_candidate_requires_tokenized_dataflow";

/// Runtime-neutral CU/MU graph partition evidence stamped by SDE distribution
/// planning. These attrs carry only geometry, abstract cost, and concurrency
/// facts. They must not name collectives, concrete storage objects, tasks,
/// routes, ranks, or runtime transports.
inline constexpr llvm::StringLiteral PartitionGraph = "partitionGraph";
inline constexpr llvm::StringLiteral PartitionScore = "partitionScore";

namespace LayoutGraph {
inline constexpr llvm::StringLiteral ArrayId = "arrayId";
inline constexpr llvm::StringLiteral Role = "role";
inline constexpr llvm::StringLiteral Kind = "kind";
inline constexpr llvm::StringLiteral OwnerDims = "ownerDims";
inline constexpr llvm::StringLiteral BlockShape = "blockShape";
inline constexpr llvm::StringLiteral CommVolumeBytes = "commVolumeBytes";
inline constexpr llvm::StringLiteral BlockParallel = "block_parallel";
inline constexpr llvm::StringLiteral BlockContraction = "block_contraction";
inline constexpr llvm::StringLiteral Replicated = "replicated";
} // namespace LayoutGraph

namespace LayoutGraphValues {
inline constexpr llvm::StringLiteral RoleWrite = "write";
inline constexpr llvm::StringLiteral RoleRead = "read";
inline constexpr llvm::StringLiteral RoleUnknown = "unknown";
} // namespace LayoutGraphValues

namespace PartitionGraphKeys {
inline constexpr llvm::StringLiteral MuId = "muId";
inline constexpr llvm::StringLiteral Role = "role";
inline constexpr llvm::StringLiteral LayoutKind = "layoutKind";
inline constexpr llvm::StringLiteral OwnerDims = "ownerDims";
inline constexpr llvm::StringLiteral BlockShape = "blockShape";
inline constexpr llvm::StringLiteral TilePayloadBytes = "tilePayloadBytes";
inline constexpr llvm::StringLiteral EdgeCommBytes = "edgeCommBytes";
inline constexpr llvm::StringLiteral EdgeClass = "edgeClass";
} // namespace PartitionGraphKeys

namespace PartitionScoreKeys {
inline constexpr llvm::StringLiteral Objective = "objective";
inline constexpr llvm::StringLiteral TargetLogicalWorkers =
    "targetLogicalWorkers";
inline constexpr llvm::StringLiteral ExposedCuCount = "exposedCuCount";
inline constexpr llvm::StringLiteral RequestedCuCount = "requestedCuCount";
inline constexpr llvm::StringLiteral ChosenCuCount = "chosenCuCount";
inline constexpr llvm::StringLiteral MinTileBytes = "minTileBytes";
inline constexpr llvm::StringLiteral ChosenTileBytes = "chosenTileBytes";
inline constexpr llvm::StringLiteral CommVolumeBytes = "commVolumeBytes";
inline constexpr llvm::StringLiteral OwnerDims = "ownerDims";
inline constexpr llvm::StringLiteral BlockShape = "blockShape";
} // namespace PartitionScoreKeys

namespace PartitionGraphValues {
inline constexpr llvm::StringLiteral ObjectiveMaxConcurrencyCommAware =
    "max_concurrency_comm_aware";
inline constexpr llvm::StringLiteral EdgeAligned = "aligned";
inline constexpr llvm::StringLiteral EdgeLayoutMismatch = "layout_mismatch";
inline constexpr llvm::StringLiteral OwnerBlock = "owner_block";
inline constexpr llvm::StringLiteral UnknownLayout = "unknown_layout";
} // namespace PartitionGraphValues

} // namespace mlir::carts::sde::AttrNames

#endif /// CARTS_DIALECT_SDE_UTILS_SDEATTRNAMES_H
