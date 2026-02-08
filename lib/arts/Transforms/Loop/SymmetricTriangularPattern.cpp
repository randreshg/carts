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

#include "arts/Transforms/Loop/LoopNormalizer.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"

ARTS_DEBUG_SETUP(loop_normalization);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Match result for symmetric-triangular detection.
struct SymmetricTriangularMatch {
  ForOp outerArtsFor;
  scf::ForOp jLoop;
  int64_t triangularOffset = 0;
  memref::StoreOp storeIJ;      /// store to C[i,j]
  memref::StoreOp storeJI;      /// store to C[j,i] (symmetric)
  Value memC;                    /// target memref
  memref::StoreOp diagStore;    /// optional diagonal store
  Value diagValue;               /// value stored on diagonal
};

/// Check if two stores write the same value to the same memref with
/// swapped 2D indices: one stores at [outerIV, innerIV] and the other at
/// [innerIV, outerIV].
static bool areSymmetricStores(memref::StoreOp s1, memref::StoreOp s2,
                               Value outerIV, Value innerIV) {
  /// Same value stored
  if (s1.getValueToStore() != s2.getValueToStore())
    return false;

  /// Same memref
  if (s1.getMemRef() != s2.getMemRef())
    return false;

  /// Must be 2D stores
  auto idx1 = s1.getIndices();
  auto idx2 = s2.getIndices();
  if (idx1.size() != 2 || idx2.size() != 2)
    return false;

  /// Check for swapped indices: s1=[outerIV, innerIV], s2=[innerIV, outerIV]
  bool s1IsIJ = (idx1[0] == outerIV && idx1[1] == innerIV);
  bool s2IsJI = (idx2[0] == innerIV && idx2[1] == outerIV);
  if (s1IsIJ && s2IsJI)
    return true;

  /// Or the other way around
  bool s1IsJI = (idx1[0] == innerIV && idx1[1] == outerIV);
  bool s2IsIJ = (idx2[0] == outerIV && idx2[1] == innerIV);
  return s1IsJI && s2IsIJ;
}

/// Check if a store writes to the diagonal: memref[iv, iv].
static bool isDiagonalStore(memref::StoreOp store, Value iv) {
  auto indices = store.getIndices();
  return indices.size() == 2 && indices[0] == iv && indices[1] == iv;
}

} // namespace

namespace mlir {
namespace arts {

class SymmetricTriangularPattern : public LoopPattern {
public:
  bool match(ForOp artsFor) override;
  LogicalResult apply(OpBuilder &builder) override;
  StringRef getName() const override { return "symmetric-triangular"; }

private:
  SymmetricTriangularMatch matchResult;

  bool detectTriangularBound(scf::ForOp jLoop, Value outerIV);
  bool detectSymmetricStores(scf::ForOp jLoop, Value outerIV, Value jIV);
  bool detectDiagonalInit(ForOp artsFor, Value outerIV, Value memC);
};

///===----------------------------------------------------------------------===//
/// Match: detect triangular loop with symmetric writes
///===----------------------------------------------------------------------===//

bool SymmetricTriangularPattern::match(ForOp artsFor) {
  matchResult = {};
  matchResult.outerArtsFor = artsFor;

  Value outerIV = artsFor.getBody()->getArgument(0);

  /// Find the first scf.for inside this arts.for
  scf::ForOp jLoop = nullptr;
  for (Operation &op : artsFor.getBody()->without_terminator()) {
    if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
      jLoop = forOp;
      break;
    }
  }
  if (!jLoop)
    return false;

  /// 1. Check triangular lower bound: lb = arith.addi(outerIV, const)
  if (!detectTriangularBound(jLoop, outerIV))
    return false;

  /// 2. Symmetric matrix: outer and inner loops must share the same upper bound
  auto outerUBRange = artsFor.getUpperBound();
  if (outerUBRange.size() != 1)
    return false;
  Value outerUB = outerUBRange[0];
  Value innerUB = jLoop.getUpperBound();
  if (outerUB != innerUB)
    return false;

  /// 3. The inner scf.for must be the only for-loop in the outer body
  int forCount = 0;
  for (Operation &op : artsFor.getBody()->without_terminator())
    if (isa<scf::ForOp>(&op))
      forCount++;
  if (forCount != 1)
    return false;

