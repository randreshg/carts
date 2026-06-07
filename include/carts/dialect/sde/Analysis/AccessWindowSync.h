///==========================================================================///
/// File: AccessWindowSync.h
///
/// Cross-barrier SU-ordering analysis from raised MU access windows.
///
/// A `sde.su_barrier` is the last-resort global ordering point between the CU
/// phase immediately before it and the CU phase immediately after it. Once
/// `raise-to-mu-access-window` has stamped per-CU `sde.mu_access_window` facts,
/// whether that barrier orders a real dependency is a structural fact, not a
/// guess: it can be read off the block-grid windows (`[blockLo, blockHi)` per
/// owner dim) the two phases touch.
///
/// This analysis is the single source of truth shared by the
/// `sde-mu-access-window-sync-opt` transform (which removes barriers it proves
/// redundant) and `verify-sde-mu-access-window-sync` (which proves every
/// surviving barrier is justified, or fails closed with evidence). Because both
/// read the same classifier, the transform removes exactly what the verifier
/// would otherwise reject as redundant. It reads current IR only and never
/// introduces a token, slice, dependency-graph, or distribution structure.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_ACCESSWINDOWSYNC_H
#define CARTS_DIALECT_SDE_ANALYSIS_ACCESSWINDOWSYNC_H

#include "carts/dialect/sde/IR/SdeDialect.h"

#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SmallVector.h"

#include <utility>

namespace mlir {
class Block;
} // namespace mlir

namespace mlir::carts::sde {

/// The cross-barrier relationship between the CU phases an `sde.su_barrier`
/// separates, classified from their raised MU access windows.
enum class BarrierSyncVerdict {
  /// No access-window surface surrounds the barrier (one phase is empty, or
  /// neither phase carries a window). Conservative skip: ordinary pre-raise IR
  /// is never reasoned about here; window coverage is
  /// `verify-sde-mu-access-window`'s job.
  OutOfScope,
  /// A window's block arrays are malformed, so the ordering cannot be proven.
  Malformed,
  /// A write on one side overlaps an access on the other and any read stays
  /// within the written blocks (aligned RAW/WAW/WAR). The barrier orders a real
  /// dependency: keep it.
  Justified,
  /// Every shared MU is touched on disjoint blocks or read-only on both sides,
  /// and both phases are fully described by windows. The ordered CUs are
  /// independent: the barrier is avoidable and can be removed.
  Redundant,
  /// A consumer reads blocks of a shared MU the producer does not write. The
  /// data must move; that redistribution is a later SDE distribution transform,
  /// not an ordering. Diagnosed, never rewritten.
  Misaligned,
  /// Windows on a shared MU disagree on owner-dim rank: the ordering cannot be
  /// proven.
  RankMismatch,
  /// An ordered CU has a memory access no window describes: the ordering cannot
  /// be proven.
  Unprovable,
};

/// Classify the cross-barrier relationship between the CU phase immediately
/// `before` an `sde.su_barrier` and the phase immediately `after` it, read off
/// their raised `sde.mu_access_window` facts. The block-grain dependency is at
/// `[blockLo, blockHi)`; the in-tile `validExtents` describes coverage within a
/// block and is not part of the cross-CU block relationship.
BarrierSyncVerdict classifyBarrierSync(ArrayRef<SdeCuRegionOp> before,
                                       ArrayRef<SdeCuRegionOp> after);

/// A block split into CU phases separated by `sde.su_barrier`s, in program
/// order. `phases[i]` holds the CUs of phase `i`; each entry in `barriers`
/// pairs a barrier with the index of the phase that precedes it (so the phase
/// after it is `phases[index + 1]`).
struct BarrierSyncPartition {
  SmallVector<SmallVector<SdeCuRegionOp, 4>, 4> phases;
  SmallVector<std::pair<SdeSuBarrierOp, unsigned>, 4> barriers;
};

/// Split `block` into the CU phases and barriers `classifyBarrierSync`
/// consumes.
BarrierSyncPartition partitionBarrierPhases(Block &block);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_ANALYSIS_ACCESSWINDOWSYNC_H
