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

#include "../ArtsPassDetails.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Dominance.h"

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_loop_vectorization_hints);

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

/// Check if a loop is innermost (contains no nested loops).
static bool isInnermostLoop(Block *headerBlock, Block *latchBlock,
                            DominanceInfo &domInfo, LLVM::LLVMFuncOp funcOp) {
  SmallPtrSet<Block *, 16> loopBlocks;
  for (Block &block : funcOp.getBody()) {
    if (domInfo.dominates(headerBlock, &block))
      loopBlocks.insert(&block);
  }

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

/// Add llvm.assume intrinsics for non-negative paramv loads.
static int addNonNegativeAssumptions(LLVM::LLVMFuncOp funcOp,
                                     MLIRContext *ctx) {
  if (funcOp.getNumArguments() < 2)
    return 0;

  Value paramv = funcOp.getArgument(1);
  int assumptionsAdded = 0;
  OpBuilder builder(ctx);

  SmallVector<LLVM::LoadOp> paramvLoads;
  funcOp.walk([&](LLVM::LoadOp loadOp) {
    Type resultType = loadOp.getResult().getType();
    if (!resultType.isInteger(64))
      return;

    Value addr = loadOp.getAddr();
    while (auto gepOp = addr.getDefiningOp<LLVM::GEPOp>())
      addr = gepOp.getBase();

    if (addr == paramv)
      paramvLoads.push_back(loadOp);
  });
  auto i64Ty = IntegerType::get(ctx, 64);
  auto i1Ty = IntegerType::get(ctx, 1);

  for (auto loadOp : paramvLoads) {
    builder.setInsertionPointAfter(loadOp);
    auto loc = loadOp.getLoc();
    auto zero = builder.create<LLVM::ConstantOp>(loc, i64Ty,
                                                 builder.getI64IntegerAttr(0));
    auto isNonNeg = builder.create<LLVM::ICmpOp>(
        loc, i1Ty, LLVM::ICmpPredicate::sge, loadOp.getResult(), zero);
    builder.create<LLVM::AssumeOp>(loc, isNonNeg);
    assumptionsAdded++;
  }

  return assumptionsAdded;
}

/// Collect all blocks belonging to a loop given its header and latch.
static SmallPtrSet<Block *, 16> getLoopBlocks(Block *headerBlock,
                                              Block *latchBlock,
                                              DominanceInfo &domInfo,
                                              LLVM::LLVMFuncOp funcOp) {
  SmallPtrSet<Block *, 16> loopBlocks;
  for (Block &block : funcOp.getBody()) {
    if (domInfo.dominates(headerBlock, &block))
      loopBlocks.insert(&block);
  }
  return loopBlocks;
}

/// Add fast-math flags to floating-point operations to enable vectorization.
/// The 'reassoc' flag is critical for reduction vectorization as it allows
/// LLVM to reorder floating-point operations in loop-carried dependencies.
static int addFastMathFlagsToFPOps(const SmallPtrSet<Block *, 16> &loopBlocks,
                                   MLIRContext *ctx) {
  int count = 0;
  auto fmfAttr = LLVM::FastmathFlagsAttr::get(
      ctx, LLVM::FastmathFlags::reassoc | LLVM::FastmathFlags::contract);

  for (Block *block : loopBlocks) {
    for (Operation &op : *block) {
      /// Handle FAddOp
      if (auto faddOp = dyn_cast<LLVM::FAddOp>(&op)) {
        auto existing = faddOp.getFastmathFlags();
        if (existing == LLVM::FastmathFlags::none) {
          faddOp.setFastmathFlagsAttr(fmfAttr);
          count++;
        }
        continue;
      }
      /// Handle FMulOp
      if (auto fmulOp = dyn_cast<LLVM::FMulOp>(&op)) {
        auto existing = fmulOp.getFastmathFlags();
        if (existing == LLVM::FastmathFlags::none) {
          fmulOp.setFastmathFlagsAttr(fmfAttr);
          count++;
        }
        continue;
      }
      /// Handle FSubOp
      if (auto fsubOp = dyn_cast<LLVM::FSubOp>(&op)) {
        auto existing = fsubOp.getFastmathFlags();
        if (existing == LLVM::FastmathFlags::none) {
          fsubOp.setFastmathFlagsAttr(fmfAttr);
          count++;
        }
        continue;
      }
      /// Handle FDivOp
      if (auto fdivOp = dyn_cast<LLVM::FDivOp>(&op)) {
        auto existing = fdivOp.getFastmathFlags();
        if (existing == LLVM::FastmathFlags::none) {
          fdivOp.setFastmathFlagsAttr(fmfAttr);
          count++;
        }
        continue;
      }
    }
  }
  return count;
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
    : public LoopVectorizationHintsBase<LoopVectorizationHintsPass> {
  unsigned vectorWidth = 0;
  unsigned interleaveCount = 4;
  bool enableScalable = false;
  bool enableMustProgress = true;

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    ARTS_INFO_HEADER(LoopVectorizationHintsPass);

    int totalHints = 0;
    int totalAccessGroups = 0;
    int totalAssumptions = 0;
    int totalFastMathFlags = 0;

    module.walk([&](LLVM::LLVMFuncOp funcOp) {
      if (!funcOp.getName().starts_with("__arts_edt_"))
        return;

      ARTS_DEBUG_TYPE("Processing EDT function: " << funcOp.getName());

      int assumptionsInFunc = addNonNegativeAssumptions(funcOp, ctx);
      if (assumptionsInFunc > 0) {
        ARTS_DEBUG_TYPE("Added "
                        << assumptionsInFunc
                        << " non-negative assumptions for paramv loads");
        totalAssumptions += assumptionsInFunc;
      }

      DominanceInfo domInfo(funcOp);

      auto typeInfo = analyzeLoadTypes(funcOp);

      unsigned width = vectorWidth;
      if (width == 0) {
        width = typeInfo.vectorWidth;
        ARTS_DEBUG_TYPE("Auto-detected vector width: " << width);
      }

      unsigned unrollCount = determineUnrollCount(typeInfo);
      ARTS_DEBUG_TYPE("Unroll count: " << unrollCount << " (loads="
                                       << typeInfo.totalLoads << ")");

      auto accessGroupId = DistinctAttr::create(UnitAttr::get(ctx));
      auto accessGroupAttr = LLVM::AccessGroupAttr::get(ctx, accessGroupId);

      int accessGroupsAttached = 0;
      funcOp.walk([&](Operation *op) {
        ArrayAttr existingGroups;
        if (auto loadOp = dyn_cast<LLVM::LoadOp>(op)) {
          existingGroups = loadOp.getAccessGroupsAttr();
        } else if (auto storeOp = dyn_cast<LLVM::StoreOp>(op)) {
          existingGroups = storeOp.getAccessGroupsAttr();
        } else {
          return;
        }

        SmallVector<Attribute> groups;
        if (existingGroups)
          groups.append(existingGroups.begin(), existingGroups.end());
        groups.push_back(accessGroupAttr);
        auto newGroupsAttr = ArrayAttr::get(ctx, groups);

        if (auto loadOp = dyn_cast<LLVM::LoadOp>(op)) {
          loadOp.setAccessGroupsAttr(newGroupsAttr);
          accessGroupsAttached++;
        } else if (auto storeOp = dyn_cast<LLVM::StoreOp>(op)) {
          storeOp.setAccessGroupsAttr(newGroupsAttr);
          accessGroupsAttached++;
        }
      });

      if (accessGroupsAttached > 0) {
        ARTS_DEBUG_TYPE("Attached access group to " << accessGroupsAttached
                                                    << " memory operations");
        totalAccessGroups += accessGroupsAttached;
      }

      SmallVector<LLVM::AccessGroupAttr> parallelAccessGroups;
      parallelAccessGroups.push_back(accessGroupAttr);

      auto innermostHints = createInnermostLoopHints(
          ctx, width, interleaveCount, enableScalable, enableMustProgress,
          unrollCount, parallelAccessGroups);
      auto outerHints = createOuterLoopHints(ctx, enableMustProgress,
                                             unrollCount, parallelAccessGroups);

      int innermostCount = 0;
      int outerCount = 0;
      int fastMathFlagsInFunc = 0;

      funcOp.walk([&](LLVM::BrOp brOp) {
        Block *currentBlock = brOp->getBlock();
        Block *destBlock = brOp.getDest();

        if (!domInfo.dominates(destBlock, currentBlock))
          return;

        if (brOp.getLoopAnnotationAttr())
          return;

        bool innermost =
            isInnermostLoop(destBlock, currentBlock, domInfo, funcOp);

        if (innermost) {
          brOp.setLoopAnnotationAttr(innermostHints);
          innermostCount++;
          ARTS_DEBUG_TYPE("Innermost loop - full vectorization hints at "
                          << brOp.getLoc());
          /// Add fast-math flags to FP operations in innermost loops
          auto loopBlocks =
              getLoopBlocks(destBlock, currentBlock, domInfo, funcOp);
          fastMathFlagsInFunc += addFastMathFlagsToFPOps(loopBlocks, ctx);
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

        bool innermost =
            isInnermostLoop(headerBlock, currentBlock, domInfo, funcOp);

        if (innermost) {
          condBr.setLoopAnnotationAttr(innermostHints);
          innermostCount++;
          ARTS_DEBUG_TYPE("Innermost loop (cond) - full vectorization hints at "
                          << condBr.getLoc());
          /// Add fast-math flags to FP operations in innermost loops
          auto loopBlocks =
              getLoopBlocks(headerBlock, currentBlock, domInfo, funcOp);
          fastMathFlagsInFunc += addFastMathFlagsToFPOps(loopBlocks, ctx);
        } else {
          condBr.setLoopAnnotationAttr(outerHints);
          outerCount++;
          ARTS_DEBUG_TYPE("Outer loop (cond) - light hints at "
                          << condBr.getLoc());
        }
      });

      totalFastMathFlags += fastMathFlagsInFunc;

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
              << totalHints << " loop backedges, access groups to "
              << totalAccessGroups << " memory operations, " << totalAssumptions
              << " non-negative assumptions, fast-math flags to "
              << totalFastMathFlags << " FP operations");
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
