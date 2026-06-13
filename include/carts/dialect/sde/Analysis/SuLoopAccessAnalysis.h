///==========================================================================///
/// File: SuLoopAccessAnalysis.h
///
/// Reusable structural loop analysis for SDE scheduling-unit loops.
///
/// This analysis stays on the SDE side of the pipeline so semantic pattern
/// classification can be shared by SDE-owned distribution and tiling planning
/// without depending on target runtime IR.
///
/// Consumes post-SDE-wrap IR inside `sde.su_iterate` CU bodies: `scf.for`,
/// `memref.load`/`store`, and lowered `arith` index chains. A planning-head
/// `LowerAffine` bridge remains until S4 swaps onto upstream `MemRefAccess`.
/// Index maps are rebuilt from `arith` by `tryGetAffineExpr` in this file.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_ANALYSIS_SU_LOOP_ACCESS_ANALYSIS_H
#define ARTS_DIALECT_SDE_ANALYSIS_SU_LOOP_ACCESS_ANALYSIS_H

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

struct SuLoopAccessSummary {
  LoopNestInfo nest;
  SmallVector<MemrefAccessEntry> reads;
  SmallVector<MemrefAccessEntry> writes;
  SmallVector<AffineMap> outputMaps;
  SmallVector<utils::IteratorType> iterTypes;
  SdeStructuredClassification classification =
      SdeStructuredClassification::elementwise;
  bool supportsReductionCarrier = false;
};

struct SuNeighborhoodAccessInfo {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> spatialDims;
  SmallVector<int64_t, 4> minOffsets;
  SmallVector<int64_t, 4> maxOffsets;
  SmallVector<int64_t, 4> writeFootprint;
};

struct SuOutputLayoutFacts {
  Value root;
  SmallVector<int64_t, 4> shape;
  SmallVector<int64_t, 4> loopDimToPhysicalDim;
  SmallVector<int64_t, 4> physicalDimToLoopDim;
};

/// Analyze one `sde.su_iterate` nest and recover the structural facts
/// needed by higher SDE passes. Returns nullopt when the loop is not a
/// supported perfectly nested memref-based structured loop.
std::optional<SuLoopAccessSummary> analyzeSuLoopAccesses(SdeSuIterateOp op);

/// Recover a runtime-neutral structured neighborhood summary directly from SDE
/// loop-access analysis.
std::optional<SuNeighborhoodAccessInfo>
extractNeighborhoodAccessInfo(const SuLoopAccessSummary &summary);

/// Resolve structured classification with the same refinements the deleted
/// loop-pattern-facts pass applied (explicit stencil facts, pipeline carry,
/// strip-mined reduction preservation).
SdeStructuredClassification
resolveStructuredClassification(SdeSuIterateOp op,
                                const SuLoopAccessSummary &summary);

/// Recomputed structured classification; nullopt when analysis fails.
std::optional<SdeStructuredClassification>
queryStructuredClassification(SdeSuIterateOp op);

/// Derive the SDE pattern enum from loop-access summary facts.
SdePattern deriveSuPattern(const SuLoopAccessSummary &summary,
                           SdeStructuredClassification classification,
                           const SuNeighborhoodAccessInfo *neighborhood);

/// Recomputed pattern; nullopt when analysis fails.
std::optional<SdePattern> querySuPattern(SdeSuIterateOp op);

/// True when every self-read matches the write map at the same indices.
bool hasOnlyPointInPlaceSelfReads(const SuLoopAccessSummary &summary);

/// Recomputed neighborhood; honors frontend-authored explicit stencil facts
/// when present, otherwise derives from the loop-access summary.
std::optional<SuNeighborhoodAccessInfo>
queryNeighborhoodAccessInfo(SdeSuIterateOp op);

/// Recomputed in-place aliasing legality (point-local self-read stencils).
bool queryInPlaceSafe(SdeSuIterateOp op);

/// Recomputed Gauss-Seidel / shared-state in-place gate.
bool queryInPlaceSharedState(SdeSuIterateOp op);

struct SuPartialReductionFacts {
  bool hasPartialReduction = false;
  SmallVector<int64_t, 4> reductionDims;
  SmallVector<int64_t, 4> ownerDims;
};

/// Recomputed partial-reduction facts (pipeline + matmul contraction tiling).
std::optional<SuPartialReductionFacts>
queryPartialReductionFacts(SdeSuIterateOp op);

/// Recover a static write-backed output layout with a loop-dim to physical-dim
/// map. Returns nullopt when external writes disagree or the layout is not
/// statically shapeable.
std::optional<SuOutputLayoutFacts>
findCompatibleSuOutputLayoutFacts(const SuLoopAccessSummary &summary);

