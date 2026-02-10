///==========================================================================///
/// File: StencilEdtRewriter.cpp
///
/// Stencil-mode acquire rewriting for task EDT dependencies.
///
/// Example transformation:
///   Before:
///     %acq = arts.db_acquire[<in>] (...) partitioning(<block>)
///            , offsets[%off], sizes[%size]
///
///   After (halo-expanded):
///     %haloStart = select(%off >= 1, %off - 1, 0)
///     %haloEnd = min(%off + %size + 1, %extent)
///     %haloSize = %haloEnd - %haloStart
///     %acqHalo = arts.db_acquire[<in>] (...) partitioning(<block>)
///                , offsets[%haloStart], sizes[%haloSize]
///==========================================================================///

#include "arts/Transforms/Edt/EdtRewriter.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

class StencilEdtRewriter final : public EdtRewriter {
public:
  DbAcquireOp rewriteAcquire(AcquireRewriteInput &input) const override {
    return mlir::arts::detail::rewriteAcquireAsBlock(
        input, /*applyStencilHalo=*/true);
  }
};

} // namespace

std::unique_ptr<EdtRewriter> mlir::arts::detail::createStencilEdtRewriter() {
  return std::make_unique<StencilEdtRewriter>();
}
