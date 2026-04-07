///==========================================================================///
/// File: EdtLoweringInternal.h
///
/// Local implementation contract for EdtLowering. This header is
/// intentionally private to the edt-lowering implementation split and
/// should not be used as shared compiler infrastructure.
///==========================================================================///

#ifndef ARTS_PASSES_TRANSFORMS_EDTLOWERINGINTERNAL_H
#define ARTS_PASSES_TRANSFORMS_EDTLOWERINGINTERNAL_H

#include "arts/Dialect.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/codegen/Codegen.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/PartitionPredicates.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
#include <optional>

namespace mlir::arts::edt_lowering {

///===----------------------------------------------------------------------===//
/// Constants
///===----------------------------------------------------------------------===//

static constexpr int32_t kArtsDepFlagPreferDuplicate = 1 << 0;
static constexpr int32_t kArtsDepFlagPreserveShape = 1 << 1;

///===----------------------------------------------------------------------===//
/// Structs
///===----------------------------------------------------------------------===//

struct NormalizedElementSlice {
  SmallVector<Value, 4> offsets;
  SmallVector<Value, 4> sizes;
  Value representable;
  Value contiguous;
  Value wholeBlock;
};

struct DepSourceInfo {
  DbAcquireOp dbAcquire;
  DepDbAcquireOp depDbAcquire;
};

///===----------------------------------------------------------------------===//
/// Helper Functions
///===----------------------------------------------------------------------===//

void normalizeTaskDepSlice(ArtsCodegen *AC, DbAcquireOp acquire,
                           const LoweringContractInfo &contract);

std::optional<NormalizedElementSlice>
normalizeCommonElementSlice(ArtsCodegen *AC, DbAcquireOp acquire,
                            DbAllocOp alloc);

Value loadRepresentativeGuidScalar(ArtsCodegen *AC, Location loc,
                                   Value guidStorage);

DepSourceInfo resolveDepSource(Value dep);

Operation *getCanonicalDependencySource(Value dep);

} // namespace mlir::arts::edt_lowering

#endif // ARTS_PASSES_TRANSFORMS_EDTLOWERINGINTERNAL_H
