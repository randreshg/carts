///==========================================================================///
/// File: LoopVectorizationHints.cpp
///
/// This pass attaches LLVM loop vectorization hints to loop backedges in EDT
/// functions. It uses MLIR's native LoopAnnotationAttr which automatically
/// translates to !llvm.loop metadata during LLVM IR emission.
///
/// Before (no vectorization hints - LLVM may not vectorize):
///   llvm.br ^loop_header  // Loop backedge
///
/// After (vectorization hints encourage LLVM's loop vectorizer):
///   llvm.br ^loop_header {
///     loop_annotation = #llvm.loop_annotation<
///       vectorize = #llvm.loop_vectorize<disable = false, width = 2>,
///       interleave = #llvm.loop_interleave<count = 4>
///     >
///   }
///
/// The pass identifies conditional branches that form loop backedges (branches
/// to blocks that dominate the branch) and attaches vectorization hints
/// (width=2 for doubles, interleave=4) to encourage LLVM's loop vectorizer.
///==========================================================================///

#include "ArtsPassDetails.h"
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

  // A backedge is when we branch to a block that dominates us
  // (i.e., we're looping back to a header block)
  return domInfo.dominates(trueBlock, currentBlock) ||
         domInfo.dominates(falseBlock, currentBlock);
}

/// Create vectorization hint attributes for a loop
static LLVM::LoopAnnotationAttr createVectorizationHints(MLIRContext *ctx) {
  // Create vectorization hint: enable vectorization with width=2 for doubles
  // The 'disable' parameter is inverted: disable=false means ENABLE vectorization
  auto vecAttr = LLVM::LoopVectorizeAttr::get(
      ctx,
      /*disable=*/BoolAttr::get(ctx, false), // false = enable vectorization
      /*predicateEnable=*/nullptr,
      /*scalableEnable=*/nullptr,
      /*width=*/IntegerAttr::get(IntegerType::get(ctx, 32), 2), // width=2 for doubles
      /*followupVectorized=*/nullptr,
      /*followupEpilogue=*/nullptr,
      /*followupAll=*/nullptr);

  // Create interleave hint: interleave by 4 for better pipelining
  auto interleaveAttr = LLVM::LoopInterleaveAttr::get(
      ctx, IntegerAttr::get(IntegerType::get(ctx, 32), 4));

  // Create the full loop annotation with vectorization hints
  return LLVM::LoopAnnotationAttr::get(
      ctx,
      /*disableNonforced=*/nullptr,
      /*vectorize=*/vecAttr,
      /*interleave=*/interleaveAttr,
      /*unroll=*/nullptr,
      /*unrollAndJam=*/nullptr,
      /*licm=*/nullptr,
      /*distribute=*/nullptr,
      /*pipeline=*/nullptr,
      /*peeled=*/nullptr,
      /*unswitch=*/nullptr,
      /*mustProgress=*/nullptr,
      /*isVectorized=*/nullptr,
      /*parallelAccesses=*/{},
      /*startLoc=*/FusedLoc(),
      /*endLoc=*/FusedLoc());
}

struct LoopVectorizationHintsPass
    : public PassWrapper<LoopVectorizationHintsPass,
                         OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LoopVectorizationHintsPass)

  StringRef getArgument() const override {
    return "arts-loop-vectorization-hints";
  }
  StringRef getDescription() const override {
    return "Attach LLVM loop vectorization hints to EDT function loops";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<LLVM::LLVMDialect>();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    ARTS_INFO_HEADER(LoopVectorizationHintsPass);

    int totalHints = 0;

    // Process each EDT function
    module.walk([&](LLVM::LLVMFuncOp funcOp) {
      // Only process EDT functions
      if (!funcOp.getName().starts_with("__arts_edt_"))
        return;

      ARTS_DEBUG_TYPE("Processing EDT function: " << funcOp.getName());

      // Build dominance info for this function
      DominanceInfo domInfo(funcOp);

      // Create the vectorization hints (same for all loops in this EDT)
      auto loopAnnotation = createVectorizationHints(ctx);

      int hintsInFunc = 0;

      // Walk UNCONDITIONAL branches - these are the common case for loop
      // backedges after SCF->CF->LLVM lowering (latch -> header)
      funcOp.walk([&](LLVM::BrOp brOp) {
        Block *currentBlock = brOp->getBlock();
        Block *destBlock = brOp.getDest();

        // A backedge is when we branch to a block that dominates us
        if (domInfo.dominates(destBlock, currentBlock)) {
          if (!brOp.getLoopAnnotationAttr()) {
            brOp.setLoopAnnotationAttr(loopAnnotation);
            hintsInFunc++;
            ARTS_DEBUG_TYPE("Attached vectorization hints to unconditional "
                            "backedge at "
                            << brOp.getLoc());
          }
        }
      });

      // Also walk CONDITIONAL branches (less common but possible in some
      // loop structures like do-while)
      funcOp.walk([&](LLVM::CondBrOp condBr) {
        if (isLoopBackedge(condBr, domInfo)) {
          // Only attach if not already annotated
          if (!condBr.getLoopAnnotationAttr()) {
            condBr.setLoopAnnotationAttr(loopAnnotation);
            hintsInFunc++;
            ARTS_DEBUG_TYPE("Attached vectorization hints to conditional "
                            "backedge at "
                            << condBr.getLoc());
          }
        }
      });

      if (hintsInFunc > 0) {
        ARTS_INFO("Attached vectorization hints to " << hintsInFunc
                  << " loop backedges in " << funcOp.getName());
        totalHints += hintsInFunc;
      }
    });

    ARTS_INFO("Total: attached vectorization hints to " << totalHints
              << " loop backedges");
    ARTS_INFO_FOOTER(LoopVectorizationHintsPass);
  }
};

} // end anonymous namespace

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
