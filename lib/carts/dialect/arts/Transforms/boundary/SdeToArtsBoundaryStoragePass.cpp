///==========================================================================///
/// File: SdeToArtsBoundaryStoragePass.cpp
/// sde-storage-to-arts-db pass runner.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCommon.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryPasses.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/DenseMap.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

LogicalResult lowerMuData(sde::SdeMuDataOp op);
LogicalResult lowerMuAlloc(sde::SdeMuAllocOp op, ArrayAttr haloShape);
LogicalResult realizeTaskDepMemrefStorage(
    ModuleOp module,
    const llvm::DenseMap<Value, HaloRedistFacts> &haloFactsByMu);

LogicalResult runSdeStorageToArtsDb(ModuleOp module) {
  llvm::DenseMap<Value, HaloRedistFacts> haloFactsByMu;
  SmallVector<Operation *> redists;
  if (failed(validateAndCollectStorageRedists(module, haloFactsByMu, redists)))
    return failure();

  if (failed(rejectUnsupportedSdeCarriers(module)))
    return failure();

  SmallVector<sde::SdeMuDataOp> muDatas;
  module.walk([&](sde::SdeMuDataOp op) { muDatas.push_back(op); });
  for (sde::SdeMuDataOp op : muDatas)
    if (failed(lowerMuData(op)))
      return failure();

  if (failed(realizeTaskDepMemrefStorage(module, haloFactsByMu)))
    return failure();

  SmallVector<sde::SdeMuAllocOp> muAllocs;
  module.walk([&](sde::SdeMuAllocOp op) { muAllocs.push_back(op); });
  for (sde::SdeMuAllocOp op : muAllocs) {
    ArrayAttr haloShape;
    auto haloIt = haloFactsByMu.find(op.getMemref());
    if (haloIt != haloFactsByMu.end())
      haloShape = haloIt->second.haloShape;
    if (failed(lowerMuAlloc(op, haloShape)))
      return failure();
  }

  for (Operation *redist : redists)
    if (redist && redist->getBlock())
      redist->erase();
  return success();
}

} // namespace mlir::carts::arts::boundary
