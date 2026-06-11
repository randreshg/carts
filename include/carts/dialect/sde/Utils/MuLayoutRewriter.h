///==========================================================================///
/// File: MuLayoutRewriter.h
///
/// SDE-owned MU-level rank-expansion rewriter.
///
/// `MuLayoutRewriter` applies a committed block-grid layout to ONE
/// `sde.mu_alloc`: it rank-expands the result memref type and rewrites EVERY
/// use into the physical coordinate system, delegating per-access index
/// localization to a mode-specific `MuAccessIndexer`. It is the SDE analogue of
/// the old ARTS DbRewriter/DbIndexer stack, rebuilt for the MU/CU model: no
/// acquires, no db_refs, no EDT rebasing — a single flat expanded memref and
/// in-place `memref.load`/`memref.store` rewrites.
///
/// The rewriter performs a REAL transformation or fails closed; it never stamps
/// a metadata promise for a later layer to repair.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_MULAYOUTREWRITER_H
#define CARTS_DIALECT_SDE_UTILS_MULAYOUTREWRITER_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SmallVector.h"

#include <memory>

namespace mlir::carts::sde {

/// Mode-specific logical->physical index localization for a rank-expanded MU.
/// Pure arith over `OpBuilder`: emits the grid (div) and tile (mod) coordinates
/// of one logical access index list, in the same order as `buildExpandedMuType`
/// (grid prefix in owner order, then per-logical-dim tile coords).
class MuAccessIndexer {
public:
  explicit MuAccessIndexer(const MuPhysicalLayout &plan) : plan(plan) {}
  virtual ~MuAccessIndexer() = default;

