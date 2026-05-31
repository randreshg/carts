///==========================================================================///
/// File: StructuredOpAnalysis.h
///
/// Reusable structural loop analysis for SDE scheduling-unit loops.
///
/// This analysis stays on the SDE side of the pipeline so semantic pattern
/// classification can be shared by SDE-owned distribution and tiling planning
/// without depending on target runtime IR.
///
/// Consumes post-LowerAffine IR: `sde.su_iterate` bodies containing
/// `scf.for`, `memref.load`/`store`, and
/// `arith.addi`/`subi`/`muli`/`index_cast`/`constant`. It does **not** match
/// `affine.*` ops; those have been lowered by the `sde-input-normalization`
/// and `initial-cleanup` stages registered in
/// `tools/compile/Compile.cpp`. The `AffineMap` / `AffineExpr` types here
/// come from `mlir/IR/AffineMap.h` (MLIR's IR-level math data structure for
/// linear combinations of dims and symbols) and are reconstructed from
/// post-lowering `arith` chains by `tryGetAffineExpr` and
/// `tryBuildIndexingMap` in `StructuredOpAnalysis.cpp`.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_ANALYSIS_STRUCTUREDMETHODANALYSIS_H
#define ARTS_DIALECT_SDE_ANALYSIS_STRUCTUREDMETHODANALYSIS_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Utils/StructuredOpsUtils.h"
#include "mlir/IR/AffineMap.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir::carts::sde {

struct LoopNestInfo {
  SmallVector<Value> ivs;
  Block *innermostBody = nullptr;
  SdeSuIterateOp rootIterOp;
};

struct MemrefAccessEntry {
  Value memref;
  AffineMap indexingMap;
  Operation *op = nullptr;
  bool isRead = false;
};

struct StructuredLoopSummary {
  LoopNestInfo nest;
  SmallVector<MemrefAccessEntry> reads;
  SmallVector<MemrefAccessEntry> writes;
  SmallVector<AffineMap> outputMaps;
  SmallVector<utils::IteratorType> iterTypes;
  SdeStructuredClassification classification =
      SdeStructuredClassification::elementwise;
  bool supportsReductionCarrier = false;
};

struct StructuredNeighborhoodInfo {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> spatialDims;
  SmallVector<int64_t, 4> minOffsets;
  SmallVector<int64_t, 4> maxOffsets;
  SmallVector<int64_t, 4> writeFootprint;
};

struct StructuredOutputLayoutPlan {
  Value root;
  SmallVector<int64_t, 4> shape;
  SmallVector<int64_t, 4> loopDimToPhysicalDim;
  SmallVector<int64_t, 4> physicalDimToLoopDim;
};

/// Analyze one `sde.su_iterate` nest and recover the structural facts
/// needed by higher SDE passes. Returns nullopt when the loop is not a
/// supported perfectly nested memref-based structured loop.
std::optional<StructuredLoopSummary> analyzeStructuredLoop(SdeSuIterateOp op);

/// Recover a runtime-neutral structured neighborhood summary directly from SDE
/// loop-access analysis.
std::optional<StructuredNeighborhoodInfo>
extractNeighborhoodSummary(const StructuredLoopSummary &summary);

/// Recover a static write-backed output layout with a loop-dim to physical-dim
/// map. Returns nullopt when external writes disagree or the layout is not
/// statically shapeable.
std::optional<StructuredOutputLayoutPlan>
findCompatibleOutputLayoutPlan(const StructuredLoopSummary &summary);

/// Convenience wrapper around analyzeStructuredLoop + output layout recovery.
std::optional<StructuredOutputLayoutPlan>
findCompatibleOutputLayoutPlan(SdeSuIterateOp op);

/// Return true when a one-dimensional apparent reduction is only reducing
/// within the owner-local output slice. These loops can use elementwise
/// pipeline planning because no cross-owner reduction carrier is needed.
bool isOwnerLocalPipelineReduction(SdeSuIterateOp op);

/// Return true when a matmul-classified scheduling unit has separate external
/// roots for the logical lhs and rhs input windows. Current owner-slice
/// materialization has one physical dependency view per root, so self-Gram
/// shapes must stay on host-whole views until the boundary can represent both
/// windows independently.
bool hasDistinctExternalMatmulInputRoots(SdeSuIterateOp op);

/// Contraction-tiling candidate facts for a matmul-class scheduling unit.
/// Pattern-free: derived from iterator types + affine access shapes only.
struct ContractionTilingCandidate {
  /// The external input root read on the reduction + parallel[1] window — the
  /// rhs/contraction-dim input.
  Value contractionInputRoot;
  /// The single reduction loop dim that blocks the contraction axis.
  unsigned reductionLoopDim = 0;
  /// Physical position of the contraction input indexed by `reductionLoopDim`.
  /// This is derived from the actual input access map, not assumed from a
  /// canonical operand order.
  std::optional<unsigned> contractionInputPhysicalDim;
  /// The output (parallel) loop dims, in loop order.
  SmallVector<int64_t, 2> parallelLoopDims;
  /// The static contraction extent (the reduction trip count), if recoverable.
  std::optional<int64_t> contractionExtent;
};