/// Convenience wrapper around analyzeSuLoopAccesses + output layout recovery.
std::optional<SuOutputLayoutFacts>
findCompatibleSuOutputLayoutFacts(SdeSuIterateOp op);

/// Return true when a point-local (`inPlaceSafe`) stencil whose access
/// footprint spans more physical dimensions than the realized loop rank can
/// still be realized as a `<= loopRank` owner strip: its parallel loop band
/// maps onto a compatible static output layout. In that case the wide
/// `ownerDims` footprint is not an unrealizable N-D owner shape — the
/// remaining cross-owner neighbor reads must be carried as a read-only halo
/// along the strip. Used to distinguish this realizable case from a genuine
/// nested-owner shape that must fail closed.
bool hasRealizableOwnerStrip(SdeSuIterateOp op);

/// Return true when a one-dimensional apparent reduction is only reducing
/// within the owner-local output slice. These loops can use elementwise
/// pipeline planning because no cross-owner reduction carrier is needed.
bool isOwnerLocalPipelineReduction(SdeSuIterateOp op);

/// Return true when a matmul-classified scheduling unit has separate external
/// roots for the logical lhs and rhs input windows. Current owner-slice
/// realization has one physical dependency view per root, so self-Gram
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
  unsigned suId = 0;
  /// How the loop indexes this physical position.
  ArrayDimKind kind = ArrayDimKind::broadcast;
  /// The loop dim doing the indexing (when kind selects a single dim).
  std::optional<unsigned> loopDim;
  /// True when the loop dim is a realized `sde.su_iterate` owner dimension,
  /// not merely an unpromoted nested loop IV.
  bool isSchedulingLoopDim = false;
  /// True when this use is a write (the producing/owner access).
  bool isWrite = false;
  /// Number of non-degenerate physical dimensions covered by this write's
  /// zero-offset parallel indexing map. Zero for reads.
  unsigned writeCoverageRank = 0;
  /// True when this write covers every non-degenerate physical dimension of
  /// the root with unique zero-offset parallel loop IVs.
  bool fullRankWrite = false;
};

/// Accumulated module-wide access facts for one array root. The root is the
/// memref SSA value after view-op stripping; profiles are keyed by it.
struct ArrayAccessProfile {
  Value root;
  unsigned rank = 0;
  SmallVector<int64_t, 4> staticShape;
  /// Per physical position, every recorded use across the module.
  SmallVector<SmallVector<ArrayPositionUse, 2>, 4> positionUses;
  /// Number of static shape dimensions with extent greater than one.
  unsigned nonDegenerateRank = 0;
  /// True when at least one scheduling unit writes this root.
  bool hasWriter = false;
  /// True when at least one scheduling unit reads this root.
  bool hasReader = false;
  /// Best write coverage rank seen across all writers.
  unsigned maxWriteCoverageRank = 0;
  /// True when some writer covers every non-degenerate physical dimension.
  bool hasFullRankWriter = false;
  /// The suId of the best writer. When hasFullRankWriter is true, this is the
  /// canonical full-root writer.
  std::optional<unsigned> writerSuId;
};

/// Module-wide access relations: one profile per accessed external array root,
/// plus the suId assigned to every analyzed `sde.su_iterate`. Pattern-free
/// (built only from affine access maps, iterator types, and static shapes).
struct ModuleSuAccessRelations {
  /// Profiles keyed by the array root SSA value (post view-strip).
  llvm::MapVector<Value, ArrayAccessProfile> profiles;
  /// The scheduling-unit ops in stable id order; index == suId.
  SmallVector<SdeSuIterateOp> schedulingUnits;
};

/// Walk every `sde.su_iterate` under `moduleOp`, run `analyzeSuLoopAccesses`,
/// and accumulate per-array-root access profiles. Returns the module access
/// relations used by layout assignment. Pattern-free.
ModuleSuAccessRelations buildModuleSuAccessRelations(Operation *moduleOp);

/// The geometric family of a chosen array layout. Names no communication
/// operation.
enum class ArrayLayoutKind {
  /// Block-distributed on owner (parallel) positions — the common case.
  blockParallel,
  /// Block-distributed on a contraction (reduction) position — a sibling matmul
  /// intermediate consumed on its contraction axis.
  blockContraction,
  /// Replicated / host-whole — highest-cost candidate.
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

/// The chosen layout for an array root after structural assignment (PhaseC).
struct AssignedArrayLayout {
  ArrayLayoutCandidate layout;
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

#endif // ARTS_DIALECT_SDE_ANALYSIS_SU_LOOP_ACCESS_ANALYSIS_H
