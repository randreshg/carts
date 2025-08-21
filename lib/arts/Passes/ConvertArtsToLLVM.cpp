///==========================================================================
// File: ConvertArtsToLLVM.cpp
//
// This file implements a pass to convert ARTS dialect operations into
// LLVM dialect operations. Since preprocessing is handled by a separate
// pass, this pass focuses purely on ARTS-specific conversion logic.
///==========================================================================

/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
/// Others
#include "mlir/Analysis/DataLayoutAnalysis.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include <memory>

/// Debug
#include "llvm/IR/DataLayout.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(convert_arts_to_llvm);

using namespace mlir;
using namespace arts;

//===----------------------------------------------------------------------===//
// Constants and Configuration
//===----------------------------------------------------------------------===//
namespace {
/// Configuration constants
} // namespace

//===----------------------------------------------------------------------===//
// Conversion Patterns
//===----------------------------------------------------------------------===//

/// Base class for ARTS to LLVM conversion patterns with shared ArtsCodegen
template <typename OpType>
class ArtsToLLVMPattern : public OpRewritePattern<OpType> {
protected:
  ArtsCodegen *AC;

public:
  ArtsToLLVMPattern(MLIRContext *context, ArtsCodegen *ac)
      : OpRewritePattern<OpType>(context), AC(ac) {}
};

/// Pattern to convert arts.get_total_workers operations
struct GetTotalWorkersPattern : public ArtsToLLVMPattern<GetTotalWorkersOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetTotalWorkersOp op,
                                PatternRewriter &rewriter) const override {
    auto result = AC->getTotalWorkers(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.get_total_nodes operations
struct GetTotalNodesPattern : public ArtsToLLVMPattern<GetTotalNodesOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetTotalNodesOp op,
                                PatternRewriter &rewriter) const override {
    auto result = AC->getTotalNodes(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.get_current_worker operations
struct GetCurrentWorkerPattern : public ArtsToLLVMPattern<GetCurrentWorkerOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetCurrentWorkerOp op,
                                PatternRewriter &rewriter) const override {
    auto result = AC->getCurrentWorker(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.get_current_node operations
struct GetCurrentNodePattern : public ArtsToLLVMPattern<GetCurrentNodeOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetCurrentNodeOp op,
                                PatternRewriter &rewriter) const override {
    auto result = AC->getCurrentNode(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.db_acquire operations
struct DbAcquirePattern : public ArtsToLLVMPattern<DbAcquireOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbAcquireOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(),
                       "Lowering arts.db_acquire to source pointer");
    rewriter.replaceOp(op, op.getSource());
    return success();
  }
};

/// Pattern to convert arts.db_release operations
struct DbReleasePattern : public ArtsToLLVMPattern<DbReleaseOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbReleaseOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(),
                       "Lowering arts.db_release (no-op - runtime managed)");
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.db_control operations
struct DbControlPattern : public ArtsToLLVMPattern<DbControlOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbControlOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(), "Lowering arts.db_control to pointer");
    rewriter.replaceOp(op, op.getPtr());
    return success();
  }
};

/// Pattern to convert arts.db_num_elements operations
struct DbNumElementsPattern : public ArtsToLLVMPattern<DbNumElementsOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbNumElementsOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(),
                       "Lowering arts.db_num_elements to constant");
    auto result = AC->createIntConstant(1, AC->Int32, op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.edt_param_unpack operations
struct EdtParamUnpackPattern : public ArtsToLLVMPattern<EdtParamUnpackOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EdtParamUnpackOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(), "Lowering arts.edt_param_unpack");

    auto paramMemref = op.getMemref();
    auto results = op.getUnpacked();

    SmallVector<Value> newResults;
    for (unsigned i = 0; i < results.size(); ++i) {
      auto idx = AC->createIndexConstant(i, op.getLoc());
      auto loadedParam = rewriter.create<memref::LoadOp>(
          op.getLoc(), paramMemref, ValueRange{idx});
      auto castedParam =
          AC->castParameter(results[i].getType(), loadedParam, op.getLoc());
      newResults.push_back(castedParam);
    }

    rewriter.replaceOp(op, newResults);
    return success();
  }
};

