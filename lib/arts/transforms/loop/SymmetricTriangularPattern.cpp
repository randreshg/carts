///==========================================================================///
/// SymmetricTriangularPattern.cpp - Triangular → rectangular normalization
///
/// Detects triangular loops with symmetric writes and converts them to
/// rectangular form for DbPartitioning.
///
/// BEFORE:
///   arts.for %i = 0 to %m {
///     store %one, %C[%i, %i]              // diagonal init
///     %lb = arith.addi %i, %c1
///     scf.for %j = %lb to %m {            // triangular
///       ... body producing %result ...
///       store %result, %C[%i, %j]         // upper
///       store %result, %C[%j, %i]         // lower (symmetric)
///     }
///   }
///
/// AFTER:
///   arts.for %i = 0 to %m {
///     scf.for %j = %c0 to %m {            // rectangular
///       ... body producing %result ...
///       store %result, %C[%i, %j]         // single write per element
///     }
///     store %one, %C[%i, %i]              // diagonal AFTER j-loop
///   }
///
/// Trade-off: 2x computation for rectangular bounds enabling uniform
/// partitioning and eliminating cross-partition symmetric writes.
///==========================================================================///

#include "arts/analysis/db/DbPatternMatchers.h"
#include "arts/transforms/loop/LoopNormalizer.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"

ARTS_DEBUG_SETUP(loop_normalization);

using namespace mlir;
using namespace mlir::arts;

namespace mlir {
namespace arts {

class SymmetricTriangularPattern : public LoopPattern {
public:
  explicit SymmetricTriangularPattern(MetadataManager &metadataManager)
      : metadataManager(metadataManager) {}

  bool match(ForOp artsFor) override;
  LogicalResult apply(OpBuilder &builder) override;
  StringRef getName() const override { return "symmetric-triangular"; }

private:
  MetadataManager &metadataManager;
  ForOp outerArtsFor;
  SymmetricTriangularPatternMatch matchResult;
};

///===----------------------------------------------------------------------===//
/// Match: detect triangular loop with symmetric writes
///===----------------------------------------------------------------------===//

bool SymmetricTriangularPattern::match(ForOp artsFor) {
  matchResult = {};
  if (!detectSymmetricTriangularPattern(artsFor, matchResult))
    return false;

  outerArtsFor = artsFor;
  return true;
}

///===----------------------------------------------------------------------===//
/// Apply: transform triangular → rectangular
///===----------------------------------------------------------------------===//

LogicalResult SymmetricTriangularPattern::apply(OpBuilder &builder) {
  auto &m = matchResult;
  Location loc = m.jLoop.getLoc();
  Value outerIV = outerArtsFor.getBody()->getArgument(0);

  ARTS_INFO("Normalizing symmetric-triangular loop (offset="
            << m.triangularOffset << ")");

  /// Step 1: Create %c0 constant for the new lower bound
  builder.setInsertionPoint(m.jLoop);
  Value c0 = arts::createZeroIndex(builder, loc);

  /// Step 2: Create new rectangular scf.for with lb=0, same ub and step
  Value ub = m.jLoop.getUpperBound();
  Value step = m.jLoop.getStep();
  Value oldJIV = m.jLoop.getInductionVar();

  /// Collect iter_args from original j-loop
  ValueRange initArgs = m.jLoop.getInitArgs();

  auto newJLoop = builder.create<scf::ForOp>(
      loc, c0, ub, step, initArgs,
      [&](OpBuilder &bodyBuilder, Location bodyLoc, Value newJIV,
          ValueRange iterArgs) {
        /// Clone the original j-loop body, remapping j IV
        IRMapping mapping;
        mapping.map(oldJIV, newJIV);

        /// Map iter_args if any
        auto oldIterArgs = m.jLoop.getRegionIterArgs();
        for (size_t i = 0; i < oldIterArgs.size(); ++i)
          mapping.map(oldIterArgs[i], iterArgs[i]);

        /// Clone all ops from original body
        for (Operation &op : m.jLoop.getBody()->without_terminator())
          bodyBuilder.clone(op, mapping);

        /// Find the symmetric store in the cloned body and erase it
        /// We need to find it by looking at the last cloned ops
        SmallVector<memref::StoreOp> clonedStores;
        Block *newBody = bodyBuilder.getBlock();
        for (Operation &op : *newBody) {
          if (auto store = dyn_cast<memref::StoreOp>(&op))
            clonedStores.push_back(store);
        }

        /// Erase the symmetric (J,I) store from the cloned body.
        /// Identify it by indices: [newJIV, outerIV] pattern.
        for (auto store : clonedStores) {
          auto indices = store.getIndices();
          if (indices.size() == 2 && indices[0] == newJIV &&
              indices[1] == outerIV) {
            store.erase();
            break;
          }
        }

        /// Clone the yield from the original body
        auto *origYield = m.jLoop.getBody()->getTerminator();
        bodyBuilder.clone(*origYield, mapping);
      });

  /// Step 3: Copy loop attributes to new j-loop
  rewriteNormalizedLoop(m.jLoop.getOperation(), newJLoop.getOperation(),
                        metadataManager);

  /// Step 4: Handle diagonal store — move it after the new j-loop
  if (m.diagStore) {
    /// Erase the original diagonal store (which was before the j-loop)
    m.diagStore->moveBefore(outerArtsFor.getBody()->getTerminator());
  }

  /// Step 5: Replace uses of j-loop results (if any iter_args)
  m.jLoop.replaceAllUsesWith(newJLoop.getResults());

  /// Step 6: Erase the original j-loop
  m.jLoop.erase();

  ARTS_INFO("Applied symmetric-triangular normalization");
  return success();
}

/// Factory function — called from LoopNormalization pass
std::unique_ptr<LoopPattern>
createSymmetricTriangularPattern(MetadataManager &metadataManager) {
  return std::make_unique<SymmetricTriangularPattern>(metadataManager);
}

} // namespace arts
} // namespace mlir
