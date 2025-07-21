#ifndef CARTS_DIALECT_CARTS_PASSES_H
#define CARTS_DIALECT_CARTS_PASSES_H

// #include <memory>

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
std::unique_ptr<Pass> createArtsInlinerPass();
std::unique_ptr<Pass> createConvertOpenMPtoARTSPass();
std::unique_ptr<Pass> createEdtPass();
std::unique_ptr<Pass> createCreateDbsPass();
std::unique_ptr<Pass> createCreateDbsPass(bool identifyDbs);
std::unique_ptr<Pass> createDbPass();
std::unique_ptr<Pass> createCreateEpochsPass();
std::unique_ptr<Pass> createConvertArtsToLLVMPass();
std::unique_ptr<Pass> createConvertArtsToLLVMPass(bool debug);
std::unique_ptr<Pass> createEdtInvariantCodeMotionPass();
std::unique_ptr<Pass> createEdtPtrRematerializationPass();
std::unique_ptr<Pass> createConvertDbToOpaquePtrPass();
std::unique_ptr<Pass> createEdtLoweringPass();
} // namespace arts
} // namespace mlir

namespace mlir {
// Forward declaration from Dialect.h
template <typename ConcreteDialect>
void registerDialect(DialectRegistry &registry);

namespace omp {
class OpenMPDialect;
} // end namespace omp

namespace memref {
class MemRefDialect;
} // end namespace memref

namespace LLVM {
class LLVMDialect;
} // end namespace LLVM

namespace func {
class FuncDialect;
} // end namespace func

namespace arith {
class ArithDialect;
} // end namespace arith

namespace polygeist {
class PolygeistDialect;
} // end namespace polygeist

#define GEN_PASS_REGISTRATION
#include "arts/Passes/ArtsPasses.h.inc"

} // end namespace mlir

#endif // CARTS_DIALECT_CARTS_PASSES_H
