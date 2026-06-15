//===-- SuLoopAccessAnalysisDetail.h --------------------------------------===//
//
// Internal call surface shared by the SuLoopAccessAnalysis carve. The reusable
// structural analysis of SDE scheduling-unit loops is split into cohesive
// translation units that all back the public wrappers in
// SuLoopAccessAnalysis.cpp:
//   * PerfectNestCollect.cpp   - perfect loop-nest detection + local-scratch
//                                side-effect tolerance.
//   * MemrefAccessCollect.cpp  - affine/memref access-entry collection.
//   * StructuredClassify.cpp   - iterator-type + structured-pattern classify.
//   * NeighborhoodAnalysis.cpp - stencil neighborhood offset extraction.
//
// These declarations carry no policy; they are only the boundary between the
// carved units. Everything else stays file-local (`static`) in its unit.
//
//===----------------------------------------------------------------------===//

#ifndef CARTS_DIALECT_SDE_ANALYSIS_SULOOPACCESSANALYSISDETAIL_H
#define CARTS_DIALECT_SDE_ANALYSIS_SULOOPACCESSANALYSISDETAIL_H

#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"

#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Interfaces/FunctionInterfaces.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallVector.h"

#include <optional>

namespace mlir::carts::sde::detail {

//===----------------------------------------------------------------------===//
// PerfectNestCollect.cpp
//===----------------------------------------------------------------------===//

// Collect the perfect loop nest rooted at `iterOp` into `info` (induction vars
// + innermost body). Returns false when the body is not a recognized nest.
bool collectPerfectNest(SdeSuIterateOp iterOp, LoopNestInfo &info);

// Whether `op` is an exact libc allocator/free call for a local scratch root
// used only inside `regions`. Shared with MemrefAccessCollect's per-body check.
bool isLocalLibcAllocatorScratchCall(Operation *op, Block &scope,
                                     ArrayRef<Operation *> regions);
bool isLocalLibcFreeScratchCall(Operation *op, Block &scope,
                                ArrayRef<Operation *> regions);

//===----------------------------------------------------------------------===//
// MemrefAccessCollect.cpp
//===----------------------------------------------------------------------===//

// Collect read/write memref access entries (remapped to loop IVs) under `body`.
// Returns false when an access cannot be represented; true requires at least
// one real access.
bool collectMemrefAccesses(Operation *scope, Block &body, ArrayRef<Value> ivs,
                           SmallVectorImpl<MemrefAccessEntry> &reads,
                           SmallVectorImpl<MemrefAccessEntry> &writes,
                           MLIRContext *ctx);

//===----------------------------------------------------------------------===//
// StructuredClassify.cpp
//===----------------------------------------------------------------------===//

// Iterator types (parallel/reduction) for `numDims` loops given the output
// access maps.
void computeIteratorTypes(unsigned numDims, ArrayRef<AffineMap> outputMaps,
                          SmallVectorImpl<utils::IteratorType> &iterTypes);

// Loop dims (re)used by an affine access map.
llvm::SmallBitVector getUsedDims(AffineMap map, unsigned numDims);

// Structured pattern classification from the access maps + iterator types.
SdeStructuredClassification
classifyPattern(ArrayRef<MemrefAccessEntry> reads,
                ArrayRef<AffineMap> outputMaps,
                ArrayRef<utils::IteratorType> iterTypes, unsigned numDims);

// Whether the SU is a single rank-1 reduction-carrier accumulator subset.
bool supportsReductionCarrierSubset(SdeSuIterateOp iterOp,
                                    const LoopNestInfo &nest,
                                    ArrayRef<MemrefAccessEntry> reads,
                                    ArrayRef<MemrefAccessEntry> writes);

//===----------------------------------------------------------------------===//
// NeighborhoodAnalysis.cpp
//===----------------------------------------------------------------------===//

// Stencil neighborhood min/max offsets from the read access entries, or
// nullopt when no neighborhood offset is present.
std::optional<SuNeighborhoodAccessInfo>
extractNeighborhoodAccessInfo(ArrayRef<MemrefAccessEntry> reads,
                              unsigned numLoops);

} // namespace mlir::carts::sde::detail

#endif // CARTS_DIALECT_SDE_ANALYSIS_SULOOPACCESSANALYSISDETAIL_H
