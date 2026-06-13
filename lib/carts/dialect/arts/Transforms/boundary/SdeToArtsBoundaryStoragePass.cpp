///==========================================================================///
/// File: SdeToArtsBoundaryStoragePass.cpp
/// sde-storage-to-arts-db pass runner.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCommon.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryPasses.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/DenseMap.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

LogicalResult lowerMuData(sde::SdeMuDataOp op);
LogicalResult lowerMuAlloc(sde::SdeMuAllocOp op,
                           ArrayRef<sde::SdeMuAccessWindowOp> windows,
                           ArrayAttr haloShape);
LogicalResult realizeTaskDepMemrefStorage(ModuleOp module);

LogicalResult runSdeStorageToArtsDb(ModuleOp module) {
  llvm::DenseMap<Value, HaloRedistFacts> haloFactsByMu;
  SmallVector<Operation *> redists;
  if (failed(validateAndCollectStorageRedists(module, haloFactsByMu, redists)))
    return failure();

  if (failed(rejectUnsupportedSdeCarriers(module)))
    return failure();

  DenseMap<Value, SmallVector<sde::SdeMuAccessWindowOp, 4>> windowsByMu;
  module.walk([&](sde::SdeMuAccessWindowOp window) {
    windowsByMu[window.getMu()].push_back(window);
  });

  SmallVector<sde::SdeMuDataOp> muDatas;
  module.walk([&](sde::SdeMuDataOp op) { muDatas.push_back(op); });
  for (sde::SdeMuDataOp op : muDatas)
    if (failed(lowerMuData(op)))
      return failure();

  SmallVector<sde::SdeMuAllocOp> muAllocs;
  module.walk([&](sde::SdeMuAllocOp op) { muAllocs.push_back(op); });
  for (sde::SdeMuAllocOp op : muAllocs) {
    auto it = windowsByMu.find(op.getMemref());
    ArrayRef<sde::SdeMuAccessWindowOp> windows =
        it == windowsByMu.end() ? ArrayRef<sde::SdeMuAccessWindowOp>()
                                : ArrayRef<sde::SdeMuAccessWindowOp>(it->second);
    ArrayAttr haloShape;
    auto haloIt = haloFactsByMu.find(op.getMemref());
    if (haloIt != haloFactsByMu.end())
      haloShape = haloIt->second.haloShape;
    if (failed(lowerMuAlloc(op, windows, haloShape)))
      return failure();
  }

  if (failed(realizeTaskDepMemrefStorage(module)))
    return failure();

  SmallVector<sde::SdeMuAccessWindowOp> residualWindows;
  module.walk([&](sde::SdeMuAccessWindowOp op) { residualWindows.push_back(op); });
  for (sde::SdeMuAccessWindowOp window : residualWindows) {
    window.emitOpError() << "was not consumed during SDE storage realization";
    return failure();
  }

  for (Operation *redist : redists)
    if (redist && redist->getBlock())
      redist->erase();
  return success();
}

} // namespace mlir::carts::arts::boundary
