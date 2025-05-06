///==========================================================================
/// File: EdtPointerRematerialization.cpp
///==========================================================================

/// Dialects
#include "ArtsPassDetails.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Pass/Pass.h"

/// Arts
#include "arts/ArtsDialect.h"

/// Others
#include "arts/Transforms/EdtPointerRematerialization.h"

/// LLVM support
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "edt-pointer-rematerialization"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

namespace {
struct EdtPointerRematerializationPass
    : public arts::EdtPointerRematerializationBase<
          EdtPointerRematerializationPass> {
  void runOnOperation() override;
};
} // end anonymous namespace

void EdtPointerRematerializationPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG({
    dbgs() << "\n"
           << line << "EdtPointerRematerializationPass STARTED\n"
           << line;
    module.dump();
  });

  /// Walk through all EdtOp instances in the module.
  module.walk([&](arts::EdtOp edtOp) { rematerializePointersInEdt(edtOp); });

  LLVM_DEBUG({
    dbgs() << "\n"
           << line << "EdtPointerRematerializationPass FINISHED\n"
           << line;
    module.dump();
  });
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtPointerRematerializationPass() {
  return std::make_unique<EdtPointerRematerializationPass>();
}

} // namespace arts
} // namespace mlir
