//===-- LayoutAssignmentInternal.h ----------------------------------------===//
//
// Internal interface shared by the layout-assignment carve:
//   * LayoutCandidateChoose.cpp owns candidate enumeration + assignment
//     (PhaseB/PhaseC) and the element-space block-sizing helpers, plus the
//     `sde-layout-candidate-choose` pass that CHOOSES each array's owner layout
//     and COMMITS it as a real authoritative choice fact for the next pass.
//   * WriterLayoutCommit.cpp owns the shared PhaseD commit driver, per-SU
//     writer/reader reconciliation (witnessedWriterOwnerPositions,
//     inferReaderRequiredLayout), and the choice-fact (de)serialization, plus
//     the `sde-writer-layout-commit` pass and the deprecated
//     `sde-layout-assignment` alias.
//
// This header carries no policy; it is the thin call surface between the two
// translation units. The layout engine is a two-pass split with a legal
// committed-fact handoff: `sde-layout-candidate-choose` decides+commits the
// chosen logical layout, and `sde-writer-layout-commit` consumes that committed
// fact and realizes the per-SU writer/reader/physical facts (it does not
// re-choose). The net IR after the two passes equals the atomic
// `runLayoutAssignment` exactly — choice is committed, never a repair-promise.
//
//===----------------------------------------------------------------------===//

#ifndef CARTS_DIALECT_SDE_TRANSFORMS_DEP_LOOP_LAYOUTASSIGNMENTINTERNAL_H
#define CARTS_DIALECT_SDE_TRANSFORMS_DEP_LOOP_LAYOUTASSIGNMENTINTERNAL_H

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"

#include "mlir/IR/Value.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

#include <optional>

namespace mlir {
class Operation;
} // namespace mlir

namespace mlir::carts::sde {
class SDECostModel;
} // namespace mlir::carts::sde

namespace mlir::carts::sde::detail {

// Node-agnostic DB/MU block-byte budget: block count grows with problem size,
// not node/worker count. Runtime ownership routing is derived later in ARTS.
inline constexpr int64_t kTargetBlockBytes = 2 * 1024 * 1024;

// PhaseC result for one array: the chosen owner layout plus the readers whose
// access geometry disagrees with it (they re-derive a required read layout).
struct ChosenLayout {
  ArrayLayoutCandidate layout;
  llvm::SmallDenseSet<unsigned, 4> disagreeingReaders;
};

// Block count implied by an owner partition (delegates to the tested
// MU-partition inference).
int64_t computeMuBlockCount(llvm::ArrayRef<int64_t> shape,
                            llvm::ArrayRef<int64_t> ownerPositions,
                            llvm::ArrayRef<int64_t> blockShape);

// Element byte width of an array root, or 0 when not int/float.
int64_t elementBytes(Value root);

// Owner-block shape sized so each block's footprint nears `targetBytes`.
SmallVector<int64_t, 4>
blockShapeFromBudget(llvm::ArrayRef<int64_t> staticShape, int64_t elemBytes,
                     llvm::ArrayRef<int64_t> ownerPositions,
                     int64_t targetBytes);

// A block-layout candidate over `ownerPositions` with the abstract block
// extent. Shared with reader-required-layout derivation in WriterLayoutCommit.
ArrayLayoutCandidate makeBlockCandidate(const ArrayAccessProfile &profile,
                                        llvm::ArrayRef<int64_t> ownerPositions,
                                        ArrayLayoutKind kind);

// PhaseB + PhaseC: enumerate candidate layouts for one array and structurally
// choose one owner layout from its access relations.
ChosenLayout assignLayout(const ArrayAccessProfile &profile,
                          std::optional<unsigned> contractionPosition,
                          bool preserveFullWriterOwnerTile);

// Atomic driver (deprecated `sde-layout-assignment`, and the standalone
// fallback when no committed choice fact is present). Runs owner-loop
// promotion, PhaseA access relations, the BlockContraction-input map,
// PhaseB/PhaseC candidate choice, and PhaseD writer+reader fact commit in a
// single pass run.
void runLayoutAssignment(::mlir::Operation *moduleOp, SDECostModel *costModel);

// Pass 1 (`sde-layout-candidate-choose`): owner-loop promotion + PhaseA +
// PhaseB/PhaseC choice, then COMMIT the chosen logical layout per array as a
// real authoritative SDE choice fact (a module attribute) for pass 2 to
// consume. It performs no per-SU writer/reader/physical realization. Fails
// closed (no fact) only when there is nothing to distribute (no cost model /
// single worker / no arrays).
void chooseAndCommitChoiceFact(::mlir::Operation *moduleOp,
                               SDECostModel *costModel);

// Pass 2 (`sde-writer-layout-commit`): CONSUME the committed choice fact from
// pass 1 and realize PhaseD writer+reader+physical facts over it, then erase
// the now-realized choice fact. It does not re-choose or re-derive a different
// layout. When no choice fact is present (standalone invocation), it falls back
// to the self-contained atomic `runLayoutAssignment`.
void consumeChoiceFactAndCommit(::mlir::Operation *moduleOp,
                                SDECostModel *costModel);

} // namespace mlir::carts::sde::detail

#endif // CARTS_DIALECT_SDE_TRANSFORMS_DEP_LOOP_LAYOUTASSIGNMENTINTERNAL_H
