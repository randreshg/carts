///==========================================================================///
/// File: MuLayout.h
///
/// SDE MU block-grid layout geometry.
///
/// The committed per-array BLOCK layout (owner dims + block extents) lives on
/// the writer `sde.su_iterate` as the `physicalOwnerDims`/`physicalBlockShape`
/// facts. Rank expansion makes that grain *structural* by expanding the
/// `sde.mu_alloc` result memref so the block grid is part of the type, not an
/// attribute a downstream pass branches on.
///
/// Normal form (flat, single-level, prefix grid):
///   logical  memref<E0 x E1 x ... x E{L-1} x T>
///   owner dim d, block extent B_d (= physicalBlockShape[d]):
///   physical memref<G0 x G1 x ... x G{K-1}            (K = #owner dims, grid)
///                   x t0 x t1 x ... x t{L-1} x T>     (tile extents)
///   where Ci = ceilDiv(E{ownerDims[i]}, B{ownerDims[i]}) and
///   t_d = (d is an owner dim) ? B_d : E_d.
///
/// Examples:
///   memref<1024xf64>,  owner [0], block [256]    -> memref<4x256xf64>
///   memref<128x64xf64>, owner [0], block [16,64] -> memref<8x16x64xf64>
///
/// This geometry is read VERBATIM off the committed facts: it never recomputes
/// owner dims or block shape, it only re-expresses them as a memref type. The
/// inverse `recoverOwnerDims` proves `ownerDims == recover(structure)`.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_MULAYOUT_H
#define CARTS_DIALECT_SDE_UTILS_MULAYOUT_H

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir::carts::sde {

/// Committed physical block-grid layout for one MU root. Static int64 only.
/// Supports any number of owner dims (ND multi-owner owner-tile). Owner dims
/// are CANONICALIZED ASCENDING by resolveMuPhysicalLayout; blockExtents and
/// blockCounts are kept parallel to ownerDims in that ascending order.
struct MuPhysicalLayout {
  /// Original (logical) array shape, all static.
  llvm::SmallVector<int64_t, 4> logicalShape;
  /// Owner (distributed) dims, CANONICALIZED ASCENDING (C0).
  llvm::SmallVector<unsigned, 2> ownerDims;
  /// Per-owner-dim block extent (= physicalBlockShape[ownerDim]).
  llvm::SmallVector<int64_t, 2> blockExtents;
  /// Per-owner-dim block count = ceilDiv(logicalShape[ownerDim], blockExtent).
  llvm::SmallVector<int64_t, 2> blockCounts;

  unsigned logicalRank() const { return logicalShape.size(); }
  unsigned numOwnerDims() const { return ownerDims.size(); }
  unsigned expandedRank() const { return logicalRank() + numOwnerDims(); }
};

/// Resolve the committed layout from a logical memref type plus the committed
/// `physicalOwnerDims` / `physicalBlockShape` attrs (both I64 ArrayAttrs).
///
/// Fails (nullopt) — fail closed, NOT a partial promise — on any of:
///   * dynamic logical dims,
///   * missing/empty owner-dim or block-shape attrs,
///   * owner dim out of range or duplicated,
///   * block extent <= 0 or strictly greater than the full extent,
///   * a non-owner block extent that is not the full extent,
///   * a block count of 1 (no real grid — nothing to expand),
///   * blockShape length neither logical-rank nor owner-dim count.
///
/// `physicalBlockShape` is accepted either rank-length (full on non-owner dims,
/// as authored today) or owner-dim-length (one extent per owner dim).
std::optional<MuPhysicalLayout>
resolveMuPhysicalLayout(mlir::MemRefType logicalType,
                        llvm::ArrayRef<int64_t> ownerDims,
                        llvm::ArrayRef<int64_t> blockShape);

std::optional<MuPhysicalLayout>
resolveMuPhysicalLayout(mlir::MemRefType logicalType,
                        mlir::ArrayAttr physicalOwnerDims,
                        mlir::ArrayAttr physicalBlockShape);

/// Build the flat rank-expanded MemRefType (normal form above). The element
/// type, memory space, and any identity layout are preserved.
mlir::MemRefType buildExpandedMuType(mlir::MemRefType logicalType,
                                     const MuPhysicalLayout &plan);

/// Recover owner dims PURELY from the expanded memref type plus the logical
/// shape — the structural proof obligation `ownerDims == recover(structure)`.
///
/// ND-general: recovers any number of owner dims. The returned owner dims are
/// ASCENDING by construction (the tiled trailing dims are scanned in ascending
/// logical-dim order and paired with the grid prefix in slot order), which
/// matches the ascending canonicalization in resolveMuPhysicalLayout. Returns
/// nullopt when the type is not a recognizable expansion of `logicalShape`
/// (rank mismatch, an ambiguous split, or grid dims without a matching tiled
/// dim) — callers treat nullopt as "out of scope / conservative", never as an
/// error.
std::optional<llvm::SmallVector<unsigned, 2>>
recoverOwnerDims(mlir::MemRefType expandedType,
                 llvm::ArrayRef<int64_t> logicalShape);

/// Physical layout recovered purely from a rank-expanded MU memref type (or
/// equivalent static shape). Owner dims are ASCENDING logical payload indices;
/// `physicalBlockShape` is rank-length with tile/block extents per payload dim.
/// Returns nullopt for flat/unexpanded shapes or ambiguous splits.
struct RecoveredMuPhysicalLayout {
  llvm::SmallVector<unsigned, 4> ownerDims;
  llvm::SmallVector<int64_t, 4> logicalShape;
  llvm::SmallVector<int64_t, 4> physicalBlockShape;
};

std::optional<RecoveredMuPhysicalLayout>
recoverMuPhysicalLayoutFromExpandedType(mlir::MemRefType expandedType);

std::optional<RecoveredMuPhysicalLayout>
recoverMuPhysicalLayoutFromExpandedShape(llvm::ArrayRef<int64_t> shape,
                                         mlir::Type elementType);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_MULAYOUT_H
