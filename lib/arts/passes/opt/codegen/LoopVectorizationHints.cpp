///==========================================================================///
/// File: LoopVectorizationHints.cpp
///
/// This pass attaches LLVM loop optimization hints to loop backedges in EDT
/// functions. It uses MLIR's native LoopAnnotationAttr which automatically
/// translates to !llvm.loop metadata during LLVM IR emission.
///
/// Vectorization hints are applied to innermost loops only. Outer loops
/// receive unroll hints but no vectorization hints to avoid warnings.
///
/// Before (no hints):
///   llvm.br ^loop_header
///
/// After (innermost loop):
///   llvm.br ^loop_header {
///     loop_annotation = #llvm.loop_annotation<
///       vectorize = #llvm.loop_vectorize<disable = false, width = 4>,
///       interleave = #llvm.loop_interleave<count = 4>,
///       unroll = #llvm.loop_unroll<disable = false, count = 8>,
///       mustProgress = true
///     >
///   }
///
/// After (outer loop):
///   llvm.br ^loop_header {
///     loop_annotation = #llvm.loop_annotation<
///       unroll = #llvm.loop_unroll<disable = false, count = 4>,
///       mustProgress = true
///     >
///   }
///==========================================================================///

#define GEN_PASS_DEF_LOOPVECTORIZATIONHINTS
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Dominance.h"

#include "arts/Dialect.h"
#include "arts/passes/Passes.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(loop_vectorization_hints);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Check if a conditional branch forms a loop backedge by checking if
/// either target block dominates the block containing the branch.
static bool isLoopBackedge(LLVM::CondBrOp condBr, DominanceInfo &domInfo) {
  Block *currentBlock = condBr->getBlock();
  Block *trueBlock = condBr.getTrueDest();
  Block *falseBlock = condBr.getFalseDest();

  return domInfo.dominates(trueBlock, currentBlock) ||
         domInfo.dominates(falseBlock, currentBlock);
}

/// Collect natural-loop blocks for a backedge latch -> header.
static SmallPtrSet<Block *, 16> getLoopBlocks(Block *headerBlock,
                                              Block *latchBlock) {
  SmallPtrSet<Block *, 16> loopBlocks;
  SmallVector<Block *, 16> worklist;
  loopBlocks.insert(headerBlock);
  if (loopBlocks.insert(latchBlock).second)
    worklist.push_back(latchBlock);

  while (!worklist.empty()) {
    Block *block = worklist.pop_back_val();
    for (Block *pred : block->getPredecessors()) {
      if (!loopBlocks.insert(pred).second)
        continue;
      if (pred != headerBlock)
        worklist.push_back(pred);
    }
  }

  return loopBlocks;
}

/// Check if a loop is innermost (contains no nested loops).
static bool isInnermostLoop(Block *headerBlock, Block *latchBlock,
                            DominanceInfo &domInfo) {
  SmallPtrSet<Block *, 16> loopBlocks = getLoopBlocks(headerBlock, latchBlock);

  for (Block *block : loopBlocks) {
    if (block == latchBlock)
      continue;

    if (auto brOp = dyn_cast<LLVM::BrOp>(block->getTerminator())) {
      Block *dest = brOp.getDest();
      if (dest != headerBlock && loopBlocks.contains(dest) &&
          domInfo.dominates(dest, block))
        return false;
    }

    if (auto condBr = dyn_cast<LLVM::CondBrOp>(block->getTerminator())) {
      for (Block *dest : {condBr.getTrueDest(), condBr.getFalseDest()}) {
        if (dest != headerBlock && loopBlocks.contains(dest) &&
            domInfo.dominates(dest, block))
          return false;
      }
    }
  }

  return true;
}

/// Analyze function to determine optimal vector width based on data types.
/// Returns: {vectorWidth, f64Count, f32Count, totalLoads}
struct TypeAnalysisResult {
  unsigned vectorWidth;
  unsigned f64Count;
  unsigned f32Count;
  unsigned totalLoads;
};

static TypeAnalysisResult analyzeLoadTypes(LLVM::LLVMFuncOp funcOp) {
  unsigned floatCount = 0, doubleCount = 0, i32Count = 0, totalLoads = 0;

  funcOp.walk([&](LLVM::LoadOp loadOp) {
    totalLoads++;
    Type elemType = loadOp.getResult().getType();
    if (elemType.isF32())
      floatCount++;
    else if (elemType.isF64())
      doubleCount++;
    else if (elemType.isInteger(32))
      i32Count++;
  });

  unsigned width = 8;
  if (doubleCount >= floatCount && doubleCount >= i32Count)
    width = 4;

  return {width, doubleCount, floatCount, totalLoads};
}

