///==========================================================================///
/// File: DbPatternMatchers.h
///
/// Shared DB-oriented loop pattern matchers used by analysis passes.
///==========================================================================///

#ifndef ARTS_ANALYSIS_DB_DBPATTERNMATCHERS_H
#define ARTS_ANALYSIS_DB_DBPATTERNMATCHERS_H

#include "arts/Dialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir {
namespace arts {

class LoopAnalysis;

/// Match lb = outerIV + c, where c is a positive constant.
std::optional<int64_t> matchTriangularOffset(Value lb, Value outerIV);

/// Detect whether a loop contains a nested bound that depends on its IV.
bool hasTriangularBoundPattern(ForOp forOp);

/// Detect update-form matmul kernels in arts.for regions:
///   C[...] = C[...] + A[...] * B[...]
bool detectMatmulUpdatePattern(ForOp forOp, LoopAnalysis *loopAnalysis);

/// Match result for update-form matmul nests with an init section:
///   arts.for { scf.for(j) { <init ops>; scf.for(k) { ... } } }
struct MatmulInitReductionLoopMatch {
  scf::ForOp jLoop;
  scf::ForOp kLoop;
  SmallVector<Operation *, 8> initOps;
};

/// Detect update-form matmul nest and expose j/k loop structure.
bool detectMatmulInitReductionLoopNest(ForOp artsFor,
                                       LoopAnalysis *loopAnalysis,
                                       MatmulInitReductionLoopMatch &out);

/// Match result for symmetric triangular loops.
struct SymmetricTriangularPatternMatch {
  scf::ForOp jLoop;
  int64_t triangularOffset = 0;
  memref::StoreOp storeIJ;   /// store to C[i,j]
  memref::StoreOp storeJI;   /// store to C[j,i]
  Value memC;                /// target memref
  memref::StoreOp diagStore; /// store to C[i,i]
  Value diagValue;           /// value stored on diagonal
};

/// Detect symmetric triangular loops used by normalization.
bool detectSymmetricTriangularPattern(ForOp artsFor,
                                      SymmetricTriangularPatternMatch &out);

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBPATTERNMATCHERS_H
