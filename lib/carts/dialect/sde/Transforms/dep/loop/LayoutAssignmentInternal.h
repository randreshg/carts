//===-- LayoutAssignmentInternal.h ----------------------------------------===//
//
// Internal interface shared by the layout-assignment carve:
//   * LayoutCandidateChoose.cpp owns candidate enumeration + assignment
//     (PhaseB/PhaseC) and the element-space block-sizing helpers, plus the
//     `sde-layout-candidate-choose` pass that commits the chosen writer home
//     layouts.
//   * WriterLayoutCommit.cpp owns the shared pass driver, per-SU writer/reader
//     reconciliation (witnessedWriterOwnerPositions, inferReaderRequiredLayout),
//     and PhaseD fact commit, plus the `sde-writer-layout-commit` pass and the
//     deprecated `sde-layout-assignment` alias.
//
// This header carries no policy; it is the thin call surface between the two
// translation units. The driver (`runLayoutAssignment`) keeps choice and commit
// atomic within a single pass — no metadata-promise handoff.
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

// Shared driver for the layout-assignment passes. Runs owner-loop promotion,
// PhaseA access relations, the BlockContraction-input map, PhaseB/PhaseC
// candidate choice, and PhaseD fact commit. `commitReaders` gates the
// reader-side reconciliation/commit: the full `sde-writer-layout-commit` (and
// the deprecated `sde-layout-assignment`) pass commit writers AND readers
// atomically; `sde-layout-candidate-choose` commits only the chosen writer home
// layouts. Choice and commit stay in one pass run — no metadata-promise handoff.
void runLayoutAssignment(::mlir::Operation *moduleOp, SDECostModel *costModel,
                         bool commitReaders);

} // namespace mlir::carts::sde::detail

#endif // CARTS_DIALECT_SDE_TRANSFORMS_DEP_LOOP_LAYOUTASSIGNMENTINTERNAL_H
