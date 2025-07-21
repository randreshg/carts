///==========================================================================
/// File: EdtPtrRematerialization.cpp
///==========================================================================

/// Dialects
#include "ArtsPassDetails.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Pass/Pass.h"

/// Arts
#include "arts/ArtsDialect.h"

/// Others
#include "arts/Transforms/EdtPtrRematerialization.h"

/// LLVM support
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "edt-ptr-rematerialization"
#define LINE "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

namespace {
struct EdtPtrRematerializationPass
    : public arts::EdtPtrRematerializationBase<
          EdtPtrRematerializationPass> {
  void runOnOperation() override;
};
} // end anonymous namespace

void EdtPtrRematerializationPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG({
    dbgs() << "\n"
           << LINE << "EdtPtrRematerializationPass STARTED\n"
           << LINE;
    module.dump();
  });

  /// Walk through all EdtOp instances in the module.
  module.walk([&](arts::EdtOp edtOp) { rematerializePointersInEdt(edtOp); });

  LLVM_DEBUG({
    dbgs() << "\n"
           << LINE << "EdtPtrRematerializationPass FINISHED\n"
           << LINE;
    module.dump();
  });
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtPtrRematerializationPass() {
  return std::make_unique<EdtPtrRematerializationPass>();
}

} // namespace arts
} // namespace mlir