/// Recover the contraction-tiling candidate for a matmul-class scheduling unit:
/// a canonical (2 parallel, 1 reduction, 3-dim) matmul with distinct external
/// lhs/rhs roots whose rhs (contraction-dim) input is read on the reduction +
/// parallel[1] window. Returns nullopt when the loop is not a canonical matmul
/// or the contraction-dim input cannot be isolated. This does NOT decide that
/// tiling should fire — the caller gates that on whether the contraction input
/// is a sibling-computed distributed intermediate.
std::optional<ContractionTilingCandidate>
findContractionTilingCandidate(SdeSuIterateOp op);

//===----------------------------------------------------------------------===//
// Module-scoped layout-assignment access model.
//===----------------------------------------------------------------------===//

/// How one indexed position of an array is used by one scheduling unit's loop.
/// Derived purely from affine access maps + iterator types — pattern-free.
enum class ArrayDimKind {
  /// The position is indexed by a single parallel loop IV (offset 0). This is
  /// an
  /// owner-dim candidate (the position can be block-distributed).
  parallelIndexed,
  /// The position is indexed by a single reduction loop IV. A position used
  /// this
  /// way blocks the contraction axis when the array is a matmul rhs.
  reductionIndexed,
  /// The position is indexed by a parallel IV with a non-zero constant offset
  /// (a stencil neighborhood read) — owner-alignable but with a halo.
  parallelHalo,
  /// The position is broadcast (constant index / not a function of any single
  /// loop dim) — it does not select an owner.
  broadcast,
};

/// One scheduling unit's use of one array-root position.
struct ArrayPositionUse {
  /// Stable per-module id of the scheduling unit that produced this use.
  unsigned codeletId = 0;
  /// How the loop indexes this physical position.
  ArrayDimKind kind = ArrayDimKind::broadcast;
  /// The loop dim doing the indexing (when kind selects a single dim).
  std::optional<unsigned> loopDim;
  /// True when this use is a write (the producing/owner access).
  bool isWrite = false;
};

/// Accumulated module-wide access facts for one array root. The root is the
/// memref SSA value after view-op stripping; profiles are keyed by it.
struct ArrayAccessProfile {
  Value root;
  unsigned rank = 0;
  SmallVector<int64_t, 4> staticShape;
  /// Per physical position, every recorded use across the module.
  SmallVector<SmallVector<ArrayPositionUse, 2>, 4> positionUses;
  /// True when at least one scheduling unit writes this root.
  bool hasWriter = false;
  /// True when at least one scheduling unit reads this root.
  bool hasReader = false;
  /// The codeletId of the (single) writer scheduling unit, if exactly one.
  std::optional<unsigned> writerCodeletId;
};

/// Module-wide access relations: one profile per accessed external array root,
/// plus the codeletId assigned to every analyzed `sde.su_iterate`. Pattern-free
/// (built only from affine access maps, iterator types, and static shapes).
struct ModuleAccessRelations {
  /// Profiles keyed by the array root SSA value (post view-strip).
  llvm::MapVector<Value, ArrayAccessProfile> profiles;
  /// The scheduling-unit ops in stable id order; index == codeletId.
  SmallVector<SdeSuIterateOp> codelets;
};

/// Walk every `sde.su_iterate` under `moduleOp`, run `analyzeStructuredLoop`,
/// and accumulate per-array-root access profiles. Returns the module access
/// relations used by layout assignment. Pattern-free.
ModuleAccessRelations buildModuleAccessRelations(Operation *moduleOp);

/// The geometric family of a chosen array layout. Names no collective.
enum class ArrayLayoutKind {
  /// Block-distributed on owner (parallel) positions — the common case.
  blockParallel,
  /// Block-distributed on a contraction (reduction) position — a sibling matmul
  /// intermediate consumed on its contraction axis.
  blockContraction,
  /// Replicated / host-whole — highest-cost fallback.
  replicated,
};

/// One candidate layout for an array (PhaseB). Element-space only.
struct ArrayLayoutCandidate {
  ArrayLayoutKind kind = ArrayLayoutKind::replicated;
  /// Element-space owner positions (the distributed axes).
  SmallVector<int64_t, 4> ownerPositions;
  /// Element-space per-position block extents (full extent on non-owner dims).
  SmallVector<int64_t, 4> blockShape;
};

/// The chosen layout for an array root after cost-minimizing assignment
/// (PhaseC). `commVolumeBytes` is the abstract per-array contribution.
struct AssignedArrayLayout {
  ArrayLayoutCandidate layout;
  int64_t commVolumeBytes = 0;
};

//===----------------------------------------------------------------------===//
// Shared affine decomposition utilities
//===----------------------------------------------------------------------===//

/// Affine expression normalized to one loop dim plus a constant offset.
struct AffineDimOffset {
  std::optional<unsigned> dim;
  int64_t offset = 0;
};

/// Extract a single-dim + constant form from an affine expression.
/// Recursively decomposes through Add expressions.
std::optional<AffineDimOffset> extractDimOffset(AffineExpr expr);

/// Check whether an indexing map contains any non-zero constant stencil
/// offsets of the form `dim + c` where c != 0.
bool hasConstantOffsets(AffineMap map);

} // namespace mlir::carts::sde

#endif // ARTS_DIALECT_SDE_ANALYSIS_STRUCTUREDMETHODANALYSIS_H