/// Pattern to convert arts.edt_dep_unpack operations
struct EdtDepUnpackPattern : public ArtsToLLVMPattern<EdtDepUnpackOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EdtDepUnpackOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(), "Lowering arts.edt_dep_unpack");

    auto depMemref = op.getMemref();
    auto results = op.getUnpacked();

    SmallVector<Value> newResults;
    for (unsigned i = 0; i < results.size(); ++i) {
      auto idx = AC->createIndexConstant(i, op.getLoc());
      auto loadedDep = rewriter.create<memref::LoadOp>(op.getLoc(), depMemref,
                                                       ValueRange{idx});
      newResults.push_back(loadedDep);
    }

    rewriter.replaceOp(op, newResults);
    return success();
  }
};

/// Pattern to convert arts.event operations
struct EventPattern : public ArtsToLLVMPattern<EventOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EventOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(), "Lowering arts.event to current EDT GUID");
    auto result = AC->getCurrentEdtGuid(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.barrier operations
struct BarrierPattern : public ArtsToLLVMPattern<BarrierOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(BarrierOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(), "Lowering arts.barrier to artsYield call");
    AC->createRuntimeCall(types::ARTSRTL_artsYield, {}, op.getLoc());
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.edt_outline_fn operations (key EDT creation)
struct EdtOutlineFnPattern : public ArtsToLLVMPattern<EdtOutlineFnOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EdtOutlineFnOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(),
                       "Lowering arts.edt_outline_fn to artsEdtCreate");

    // Get outlined function name
    auto funcNameAttr = op->getAttrOfType<StringAttr>("outlined_func");
    if (!funcNameAttr)
      return op.emitError("Missing outlined_func attribute");

    auto outlined =
        AC->getModule().lookupSymbol<func::FuncOp>(funcNameAttr.getValue());
    if (!outlined)
      return op.emitError("Outlined function not found");

    // Build artsEdtCreate arguments
    auto funcPtr = AC->createFnPtr(outlined, op.getLoc());
    auto route = AC->getCurrentNode(op.getLoc());
    auto paramv = op.getParamMemref();
    auto depc = op.getDepCount();
    auto paramc = AC->createIntConstant(
        0, AC->Int32, op.getLoc()); // Dynamic size handling could be added

    // Create artsEdtCreate call
    auto call = AC->createRuntimeCall(types::ARTSRTL_artsEdtCreate,
                                      {funcPtr, route, paramc, paramv, depc},
                                      op.getLoc());
    rewriter.replaceOp(op, call.getResult(0));
    return success();
  }
};