/// Returns true if `value` is dataflow-dependent on `root`.
/// `followLoads` controls whether load operands are traversed.
static bool valueDependsOn(Value value, Value root, bool followLoads) {
  SmallVector<Value, 16> worklist;
  SmallPtrSet<Value, 32> visited;
  worklist.push_back(value);

  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!visited.insert(current).second)
      continue;
    if (current == root)
      return true;

    Operation *def = current.getDefiningOp();
    if (!def)
      continue;

    if (auto gep = dyn_cast<LLVM::GEPOp>(def)) {
      worklist.push_back(gep.getBase());
      continue;
    }
    if (auto load = dyn_cast<LLVM::LoadOp>(def)) {
      if (followLoads)
        worklist.push_back(load.getAddr());
      continue;
    }
    if (auto cast = dyn_cast<UnrealizedConversionCastOp>(def)) {
      for (Value in : cast.getInputs())
        worklist.push_back(in);
      continue;
    }
  }

  return false;
}

/// Returns true if `value` depends on a loop-varying quantity in this natural
/// loop. Values defined outside the loop are treated as invariant.
static bool
valueDependsOnLoopVariant(Value value,
                          const SmallPtrSet<Block *, 16> &loopBlocks) {
  SmallVector<Value, 16> worklist;
  SmallPtrSet<Value, 32> visited;
  worklist.push_back(value);

  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!visited.insert(current).second)
      continue;

    if (auto blockArg = dyn_cast<BlockArgument>(current)) {
      if (loopBlocks.contains(blockArg.getOwner()))
        return true;
      continue;
    }

    Operation *def = current.getDefiningOp();
    if (!def)
      continue;
    if (!loopBlocks.contains(def->getBlock()))
      continue;

    if (isa<LLVM::ConstantOp>(def))
      continue;

    for (Value operand : def->getOperands())
      worklist.push_back(operand);
  }

  return false;
}

/// Returns true if the loop performs loop-varying dep-array traversal. Purely
/// invariant dep-array pointer materialization is allowed.
static bool loopHasVariantDepArrayAccess(Block *headerBlock, Block *latchBlock,
                                         Value depv) {
  if (!depv)
    return false;

  SmallPtrSet<Block *, 16> loopBlocks = getLoopBlocks(headerBlock, latchBlock);
  for (Block *block : loopBlocks) {
    for (Operation &opRef : *block) {
      Operation *op = &opRef;
      if (auto load = dyn_cast<LLVM::LoadOp>(op)) {
        if (valueDependsOn(load.getAddr(), depv, /*followLoads=*/false) &&
            valueDependsOnLoopVariant(load.getAddr(), loopBlocks))
          return true;
        continue;
      }
      if (auto store = dyn_cast<LLVM::StoreOp>(op)) {
        if (valueDependsOn(store.getAddr(), depv, /*followLoads=*/false) &&
            valueDependsOnLoopVariant(store.getAddr(), loopBlocks))
          return true;
        continue;
      }
    }
  }
  return false;
}

/// Determine optimal unroll count based on loop characteristics.
static unsigned determineUnrollCount(const TypeAnalysisResult &typeInfo) {
  unsigned cacheLineElements =
      (typeInfo.f64Count >= typeInfo.f32Count) ? 8 : 16;
  unsigned vectorWidth = typeInfo.vectorWidth;
  unsigned unrollCount = cacheLineElements / vectorWidth;
  if (unrollCount == 0)
    unrollCount = 1;
  unrollCount *= vectorWidth;

  if (typeInfo.totalLoads > 16)
    unrollCount = std::min(unrollCount, 2u);
  else if (typeInfo.totalLoads > 8)
    unrollCount = std::min(unrollCount, 4u);

  return std::clamp(unrollCount, 2u, 8u);
}

