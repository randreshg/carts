///==========================================================================///
/// File: EdtRewriter.h
///
/// Rewriters for creating per-worker DbAcquireOps during ForLowering.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_EDT_EDTREWRITER_H
#define ARTS_TRANSFORMS_EDT_EDTREWRITER_H

#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "llvm/ADT/SmallVector.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include <memory>

namespace mlir {
namespace arts {

/// Input bundle for per-dependency acquire rewriting.
struct AcquireRewriteInput {
  ArtsCodegen *AC = nullptr;
  Location loc;
  DbAcquireOp parentAcquire;
  Value rootGuid;
  Value rootPtr;
  Value zero;
  Value one;
  Value acquireOffset;
  Value acquireSize;
  Value acquireHintSize;
  /// Additional per-dimension partition slices for N-D block partitioning.
  SmallVector<Value, 4> extraOffsets;
  SmallVector<Value, 4> extraSizes;
  SmallVector<Value, 4> extraHintSizes;
  Value step;
  bool stepIsUnit = true;
  bool singleElement = false;
  bool forceCoarse = false;
  Value stencilExtent; // optional
};

class EdtRewriter {
public:
  virtual ~EdtRewriter() = default;

  /// Create the rewritten DbAcquireOp for one dependency.
  virtual DbAcquireOp rewriteAcquire(AcquireRewriteInput &input) const = 0;

  /// Factory for block vs stencil acquire rewriting.
  static std::unique_ptr<EdtRewriter> create(bool stencilHalo);
};

namespace detail {

/// Shared block-style acquire rewrite implementation with optional halo logic.
DbAcquireOp rewriteAcquireAsBlock(AcquireRewriteInput &input,
                                  bool applyStencilHalo);

/// Internal factories used by EdtRewriter::create.
std::unique_ptr<EdtRewriter> createBlockEdtRewriter();
std::unique_ptr<EdtRewriter> createStencilEdtRewriter();

} // namespace detail

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_EDT_EDTREWRITER_H