/// Pattern to convert arts.record_in_dep operations
struct RecordInDepPattern : public ArtsToLLVMPattern<RecordInDepOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(RecordInDepOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(
        op.getLoc(),
        "Lowering arts.record_in_dep to artsDbAddDependence calls");

    auto edtGuid = op.getEdtGuid();
    unsigned slot = 0; // Could be tracked more precisely

    // Add dependency for each datablock
    for (Value datablock : op.getDatablocks()) {
      auto slotVal = AC->createIntConstant(slot++, AC->Int32, op.getLoc());
      AC->createRuntimeCall(types::ARTSRTL_artsDbAddDependence,
                            {datablock, edtGuid, slotVal}, op.getLoc());
    }

    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.increment_out_latch operations
struct IncrementOutLatchPattern
    : public ArtsToLLVMPattern<IncrementOutLatchOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(IncrementOutLatchOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(
        op.getLoc(),
        "Lowering arts.increment_out_latch to artsDbIncrementLatch calls");

    // Increment latch for each datablock
    for (Value datablock : op.getDatablocks()) {
      AC->createRuntimeCall(types::ARTSRTL_artsDbIncrementLatch, {datablock},
                            op.getLoc());
    }

    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.alloc operations (ARTS memory allocation)
struct AllocPattern : public ArtsToLLVMPattern<AllocOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(AllocOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(), "Lowering arts.alloc to artsMalloc call");

    auto resultType = op.getResult().getType().cast<MemRefType>();

    // Calculate total allocation size using ArtsCodegen helpers
    auto elementSize = AC->computeMemrefElementSize(resultType, op.getLoc());
    Value totalSize = elementSize;

    // Handle dynamic sizes
    auto dynamicSizes = op.getDynamicSizes();
    if (!dynamicSizes.empty()) {
      auto totalElements =
          AC->computeMemrefTotalElements(dynamicSizes, op.getLoc());
      totalSize =
          rewriter.create<arith::MulIOp>(op.getLoc(), totalSize, totalElements);
    } else {
      // Handle static shape
      auto shape = resultType.getShape();
      int64_t totalElements = 1;
      for (auto dim : shape) {
        if (dim != ShapedType::kDynamic) {
          totalElements *= dim;
        }
      }
      auto totalElementsVal =
          AC->createIntConstant(totalElements, AC->Int64, op.getLoc());
      totalSize = rewriter.create<arith::MulIOp>(op.getLoc(), totalSize,
                                                 totalElementsVal);
    }

    // Call artsMalloc and cast result
    auto artsAllocCall = AC->createRuntimeCall(types::ARTSRTL_artsMalloc,
                                               {totalSize}, op.getLoc());
    auto castPtr =
        AC->castPtr(resultType, artsAllocCall.getResult(0), op.getLoc());

    rewriter.replaceOp(op, castPtr);
    return success();
  }
};

/// Pattern to convert arts.db_alloc operations (direct DB creation)
struct DbAllocPattern : public ArtsToLLVMPattern<DbAllocOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbAllocOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(),
                       "Lowering arts.db_alloc to artsDbCreateWithGuid");

    // Direct implementation without DbCodegen - much simpler!
    auto currentNode = AC->getCurrentNode(op.getLoc());

    // ARTS_DB_PIN mode (from arts.h: #define ARTS_DB_PIN 10)
    auto dbMode = AC->createIntConstant(10, AC->Int32, op.getLoc());

    // Reserve GUID for the DB
    auto guidCall = AC->createRuntimeCall(types::ARTSRTL_artsReserveGuidRoute,
                                          {dbMode, currentNode}, op.getLoc());
    auto guid = guidCall.getResult(0);

    // Calculate element size (simplified - could be enhanced for
    // multi-dimensional)
    auto resultType = op.getResult().getType().cast<MemRefType>();
    auto elementSize = AC->computeMemrefElementSize(resultType, op.getLoc());

    // Create the DB with GUID
    auto dbCall = AC->createRuntimeCall(types::ARTSRTL_artsDbCreateWithGuid,
                                        {guid, elementSize}, op.getLoc());

    rewriter.replaceOp(op, dbCall.getResult(0));
    return success();
  }
};

