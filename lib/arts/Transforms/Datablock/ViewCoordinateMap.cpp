///==========================================================================///
/// File: ViewCoordinateMap.cpp
///
/// Implementation of ViewCoordinateMap for global to local coordinate
/// transformation.
///==========================================================================///

#include "arts/Transforms/Datablock/ViewCoordinateMap.h"

using namespace mlir;
using namespace mlir::arts;

/// Create coordinate map from acquire operation.
/// For chunked mode (promotion_mode = "chunked"), uses offset_hints for
/// element-space localization since offsets contain chunk-space indices.
ViewCoordinateMap ViewCoordinateMap::fromAcquire(DbAcquireOp acquire) {
  ViewCoordinateMap map;
  map.numIndexedDims = acquire.getIndices().size();

  /// Check for promotion mode attribute to determine offset source
  auto modeAttr = acquire->getAttrOfType<PromotionModeAttr>("promotion_mode");
  if (modeAttr && modeAttr.getValue() == PromotionMode::chunked) {
    /// Chunked mode: use offset_hints for element-space localization
    /// because offsets contain chunk-space indices
    map.sliceOffsets.assign(acquire.getOffsetHints().begin(),
                            acquire.getOffsetHints().end());
  } else {
    /// Non-promoted or element-wise: use offsets directly (element-space)
    map.sliceOffsets.assign(acquire.getOffsets().begin(),
                            acquire.getOffsets().end());
  }
  return map;
}
