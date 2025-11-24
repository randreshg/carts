///==========================================================================///
/// File: CanonicalizeMemrefs.cpp
///
/// This pass canonicalizes memref allocations and OpenMP task dependencies.
/// It orchestrates three transformers:
/// 1. NestedAllocTransformer - Converts nested arrays to N-D arrays
/// 2. ArrayOfPtrsTransformer - Eliminates wrapper allocas
/// 3. OmpDepsTransformer - Canonicalizes OpenMP task dependencies
///==========================================================================///

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Transforms/ArrayOfPtrsTransformer.h"
#include "arts/Transforms/NestedAllocTransformer.h"
#include "arts/Transforms/OmpDepsTransformer.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/OpRemovalManager.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/Pass/Pass.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>
#include <string>

/// Debug
ARTS_DEBUG_SETUP(canonicalize_memrefs);

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::omp;

///===----------------------------------------------------------------------===///
// Pass Implementation
///===----------------------------------------------------------------------===///

struct CanonicalizeMemrefsPass
    : public arts::CanonicalizeMemrefsBase<CanonicalizeMemrefsPass> {

  void runOnOperation() override;

private:
  /// Verification: Ensure all allocations are canonical form
  void verifyAllCanonical();
};

///===----------------------------------------------------------------------===///
// Main Pass Implementation
///===----------------------------------------------------------------------===///

void CanonicalizeMemrefsPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *ctx = module.getContext();

  ARTS_INFO_HEADER(CanonicalizeMemrefsPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Verification: Count OpenMP task operations before pass
  unsigned numOmpTasksBefore = 0;
  module.walk([&](omp::TaskOp task) { ++numOmpTasksBefore; });
  ARTS_DEBUG("  - OpenMP task count before pass: " << numOmpTasksBefore);

  OpBuilder builder(ctx);
  OpRemovalManager removalMgr;

  /// Phase 1: Detect and transform nested allocations (arrays-of-arrays)
  ARTS_INFO("Phase 1: Detecting and transforming nested allocations");
  {
    NestedAllocTransformer nestedTransformer(ctx);
    if (failed(nestedTransformer.transform(module, builder, removalMgr))) {
      module.emitError("Failed to canonicalize nested allocations");
      signalPassFailure();
      return;
    }

    auto stats = nestedTransformer.getStatistics();
    if (stats.patternsTransformed > 0) {
      ARTS_INFO("  - Transformed " << stats.patternsTransformed
                                   << " nested allocation patterns");
      ARTS_DEBUG("    - Accesses rewritten: " << stats.accessesRewritten);
      ARTS_DEBUG("    - Allocations removed: " << stats.allocationsRemoved);
    } else {
      ARTS_DEBUG("  - No nested allocation patterns found");
    }
  }

  /// Phase 2: Detect and transform array-of-pointers patterns
  ARTS_INFO("Phase 2: Detecting and transforming array-of-pointers patterns");
  {
    ArrayOfPtrsTransformer arrayTransformer(ctx);
    if (failed(arrayTransformer.transform(module, builder, removalMgr))) {
      module.emitError("Failed to canonicalize array-of-pointers patterns");
      signalPassFailure();
      return;
    }

    auto stats = arrayTransformer.getStatistics();
    if (stats.patternsTransformed > 0) {
      ARTS_INFO("  - Transformed " << stats.patternsTransformed
                                   << " array-of-pointers patterns");
      ARTS_DEBUG("    - Accesses rewritten: " << stats.accessesRewritten);
    } else {
      ARTS_DEBUG("  - No array-of-pointers patterns found");
    }
  }

  /// Phase 3: Canonicalize OpenMP Dependencies
  ARTS_INFO("Phase 3: Canonicalizing OpenMP task dependencies");
  {
    OmpDepsTransformer ompTransformer(ctx);
    if (failed(ompTransformer.transform(module, builder))) {
      module.emitError("Failed to canonicalize OpenMP dependencies");
      signalPassFailure();
      return;
    }

    auto stats = ompTransformer.getStatistics();
    if (stats.tasksProcessed > 0) {
      ARTS_DEBUG("  - Processed " << stats.tasksProcessed << " OpenMP tasks");
      ARTS_DEBUG("    - Dependencies canonicalized: " << stats.depsCanonical);
    }
  }

  /// Phase 4: Cleanup - Remove all marked operations
  ARTS_INFO("Phase 4: Removing legacy allocations and operations");
  if (!removalMgr.empty()) {
    ARTS_DEBUG("  - Removing " << removalMgr.size() << " operations");
    removalMgr.removeAllMarked(module, /*recursive=*/true);
  }

  /// Verification: Count OpenMP task operations after pass
  unsigned numOmpTasksAfter = 0;
  module.walk([&](omp::TaskOp task) { ++numOmpTasksAfter; });
  ARTS_DEBUG("  - OpenMP task count after pass: " << numOmpTasksAfter);

  if (numOmpTasksBefore != numOmpTasksAfter) {
    module.emitError("CanonicalizeMemrefsPass verification failed: OpenMP task "
                     "count mismatch (before: ")
        << numOmpTasksBefore << ", after: " << numOmpTasksAfter << ")";
    signalPassFailure();
    return;
  }

  /// Verify all allocations are in canonical form
  verifyAllCanonical();

  ARTS_INFO_FOOTER(CanonicalizeMemrefsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
// Verification
///===----------------------------------------------------------------------===///

void CanonicalizeMemrefsPass::verifyAllCanonical() {
  ModuleOp module = getOperation();
  bool hasIssues = false;

  /// Verify no nested memref types remain
  module.walk([&](Operation *op) {
    if (auto allocOp = dyn_cast<memref::AllocOp>(op)) {
      auto memrefType = allocOp.getType();
      if (auto elemType = memrefType.getElementType().dyn_cast<MemRefType>()) {
        allocOp->emitError("CanonicalizeMemrefsPass verification failed: "
                           "Found nested memref allocation (memref-of-memref) "
                           "after canonicalization");
        hasIssues = true;
      }
    }
    if (auto allocaOp = dyn_cast<memref::AllocaOp>(op)) {
      auto memrefType = allocaOp.getType();
      if (memrefType.getRank() == 0) {
        if (auto elemType =
                memrefType.getElementType().dyn_cast<MemRefType>()) {
          allocaOp->emitError(
              "CanonicalizeMemrefsPass verification failed: Found rank-0 "
              "alloca storing a memref after canonicalization");
          hasIssues = true;
        }
      }
    }

    /// Verify memref.load operations have correct number of indices
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      auto memrefType = loadOp.getMemRefType();
      unsigned memrefRank = memrefType.getRank();
      unsigned numIndices = loadOp.getIndices().size();

      if (numIndices != memrefRank) {
        op->emitError(
            "CanonicalizeMemrefsPass verification failed: memref.load "
            "has ")
            << numIndices << " indices but memref type has rank " << memrefRank
            << ". This indicates strided or multi-dimensional access patterns "
            << "that were not properly canonicalized. The source code may use "
            << "VLA pointer casts or other memory layouts that CARTS does not "
            << "currently support. Consider rewriting the code to use "
            << "explicit multi-dimensional arrays (e.g., malloc for each "
               "dimension).";
        hasIssues = true;
      }
    }

    /// Verify memref.store operations have correct number of indices
    if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      auto memrefType = storeOp.getMemRefType();
      unsigned memrefRank = memrefType.getRank();
      unsigned numIndices = storeOp.getIndices().size();

      if (numIndices != memrefRank) {
        op->emitError(
            "CanonicalizeMemrefsPass verification failed: memref.store "
            "has ")
            << numIndices << " indices but memref type has rank " << memrefRank
            << ". This indicates strided or multi-dimensional access patterns "
            << "that were not properly canonicalized. The source code may use "
            << "VLA pointer casts or other memory layouts that CARTS does not "
            << "currently support. Consider rewriting the code to use "
            << "explicit multi-dimensional arrays (e.g., malloc for each "
               "dimension).";
        hasIssues = true;
      }
    }
  });

  if (hasIssues) {
    module.emitError(
        "CanonicalizeMemrefsPass verification failed: Non-canonical memref "
        "operations remain after transformation. This typically occurs when "
        "source code uses VLA pointer casts or other layouts that cannot be "
        "canonicalized; please rewrite to use explicit multi-dimensional "
        "allocations.");
    signalPassFailure();
  }
}

///===----------------------------------------------------------------------===///
// Pass Registration
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createCanonicalizeMemrefsPass() {
  return std::make_unique<CanonicalizeMemrefsPass>();
}

} // namespace arts
} // namespace mlir