/// Pattern to convert arts.epoch operations (complex epoch management)
struct EpochPattern : public ArtsToLLVMPattern<EpochOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EpochOp op,
                                PatternRewriter &rewriter) const override {
    AC->printDebugInfo(op.getLoc(),
                       "Lowering arts.epoch to epoch EDT creation");

    // Create a done-edt for the epoch (simplified from original EdtCodegen
    // approach)
    constexpr int DEFAULT_EDT_SLOT = 0;

    // For now, use a simplified approach - could be enhanced with full
    // EdtCodegen logic
    auto guid = AC->getCurrentEdtGuid(op.getLoc());
    auto edtSlot =
        AC->createIntConstant(DEFAULT_EDT_SLOT, AC->Int32, op.getLoc());
    auto currentEpoch = AC->createEpoch(guid, edtSlot, op.getLoc());

    // Move operations out of epoch region to before the epoch
    auto &epochRegion = op.getRegion();
    if (!epochRegion.empty()) {
      auto &epochBlock = epochRegion.front();
      SmallVector<Operation *> opsToMove;
      for (auto &innerOp : epochBlock.without_terminator())
        opsToMove.push_back(&innerOp);

      for (auto *opToMove : opsToMove) {
        opToMove->moveBefore(op);
      }
    }

    // Wait on the epoch
    rewriter.setInsertionPointAfter(op);
    AC->waitOnHandle(currentEpoch, op.getLoc());
    rewriter.eraseOp(op);

    return success();
  }
};

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertArtsToLLVMPass
    : public arts::ConvertArtsToLLVMBase<ConvertArtsToLLVMPass> {

  explicit ConvertArtsToLLVMPass(bool debug = false) : debugMode(debug) {}

  void runOnOperation() override;

private:
  LogicalResult initializeCodegen();
  void populatePatterns(RewritePatternSet &patterns);

  /// Member variables
  ArtsCodegen *AC = nullptr;
  bool debugMode = false;
};
} // namespace

//===----------------------------------------------------------------------===//
// Core Pass Implementation
//===----------------------------------------------------------------------===//

void ConvertArtsToLLVMPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *context = &getContext();

  ARTS_DEBUG_HEADER(ConvertArtsToLLVMPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Initialize codegen infrastructure
  if (failed(initializeCodegen())) {
    ARTS_ERROR("Failed to initialize ArtsCodegen");
    return signalPassFailure();
  }

  /// Create and populate rewrite patterns
  RewritePatternSet patterns(context);
  populatePatterns(patterns);

  /// Apply patterns with greedy rewriter
  GreedyRewriteConfig config;
  if (failed(
          applyPatternsAndFoldGreedily(module, std::move(patterns), config))) {
    ARTS_ERROR("Failed to apply ARTS to LLVM conversion patterns");
    delete AC;
    return signalPassFailure();
  }

  /// Initialize runtime (main function transformation)
  AC->initRT(AC->getUnknownLoc());

  /// Cleanup
  delete AC;
  AC = nullptr;

  ARTS_DEBUG_FOOTER(ConvertArtsToLLVMPass);
  ARTS_DEBUG_REGION(module.dump(););
}

LogicalResult ConvertArtsToLLVMPass::initializeCodegen() {
  ModuleOp module = getOperation();

  /// Validate data layout
  auto llvmDLAttr = module->getAttrOfType<StringAttr>("llvm.data_layout");
  if (!llvmDLAttr)
    return module.emitError("Module missing required LLVM data layout");

  llvm::DataLayout llvmDL(llvmDLAttr.getValue().str());
  mlir::DataLayout mlirDL(module);
  AC = new ArtsCodegen(module, llvmDL, mlirDL, debugMode);

  ARTS_DEBUG_TYPE("ArtsCodegen initialized successfully");
  return success();
}

void ConvertArtsToLLVMPass::populatePatterns(RewritePatternSet &patterns) {
  MLIRContext *context = patterns.getContext();

  /// Add all conversion patterns
  patterns.add<GetTotalWorkersPattern, GetTotalNodesPattern,
               GetCurrentWorkerPattern, GetCurrentNodePattern, DbAcquirePattern,
               DbReleasePattern, DbControlPattern, DbNumElementsPattern,
               EdtParamUnpackPattern, EdtDepUnpackPattern, EventPattern,
               BarrierPattern, EdtOutlineFnPattern, RecordInDepPattern,
               IncrementOutLatchPattern, AllocPattern, DbAllocPattern,
               EpochPattern>(context, AC);
}

//===----------------------------------------------------------------------===//
// Pass Functions
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertArtsToLLVMPass() {
  return std::make_unique<ConvertArtsToLLVMPass>();
}

std::unique_ptr<Pass> createConvertArtsToLLVMPass(bool debug) {
  return std::make_unique<ConvertArtsToLLVMPass>(debug);
}
} // namespace arts
} // namespace mlir
