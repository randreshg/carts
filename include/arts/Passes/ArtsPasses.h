///==========================================================================///
/// File: ArtsPasses.h
///
/// This file defines the ArtsPasses class which is used to register the passes
/// for the ARTS dialect.
///==========================================================================///

#ifndef ARTS_PASSES_ARTSPASSES_H
#define ARTS_PASSES_ARTSPASSES_H

#include "arts/ArtsDialect.h"
#include "mlir/Conversion/LLVMCommon/LoweringOptions.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/Vector/IR/VectorOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "polygeist/Dialect.h"

namespace mlir {
class PatternRewriter;
class RewritePatternSet;
class DominanceInfo;

namespace arts {
class ArtsAnalysisManager;
class ArtsAbstractMachine;
std::unique_ptr<Pass> createArtsInlinerPass();
std::unique_ptr<Pass> createCollectMetadataPass();
std::unique_ptr<Pass> createCollectMetadataPass(bool exportMetadata,
                                                llvm::StringRef metadataFile);
std::unique_ptr<Pass> createCanonicalizeMemrefsPass();
std::unique_ptr<Pass> createConvertOpenMPtoArtsPass();
std::unique_ptr<Pass> createDeadCodeEliminationPass();
std::unique_ptr<Pass> createEdtPass(ArtsAnalysisManager *AM, bool runAnalysis);
std::unique_ptr<Pass> createConcurrencyPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createCreateDbsPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createDbPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createDbPartitioningPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createDistributedDbOwnershipPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createCreateEpochsPass();
std::unique_ptr<Pass> createConvertArtsToLLVMPass();
std::unique_ptr<Pass> createConvertArtsToLLVMPass(bool debug);
std::unique_ptr<Pass>
createConvertArtsToLLVMPass(bool debug, bool distributedInitPerWorker);
std::unique_ptr<Pass>
createConvertArtsToLLVMPass(bool debug, bool distributedInitPerWorker,
                            const ArtsAbstractMachine *machine);
std::unique_ptr<Pass> createEdtInvariantCodeMotionPass();
std::unique_ptr<Pass> createEdtAllocaSinkingPass();
std::unique_ptr<Pass> createDataPointerHoistingPass();
std::unique_ptr<Pass> createAliasScopeGenPass();
std::unique_ptr<Pass> createLoopVectorizationHintsPass();
std::unique_ptr<Pass> createScalarReplacementPass();
std::unique_ptr<Pass> createEdtPtrRematerializationPass();
std::unique_ptr<Pass> createDbLoweringPass(uint64_t idStride = 1000);
std::unique_ptr<Pass> createEpochLoweringPass();
std::unique_ptr<Pass> createParallelEdtLoweringPass();
std::unique_ptr<Pass> createEdtLoweringPass(uint64_t idStride = 1000);
std::unique_ptr<Pass> createArtsForOptimizationPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass>
createDistributedHostLoopOutliningPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createEdtDistributionPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createForLoweringPass();
std::unique_ptr<Pass> createLoopFusionPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createEpochOptPass();
std::unique_ptr<Pass> createArtsHoistingPass();
std::unique_ptr<Pass> createBlockLoopStripMiningPass();
std::unique_ptr<Pass> createLoopNormalizationPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createLoopReorderingPass(ArtsAnalysisManager *AM);
std::unique_ptr<Pass> createLoopTransformsPass(ArtsAnalysisManager *AM,
                                               bool enableMatmul = true,
                                               bool enableTiling = true,
                                               int64_t tileJ = 64,
                                               int64_t minTripCount = 128);
std::unique_ptr<Pass> createVerifyMetadataPass(ArtsAnalysisManager *AM,
                                               bool failOnMissing = false);
} // namespace arts
} // namespace mlir

namespace mlir {
/// Forward declaration from Dialect.h
template <typename ConcreteDialect>
void registerDialect(DialectRegistry &registry);

namespace omp {
class OpenMPDialect;
} // namespace omp

namespace memref {
class MemRefDialect;
} // namespace memref

namespace LLVM {
class LLVMDialect;
} // namespace LLVM

namespace func {
class FuncDialect;
} // namespace func

namespace arith {
class ArithDialect;
} // namespace arith

namespace polygeist {
class PolygeistDialect;
} // namespace polygeist

#define GEN_PASS_REGISTRATION
#include "arts/Passes/ArtsPasses.h.inc"

} // namespace mlir

#endif /// CARTS_DIALECT_CARTS_PASSES_H
