///==========================================================================///
/// File: GpuEligibilityAnalysis.cpp
///
/// Analyze arts.for loops for GPU eligibility and attach profitability
/// metadata to guide GPU lowering.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Interfaces/DataLayoutInterfaces.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SmallPtrSet.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(gpu_eligibility_analysis);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct GpuEligibilityAnalysisPass
    : public arts::GpuEligibilityAnalysisBase<GpuEligibilityAnalysisPass> {
  GpuEligibilityAnalysisPass() = default;
  explicit GpuEligibilityAnalysisPass(llvm::StringRef config) {
    configPath = config.str();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    ArtsAbstractMachine machine(configPath);
    if (!machine.hasGpuSupport())
      return;

    MLIRContext *context = module.getContext();
    DataLayout dataLayout(module);

    module.walk([&](arts::ForOp forOp) {
      bool hasReductions = !forOp.getReductionAccumulators().empty();
      bool hasDeps = false;
      bool potentiallyParallel = true;
      int64_t tripCount = -1;

      if (auto loopAttr = forOp->getAttrOfType<LoopMetadataAttr>(
              AttrNames::LoopMetadata::Name)) {
        if (loopAttr.getHasInterIterationDeps())
          hasDeps = loopAttr.getHasInterIterationDeps().getValue();
        if (loopAttr.getHasReductions())
          hasReductions = loopAttr.getHasReductions().getValue();
        if (loopAttr.getPotentiallyParallel())
          potentiallyParallel = loopAttr.getPotentiallyParallel().getValue();
        if (loopAttr.getTripCount())
          tripCount = loopAttr.getTripCount().getInt();
      }

      if (tripCount < 0) {
        int64_t lb = 0, ub = 0, step = 1;
        if (ValueUtils::getConstantIndex(forOp.getLowerBound()[0], lb) &&
            ValueUtils::getConstantIndex(forOp.getUpperBound()[0], ub) &&
            ValueUtils::getConstantIndex(forOp.getStep()[0], step) &&
            step > 0) {
          if (ub > lb)
            tripCount = (ub - lb + step - 1) / step;
          else
            tripCount = 0;
        }
      }

      int64_t memoryBytes = 0;
      llvm::SmallPtrSet<Value, 8> seen;
      forOp.getRegion().walk([&](Operation *op) {
        Value memref;
        if (auto load = dyn_cast<memref::LoadOp>(op))
          memref = load.getMemref();
        else if (auto store = dyn_cast<memref::StoreOp>(op))
          memref = store.getMemref();
        else
          return;

        Value base = ValueUtils::getUnderlyingValue(memref);
        if (!seen.insert(base).second)
          return;

        auto memrefType = base.getType().dyn_cast<MemRefType>();
        if (!memrefType || !memrefType.hasStaticShape())
          return;

        int64_t elems = memrefType.getNumElements();
        if (elems <= 0)
          return;
        int64_t elemBits =
            dataLayout.getTypeSizeInBits(memrefType.getElementType());
        int64_t elemBytes = (elemBits + 7) / 8;
        memoryBytes += elems * elemBytes;
      });

      bool isEmbarrassinglyParallel =
          potentiallyParallel && !hasDeps && !hasReductions;
      bool meetsIterThreshold =
          (tripCount >= 0 && tripCount >= machine.getGpuMinIterations());
      bool meetsDataThreshold =
          (memoryBytes == 0 ||
           memoryBytes >= machine.getGpuMinDataBytes());

      // Relaxed criteria: if embarrassingly parallel, mark for GPU
      // regardless of thresholds. Thresholds are only used for profitability.
      if (!isEmbarrassinglyParallel)
        return;

      // Always set gpu_target for embarrassingly parallel loops
      forOp->setAttr(AttrNames::Operation::GpuTarget,
                     GpuTargetAttr::get(context, GpuTarget::Auto));

      // Only attach profitability metadata if thresholds are met
      // (provides optimization hints for GPU lowering decisions)
      if (meetsIterThreshold && meetsDataThreshold) {
        auto i64Ty = IntegerType::get(context, 64);
        auto iterationsAttr = IntegerAttr::get(i64Ty, tripCount);
        auto memoryBytesAttr = IntegerAttr::get(i64Ty, memoryBytes);
        auto parallelAttr = BoolAttr::get(context, isEmbarrassinglyParallel);
        auto reductionsAttr = BoolAttr::get(context, hasReductions);

        auto profitability = GpuProfitabilityAttr::get(
            context, iterationsAttr, memoryBytesAttr, parallelAttr,
            reductionsAttr);
        forOp->setAttr(AttrNames::Operation::GpuProfitability, profitability);
      }
    });
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createGpuEligibilityAnalysisPass() {
  return std::make_unique<GpuEligibilityAnalysisPass>();
}

std::unique_ptr<Pass>
mlir::arts::createGpuEligibilityAnalysisPass(llvm::StringRef configPath) {
  return std::make_unique<GpuEligibilityAnalysisPass>(configPath);
}