  /// `logicalIndices.size()` must equal `plan.logicalRank()`. Returns
  /// `plan.expandedRank()` indices.
  virtual llvm::SmallVector<mlir::Value, 6>
  localize(mlir::ValueRange logicalIndices, mlir::OpBuilder &builder,
           mlir::Location loc) const = 0;

protected:
  const MuPhysicalLayout &plan;
};

/// HPF BLOCK div/mod localization: for owner dim with block extent B,
///   gridCoord = idx / B ; tileCoord = idx % B.
/// Non-owner dims pass through unchanged.
class MuBlockIndexer : public MuAccessIndexer {
public:
  using MuAccessIndexer::MuAccessIndexer;
  llvm::SmallVector<mlir::Value, 6> localize(mlir::ValueRange logicalIndices,
                                             mlir::OpBuilder &builder,
                                             mlir::Location loc) const override;
};

/// Elementwise budget-reconciled tile: identical div/mod against the committed
/// block extent. Distinct type so per-element window divergence can land later
/// without changing the rewriter contract.
class MuElementWiseIndexer : public MuBlockIndexer {
public:
  using MuBlockIndexer::MuBlockIndexer;
};

/// Stencil without halo growth: identical div/mod. Halo grown-tile / redist
/// realization is a separate SDE transformation.
class MuStencilIndexer : public MuBlockIndexer {
public:
  using MuBlockIndexer::MuBlockIndexer;
};

/// Factory for the mode-specific access indexer of a committed block-grid plan:
/// stencil -> MuStencilIndexer, elementwise(/pipeline) -> MuElementWiseIndexer,
/// otherwise MuBlockIndexer. Shared by rank expansion and coarse avoidance so
/// both realize the same committed plan identically.
std::unique_ptr<MuAccessIndexer>
makeMuAccessIndexer(SdeStructuredClassification cls,
                    const MuPhysicalLayout &plan);

/// The block-grid realize gate shared by rank expansion, coarse avoidance, and
/// coarse-avoidance verification: true iff `si` is a committed, fully-static
/// elementwise/stencil BLOCK plan (any number of owner dims) whose committed
/// `physicalOwnerDims`/`physicalBlockShape` resolve to a real grid (block count
/// > 1 on at least one owner dim). It reads the committed plan VERBATIM — it
/// never recomputes owner dims or block shape — and fills `out` on success.
/// Matmul, reduction, dynamic, and no-committed-plan MUs are out of scope
/// (returns false; the caller leaves them conservative or diagnoses them).
bool isBlockGridRealizable(SdeSuIterateOp si, mlir::MemRefType logicalType,
                           MuPhysicalLayout &out);

/// Find the committed block-grid storage plan governing `muAlloc`: the nearest
/// enclosing `su_iterate` of any store user that carries both
/// `physicalOwnerDims` and `physicalBlockShape`. Reader layouts are consumers
/// of storage and may require SDE movement; they must not choose or conflict
/// with storage grain. Returns null when there is no such writer, or when
/// distinct writers disagree on the committed owner-dims/block-shape.
SdeSuIterateOp findCommittedBlockPlanWriter(SdeMuAllocOp muAlloc);

/// True if `root` has a use a block-grid layout cannot localize — any user
/// other than a direct `memref.load`/`store`/`dealloc`,
/// `sde.array_layout_root` provenance, an `sde.mu_access_window` fact, or an
/// `sde.redist` fact. The single allow-list shared by the coarse-avoidance
/// gate and redistribution realization so they never drift.
bool muRootHasUnsupportedUse(mlir::Value root);

/// A rank-expanded block-grid MU candidate (any number of owner dims),
/// recognized purely from the committed writer `su_iterate` plus the expanded
/// memref type. This is shape RECOGNITION only (the `verify-sde-mu-layout`
/// shape gate); it does NOT prove tile/grain consistency — callers decide
/// whether a mismatch is a skip (a raiser) or an error (a verifier).
///
/// The expanded form is K leading grid dims (in owner order) followed by L tile
/// dims, so `muType.getRank() == logicalRank + ownerDims.size()`. All
/// owner-parallel vectors are indexed in the same (ascending) owner order.
struct ExpandedBlockGridMu {
  llvm::SmallVector<unsigned, 4> ownerDims; ///< committed owner dims, ASCENDING
  unsigned logicalRank = 0; ///< == expandedRank - ownerDims.size()
  llvm::SmallVector<int64_t, 4>
      blockExtents; ///< per owner dim, parallel to ownerDims
  llvm::SmallVector<int64_t, 4> gridCounts; ///< leading grid dims, owner order
};

/// Recognize the expanded form (any number of owner dims): `physicalOwnerDims`
/// has K entries, `muType.getRank() == physicalBlockShape.size() + K`, owner
/// dims are unique, in `[0, logicalRank)`, and sorted ascending. Fills
/// `blockExtents[i] = physicalBlockShape[ownerDims[i]]` and `gridCounts[i]`
/// from the leading K dims of `muType`. Returns nullopt for flat / owner-length
/// / rank-mismatch / missing-attr MUs (out of scope — conservative, not an
/// error).
std::optional<ExpandedBlockGridMu>
recognizeExpandedBlockGridMu(SdeSuIterateOp si, mlir::MemRefType muType);

/// Each grid count of a rank-expanded MU must be `ceilDiv(extent, block)` for
/// an ACTUAL committed iteration extent on the writer `su_iterate`, read from
/// the iteration domain (independent of the expanded type) so a recover proof
/// is not tautological. `blockExtents` and `gridCounts` are parallel, one entry
/// per owner dim (owner order). Returns one matching original owner-dim extent
/// per owner dim — each from a DISTINCT committed loop iteration dim — or
/// nullopt when any owner dim has no committed iteration extent yielding its
/// grid count (or when the inputs are inconsistent).
std::optional<llvm::SmallVector<int64_t, 4>>
findOwnerIterationExtents(SdeSuIterateOp si,
                          llvm::ArrayRef<int64_t> blockExtents,
                          llvm::ArrayRef<int64_t> gridCounts);

/// Applies a committed block-grid layout to one `sde.mu_alloc`.
class MuLayoutRewriter {
public:
  MuLayoutRewriter(const MuPhysicalLayout &plan, MuAccessIndexer &indexer)
      : plan(plan), indexer(indexer) {}

  /// Rank-expand `muAlloc` and rewrite all of its uses. On success the original
  /// `mu_alloc` and its (now dead) load/store/dealloc users are erased and a
  /// new expanded `mu_alloc` takes their place. On failure NO IR is mutated:
  /// the MU is left in its conservative flat form for the caller to diagnose.
  mlir::LogicalResult apply(SdeMuAllocOp muAlloc);

private:
  const MuPhysicalLayout &plan;
  MuAccessIndexer &indexer;
};

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_MULAYOUTREWRITER_H