/// Create full vectorization hints for innermost loops.
static LLVM::LoopAnnotationAttr
createInnermostLoopHints(MLIRContext *ctx, unsigned width, unsigned interleave,
                         bool scalable, bool mustProgress, unsigned unrollCount,
                         ArrayRef<LLVM::AccessGroupAttr> accessGroups) {
  auto vecAttr = LLVM::LoopVectorizeAttr::get(
      ctx,
      /*disable=*/BoolAttr::get(ctx, false),
      /*predicateEnable=*/nullptr,
      /*scalableEnable=*/scalable ? BoolAttr::get(ctx, true) : nullptr,
      /*width=*/IntegerAttr::get(IntegerType::get(ctx, 32), width),
      /*followupVectorized=*/nullptr,
      /*followupEpilogue=*/nullptr,
      /*followupAll=*/nullptr);

  auto interleaveAttr = LLVM::LoopInterleaveAttr::get(
      ctx, IntegerAttr::get(IntegerType::get(ctx, 32), interleave));

  auto unrollAttr = LLVM::LoopUnrollAttr::get(
      ctx,
      /*disable=*/BoolAttr::get(ctx, false),
      /*count=*/IntegerAttr::get(IntegerType::get(ctx, 32), unrollCount),
      /*runtimeDisable=*/nullptr,
      /*full=*/nullptr,
      /*followupUnrolled=*/nullptr,
      /*followupRemainder=*/nullptr,
      /*followupAll=*/nullptr);

  return LLVM::LoopAnnotationAttr::get(
      ctx,
      /*disableNonforced=*/nullptr,
      /*vectorize=*/vecAttr,
      /*interleave=*/interleaveAttr,
      /*unroll=*/unrollAttr,
      /*unrollAndJam=*/nullptr,
      /*licm=*/nullptr,
      /*distribute=*/nullptr,
      /*pipeline=*/nullptr,
      /*peeled=*/nullptr,
      /*unswitch=*/nullptr,
      /*mustProgress=*/mustProgress ? BoolAttr::get(ctx, true) : nullptr,
      /*isVectorized=*/nullptr,
      /*parallelAccesses=*/accessGroups,
      /*startLoc=*/FusedLoc(),
      /*endLoc=*/FusedLoc());
}

/// Create light hints for outer loops (no vectorization).
static LLVM::LoopAnnotationAttr
createOuterLoopHints(MLIRContext *ctx, bool mustProgress, unsigned unrollCount,
                     ArrayRef<LLVM::AccessGroupAttr> accessGroups) {
  unsigned outerUnroll = std::min(unrollCount, 4u);

  auto unrollAttr = LLVM::LoopUnrollAttr::get(
      ctx,
      /*disable=*/BoolAttr::get(ctx, false),
      /*count=*/IntegerAttr::get(IntegerType::get(ctx, 32), outerUnroll),
      /*runtimeDisable=*/nullptr,
      /*full=*/nullptr,
      /*followupUnrolled=*/nullptr,
      /*followupRemainder=*/nullptr,
      /*followupAll=*/nullptr);

  return LLVM::LoopAnnotationAttr::get(
      ctx,
      /*disableNonforced=*/nullptr,
      /*vectorize=*/nullptr,
      /*interleave=*/nullptr,
      /*unroll=*/unrollAttr,
      /*unrollAndJam=*/nullptr,
      /*licm=*/nullptr,
      /*distribute=*/nullptr,
      /*pipeline=*/nullptr,
      /*peeled=*/nullptr,
      /*unswitch=*/nullptr,
      /*mustProgress=*/mustProgress ? BoolAttr::get(ctx, true) : nullptr,
      /*isVectorized=*/nullptr,
      /*parallelAccesses=*/accessGroups,
      /*startLoc=*/FusedLoc(),
      /*endLoc=*/FusedLoc());
}

