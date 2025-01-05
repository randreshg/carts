#ifndef CARTS_DIALECT_CARTS_PASSES_H
#define CARTS_DIALECT_CARTS_PASSES_H

#include <memory>

#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "mlir/Conversion/LLVMCommon/LoweringOptions.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/Vector/IR/VectorOps.h"
#include "mlir/Pass/Pass.h"


#include "arts/ArtsDialect.h"


namespace mlir {
class PatternRewriter;
class RewritePatternSet;
class DominanceInfo;

namespace arts {
std::unique_ptr<Pass> createConvertOpenMPtoARTSPass();
std::unique_ptr<Pass> createConvertARTSToFuncsPass();
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

#define GEN_PASS_REGISTRATION
#include "arts/Passes/ArtsPasses.h.inc"

} // end namespace mlir

#endif // CARTS_DIALECT_CARTS_PASSES_H
