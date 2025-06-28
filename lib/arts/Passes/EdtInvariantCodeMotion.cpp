///==========================================================================
/// File: EdtInvariantCodeMotion.cpp
///==========================================================================

/// Dialects
#include "ArtsPassDetails.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Pass/Pass.h"

/// Arts
#include "arts/ArtsDialect.h"

/// Others
#include "arts/Transforms/EdtInvariantCodeMotion.h"

/// LLVM support
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "edt-invariant-code-motion"
#define LINE "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

namespace {
struct EdtInvariantCodeMotionPass
    : public arts::EdtInvariantCodeMotionBase<EdtInvariantCodeMotionPass> {
  void runOnOperation() override;
};
} // end anonymous namespace

void EdtInvariantCodeMotionPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG({
    dbgs() << "\n" << LINE << "EdtInvariantCodeMotionPass STARTED\n" << LINE;
    module.dump();
  });

  bool changed = false;
  /// Walk through all EdtOp instances in the module.
  module.walk([&](arts::EdtOp edtOp) {
    LLVM_DEBUG(dbgs() << LINE; DBGS() << "Processing EDT:\n" << edtOp << "\n";);

    /// Use the new function to move invariant code out of this EDT.
    auto movedCount = moveEdtInvariantCode(edtOp);
    if (movedCount > 0) {
      changed = true;
    }

    LLVM_DEBUG(dbgs() << "Moved " << movedCount
                      << " operations out of EDT.\n";);
  });

  // Optionally, mark analysis as preserved or invalidated if needed,
  // depending on what the pass manager expects and what analyses are affected.
  // if (!changed)
  //  markAllAnalysesPreserved();

  LLVM_DEBUG({
    dbgs() << "\n"
           << LINE << "EdtInvariantCodeMotionPass FINISHED (changed=" << changed
           << ")\n"
           << LINE;
    module.dump();
  });
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtInvariantCodeMotionPass() {
  return std::make_unique<EdtInvariantCodeMotionPass>();
}

} // namespace arts
} // namespace mlir