  /// 4. Check for symmetric stores in j-loop body
  Value jIV = jLoop.getInductionVar();
  if (!detectSymmetricStores(jLoop, outerIV, jIV))
    return false;

  /// 5. Require diagonal init: store %val, %C[%i, %i] — characteristic of
  /// correlation/covariance symmetric matrix initialization
  if (!detectDiagonalInit(artsFor, outerIV, matchResult.memC))
    return false;

  matchResult.jLoop = jLoop;
  return true;
}

bool SymmetricTriangularPattern::detectTriangularBound(scf::ForOp jLoop,
                                                       Value outerIV) {
  auto offset = getTriangularOffset(jLoop.getLowerBound(), outerIV);
  if (!offset)
    return false;
  matchResult.triangularOffset = *offset;
  return true;
}

bool SymmetricTriangularPattern::detectSymmetricStores(scf::ForOp jLoop,
                                                       Value outerIV,
                                                       Value jIV) {
  /// Collect all memref.store ops in the j-loop body (not nested in sub-loops)
  SmallVector<memref::StoreOp> stores;
  for (Operation &op : jLoop.getBody()->without_terminator()) {
    if (auto store = dyn_cast<memref::StoreOp>(&op))
      stores.push_back(store);
  }

  /// Need exactly 2 stores at the top-level of the j-loop body
  if (stores.size() != 2)
    return false;

  /// Check if they form a symmetric pair
  if (!areSymmetricStores(stores[0], stores[1], outerIV, jIV))
    return false;

  /// Determine which is [i,j] and which is [j,i]
  auto idx0 = stores[0].getIndices();
  if (idx0[0] == outerIV && idx0[1] == jIV) {
    matchResult.storeIJ = stores[0];
    matchResult.storeJI = stores[1];
  } else {
    matchResult.storeIJ = stores[1];
    matchResult.storeJI = stores[0];
  }

  matchResult.memC = stores[0].getMemRef();
  return true;
}

bool SymmetricTriangularPattern::detectDiagonalInit(ForOp artsFor,
                                                    Value outerIV,
                                                    Value memC) {
  for (Operation &op : artsFor.getBody()->without_terminator()) {
    if (auto store = dyn_cast<memref::StoreOp>(&op)) {
      if (store.getMemRef() == memC && isDiagonalStore(store, outerIV)) {
        matchResult.diagStore = store;
        matchResult.diagValue = store.getValueToStore();
        return true;
      }
    }
  }
  return false;
}

///===----------------------------------------------------------------------===//
/// Apply: transform triangular → rectangular
///===----------------------------------------------------------------------===//

LogicalResult SymmetricTriangularPattern::apply(OpBuilder &builder) {
  auto &m = matchResult;
  Location loc = m.jLoop.getLoc();
  Value outerIV = m.outerArtsFor.getBody()->getArgument(0);

  ARTS_INFO("Normalizing symmetric-triangular loop (offset="
            << m.triangularOffset << ")");

  /// Step 1: Create %c0 constant for the new lower bound
  builder.setInsertionPoint(m.jLoop);
  Value c0 = builder.create<arith::ConstantIndexOp>(loc, 0);

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
  copyLoopAttributes(m.jLoop.getOperation(), newJLoop.getOperation());

  /// Step 4: Update tripCount metadata — the loop is now rectangular (full ub)
  /// The old tripCount was dynamic (0) for triangular; set to 0 to keep it
  /// dynamic, since actual trip count depends on the upper bound at runtime.

  /// Step 5: Handle diagonal store — move it after the new j-loop
  if (m.diagStore) {
    /// Erase the original diagonal store (which was before the j-loop)
    m.diagStore->moveBefore(m.outerArtsFor.getBody()->getTerminator());
  }

  /// Step 6: Replace uses of j-loop results (if any iter_args)
  m.jLoop.replaceAllUsesWith(newJLoop.getResults());

  /// Step 7: Erase the original j-loop
  m.jLoop.erase();

  ARTS_INFO("Applied symmetric-triangular normalization");
  return success();
}

/// Factory function — called from LoopNormalization pass
std::unique_ptr<LoopPattern> createSymmetricTriangularPattern() {
  return std::make_unique<SymmetricTriangularPattern>();
}

} // namespace arts
} // namespace mlir
