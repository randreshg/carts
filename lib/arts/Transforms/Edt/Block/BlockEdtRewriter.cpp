///==========================================================================///
/// File: BlockEdtRewriter.cpp
///
/// Block-mode acquire rewriting for task EDT dependencies.
///
/// Example transformation:
///   Before:
///     %acq = arts.db_acquire[<inout>] (...) partitioning(<coarse>)
///            , offsets[%c0], sizes[%c1]
///
///   After:
///     %acqChunk = arts.db_acquire[<inout>] (...) partitioning(<block>,
///                  offsets[%partOff0, ...], sizes[%partSize0, ...])
///                  , offsets[%off0], sizes[%size0]
///
///   Notes:
///   - DB-space offsets/sizes stay rank-consistent with the source acquire.
///   - Additional owner hints are appended to partition_offsets/sizes only.
///==========================================================================///

#include "arts/Transforms/Edt/EdtRewriter.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

class BlockEdtRewriter final : public EdtRewriter {
public:
  DbAcquireOp rewriteAcquire(AcquireRewriteInput &input) const override {
    return mlir::arts::detail::rewriteAcquireAsBlock(
        input, /*applyStencilHalo=*/false);
  }
};

} // namespace

std::unique_ptr<EdtRewriter> mlir::arts::detail::createBlockEdtRewriter() {
  return std::make_unique<BlockEdtRewriter>();
}