struct LoopVectorizationHintsPass
    : public impl::LoopVectorizationHintsBase<LoopVectorizationHintsPass> {
  unsigned vectorWidth = 0;
  unsigned interleaveCount = 4;
  bool enableScalable = false;
  bool enableMustProgress = true;

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    ARTS_INFO_HEADER(LoopVectorizationHintsPass);

    int totalHints = 0;
    int skippedUnsafeLoops = 0;

    module.walk([&](LLVM::LLVMFuncOp funcOp) {
      if (!funcOp.getName().starts_with("__arts_edt_"))
        return;

      ARTS_DEBUG_TYPE("Processing EDT function: " << funcOp.getName());

      DominanceInfo domInfo(funcOp);
      Value depv =
          (funcOp.getNumArguments() >= 4) ? funcOp.getArgument(3) : Value{};

      auto typeInfo = analyzeLoadTypes(funcOp);

      unsigned width = vectorWidth;
      if (width == 0) {
        width = typeInfo.vectorWidth;
        ARTS_DEBUG_TYPE("Auto-detected vector width: " << width);
      }

      unsigned unrollCount = determineUnrollCount(typeInfo);
      ARTS_DEBUG_TYPE("Unroll count: " << unrollCount << " (loads="
                                       << typeInfo.totalLoads << ")");
      SmallVector<LLVM::AccessGroupAttr> parallelAccessGroups;

      auto innermostHints = createInnermostLoopHints(
          ctx, width, interleaveCount, enableScalable, enableMustProgress,
          unrollCount, parallelAccessGroups);
      auto outerHints = createOuterLoopHints(ctx, enableMustProgress,
                                             unrollCount, parallelAccessGroups);

      int innermostCount = 0;
      int outerCount = 0;

      funcOp.walk([&](LLVM::BrOp brOp) {
        Block *currentBlock = brOp->getBlock();
        Block *destBlock = brOp.getDest();

        if (!domInfo.dominates(destBlock, currentBlock))
          return;

        if (brOp.getLoopAnnotationAttr())
          return;

        if (loopHasVariantDepArrayAccess(destBlock, currentBlock, depv)) {
          skippedUnsafeLoops++;
          ARTS_DEBUG_TYPE("Skipping loop hints in "
                          << funcOp.getName()
                          << " (loop-varying dep-array access)");
          return;
        }

        bool innermost = isInnermostLoop(destBlock, currentBlock, domInfo);

        if (innermost) {
          brOp.setLoopAnnotationAttr(innermostHints);
          innermostCount++;
          ARTS_DEBUG_TYPE("Innermost loop - full vectorization hints at "
                          << brOp.getLoc());
        } else {
          brOp.setLoopAnnotationAttr(outerHints);
          outerCount++;
          ARTS_DEBUG_TYPE("Outer loop - light hints (no vectorization) at "
                          << brOp.getLoc());
        }
      });

      funcOp.walk([&](LLVM::CondBrOp condBr) {
        if (!isLoopBackedge(condBr, domInfo))
          return;

        if (condBr.getLoopAnnotationAttr())
          return;

        Block *currentBlock = condBr->getBlock();
        Block *trueBlock = condBr.getTrueDest();
        Block *falseBlock = condBr.getFalseDest();
        Block *headerBlock =
            domInfo.dominates(trueBlock, currentBlock) ? trueBlock : falseBlock;

        if (loopHasVariantDepArrayAccess(headerBlock, currentBlock, depv)) {
          skippedUnsafeLoops++;
          ARTS_DEBUG_TYPE("Skipping loop hints in "
                          << funcOp.getName()
                          << " (loop-varying dep-array access)");
          return;
        }

        bool innermost = isInnermostLoop(headerBlock, currentBlock, domInfo);

        if (innermost) {
          condBr.setLoopAnnotationAttr(innermostHints);
          innermostCount++;
          ARTS_DEBUG_TYPE("Innermost loop (cond) - full vectorization hints at "
                          << condBr.getLoc());
        } else {
          condBr.setLoopAnnotationAttr(outerHints);
          outerCount++;
          ARTS_DEBUG_TYPE("Outer loop (cond) - light hints at "
                          << condBr.getLoc());
        }
      });

      int hintsInFunc = innermostCount + outerCount;
      if (hintsInFunc > 0) {
        ARTS_INFO("Attached hints to "
                  << hintsInFunc << " loops in " << funcOp.getName() << " ("
                  << innermostCount << " innermost with vectorization, "
                  << outerCount << " outer without)");
        totalHints += hintsInFunc;
      }
    });

    ARTS_INFO("Total: attached hints to "
              << totalHints << " loop backedges; skipped " << skippedUnsafeLoops
              << " loop backedges with loop-varying dependency-array access");
    ARTS_INFO_FOOTER(LoopVectorizationHintsPass);
  }
};

} // namespace

///===----------------------------------------------------------------------===///
/// Pass creation and registration
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {

std::unique_ptr<Pass> createLoopVectorizationHintsPass() {
  return std::make_unique<LoopVectorizationHintsPass>();
}

} // namespace arts
} // namespace mlir
