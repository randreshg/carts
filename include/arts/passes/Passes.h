///==========================================================================///
/// File: Passes.h
///
/// This file declares pass creation APIs and pass registration hooks
/// for the ARTS dialect.
///==========================================================================///

#ifndef ARTS_PASSES_PASSES_H
#define ARTS_PASSES_PASSES_H

#include "arts/Dialect.h"
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
class AnalysisManager;
class AbstractMachine;
std::unique_ptr<Pass> createArtsInlinerPass();
std::unique_ptr<Pass> createCollectMetadataPass();
std::unique_ptr<Pass> createCollectMetadataPass(bool exportMetadata,
                                                llvm::StringRef metadataFile);
std::unique_ptr<Pass> createCanonicalizeMemrefsPass();
std::unique_ptr<Pass> createConvertOpenMPtoArtsPass();
std::unique_ptr<Pass> createDCEPass();
std::unique_ptr<Pass> createEdtPass(AnalysisManager *AM, bool runAnalysis);
std::unique_ptr<Pass> createConcurrencyPass(AnalysisManager *AM);
std::unique_ptr<Pass> createCreateDbsPass(AnalysisManager *AM);
std::unique_ptr<Pass> createDbPass(AnalysisManager *AM);
std::unique_ptr<Pass> createDbOptsPass();
std::unique_ptr<Pass> createDbPartitioningPass(AnalysisManager *AM);
std::unique_ptr<Pass> createDistributedDbOwnershipPass(AnalysisManager *AM);
std::unique_ptr<Pass> createCreateEpochsPass();
std::unique_ptr<Pass> createConvertArtsToLLVMPass();
std::unique_ptr<Pass> createConvertArtsToLLVMPass(bool debug);
std::unique_ptr<Pass>
createConvertArtsToLLVMPass(bool debug, bool distributedInitPerWorker);
std::unique_ptr<Pass>
createConvertArtsToLLVMPass(bool debug, bool distributedInitPerWorker,
                            const AbstractMachine *machine);
std::unique_ptr<Pass> createEdtICMPass();
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
std::unique_ptr<Pass> createForOptPass(AnalysisManager *AM);
std::unique_ptr<Pass>
createDistributedHostLoopOutliningPass(AnalysisManager *AM);
std::unique_ptr<Pass> createEdtDistributionPass(AnalysisManager *AM);
std::unique_ptr<Pass> createForLoweringPass();
std::unique_ptr<Pass> createForLoweringPass(AnalysisManager *AM);
std::unique_ptr<Pass> createLoopFusionPass(AnalysisManager *AM);
std::unique_ptr<Pass> createEpochOptPass();
std::unique_ptr<Pass> createHoistingPass();
std::unique_ptr<Pass> createBlockLoopStripMiningPass();
std::unique_ptr<Pass> createDepTransformsPass();
std::unique_ptr<Pass> createStencilBoundaryPeelingPass();
std::unique_ptr<Pass> createLoopNormalizationPass(AnalysisManager *AM);
std::unique_ptr<Pass> createLoopReorderingPass(AnalysisManager *AM);
std::unique_ptr<Pass> createKernelTransformsPass(AnalysisManager *AM,
                                                 bool enableMatmul = true,
                                                 bool enableTiling = true,
                                                 int64_t tileJ = 64,
                                                 int64_t minTripCount = 128);
std::unique_ptr<Pass> createVerifyMetadataPass(AnalysisManager *AM,
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
#include "arts/passes/Passes.h.inc"

} // namespace mlir

#endif /// ARTS_PASSES_PASSES_H
