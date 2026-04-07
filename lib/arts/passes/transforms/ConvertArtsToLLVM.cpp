///==========================================================================///
/// File: ConvertArtsToLLVM.cpp
///
/// This file implements a pass to convert ARTS dialect operations into
/// LLVM dialect operations.
///
/// Example:
///   Before:
///     %g = arts.edt_create ...
///     arts.record_dep %g, ...
///     arts.wait_on_epoch %e
///
///   After:
///     %g = call @arts_edt_create(...)
///     call @arts_add_dependence(...)
///     call @arts_wait_on_handle(...)
///==========================================================================///

#include "arts/passes/transforms/ConvertArtsToLLVMInternal.h"

#include "arts/Dialect.h"
#include "arts/codegen/Codegen.h"
#define GEN_PASS_DEF_CONVERTARTSTOLLVM
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/abstract_machine/AbstractMachine.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include <memory>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(convert_arts_to_llvm);

using namespace mlir;
using namespace arts;

using namespace mlir::arts::convert_arts_to_llvm;

///===----------------------------------------------------------------------===///
/// Pass Implementation
///===----------------------------------------------------------------------===///
namespace {
struct ConvertArtsToLLVMPass
    : public impl::ConvertArtsToLLVMBase<ConvertArtsToLLVMPass> {

  explicit ConvertArtsToLLVMPass(bool debug = false,
                                 bool distributedInitPerWorker = false,
                                 const AbstractMachine *machine = nullptr)
      : debugMode(debug), distributedInitPerWorker(distributedInitPerWorker),
        machine(machine) {}

  void runOnOperation() override;

private:
  ArtsCodegen *AC = nullptr;
  bool debugMode = false;
  bool distributedInitPerWorker = false;
  const AbstractMachine *machine = nullptr;
};
} // namespace

///===----------------------------------------------------------------------===///
/// Core Pass Implementation
///===----------------------------------------------------------------------===///

void ConvertArtsToLLVMPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *context = &getContext();

  ARTS_INFO_HEADER(ConvertArtsToLLVMPass);
  ARTS_DEBUG_REGION(module.dump(););

  //// Initialize codegen infrastructure
  auto ownedAC = std::make_unique<ArtsCodegen>(module, debugMode);
  AC = ownedAC.get();
  AC->setDistributedInitInWorkers(distributedInitPerWorker);
  AC->setAbstractMachine(machine);
  ARTS_DEBUG_TYPE("ArtsCodegen initialized successfully");

  //// Apply patterns with greedy rewriter (two runs)
  GreedyRewriteConfig config;
  config.setUseTopDownTraversal(true);
  config.setRegionSimplificationLevel(GreedySimplifyRegionLevel::Aggressive);

  /// Run 1: core patterns
  {
    ARTS_INFO("Running core patterns");
    RewritePatternSet corePatterns(context);
    convert_arts_to_llvm::populateCorePatterns(corePatterns, AC);
    if (failed(
            applyPatternsGreedily(module, std::move(corePatterns), config))) {
      ARTS_ERROR("Failed to apply core ARTS to LLVM conversion patterns");
      return signalPassFailure();
    }
  }

  /// Run 2: Db Patterns
  {
    ARTS_INFO("Running db patterns");
    RewritePatternSet dbPatterns(context);
    convert_arts_to_llvm::populateDbPatterns(dbPatterns, AC);
    if (failed(applyPatternsGreedily(module, std::move(dbPatterns), config))) {
      ARTS_ERROR("Failed to apply DbAcquire/DbRelease conversion patterns");
      return signalPassFailure();
    }
  }

  /// Run 3: Other Patterns
  {
    ARTS_INFO("Running other patterns");
    RewritePatternSet otherPatterns(context);
    convert_arts_to_llvm::populateOtherPatterns(otherPatterns, AC);
    if (failed(
            applyPatternsGreedily(module, std::move(otherPatterns), config))) {
      ARTS_ERROR("Failed to apply other conversion patterns");
      return signalPassFailure();
    }
  }
  //// Initialize runtime
  AC->initRT(AC->getUnknownLoc());

  //// Cleanup
  AC = nullptr;

  ARTS_INFO_FOOTER(ConvertArtsToLLVMPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Pass Functions
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertArtsToLLVMPass() {
  return std::make_unique<ConvertArtsToLLVMPass>();
}

std::unique_ptr<Pass>
createConvertArtsToLLVMPass(bool debug, bool distributedInitPerWorker,
                            const AbstractMachine *machine) {
  return std::make_unique<ConvertArtsToLLVMPass>(
      debug, distributedInitPerWorker, machine);
}
} // namespace arts
} // namespace mlir
