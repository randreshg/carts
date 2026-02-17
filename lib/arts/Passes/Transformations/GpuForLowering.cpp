///==========================================================================///
/// File: GpuForLowering.cpp
///
/// Lower GPU-marked arts.for loops to arts.gpu_edt regions.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(gpu_for_lowering);

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool getConstantTripCount(ForOp op, int64_t &tripCount, int64_t &lb,
                                 int64_t &step) {
  int64_t ub = 0;
  if (!ValueUtils::getConstantIndex(op.getLowerBound()[0], lb))
    return false;
  if (!ValueUtils::getConstantIndex(op.getUpperBound()[0], ub))
    return false;
  if (!ValueUtils::getConstantIndex(op.getStep()[0], step))
    return false;
  if (step <= 0)
    return false;
  if (ub <= lb) {
    tripCount = 0;
    return true;
  }
  tripCount = (ub - lb + step - 1) / step;
  return true;
}

/// Get the base memref value, stopping at block arguments.
/// This is important for GPU EDTs - we want the block argument that
/// represents the dependency interface, not the original db_alloc.
static Value getBaseMemrefStopAtBlockArg(Value v) {
  while (v) {
    // Stop at block arguments - these are the dependency interface
    if (v.isa<BlockArgument>())
      return v;

    Operation *op = v.getDefiningOp();
    if (!op)
      return v;

    // Trace through db_ref to get the underlying datablock ptr
    if (auto dbRef = dyn_cast<DbRefOp>(op)) {
      v = dbRef.getSource();
      continue;
    }
    // Trace through casts
    if (auto cast = dyn_cast<memref::CastOp>(op)) {
      v = cast.getSource();
      continue;
    }
    if (auto viewLike = dyn_cast<ViewLikeOpInterface>(op)) {
      v = viewLike.getViewSource();
      continue;
    }
    // Stop at other operations (allocs, acquires, etc.)
    return v;
  }
  return v;
}

static bool isDbBackedDependency(Value dep) {
  Operation *underlyingDb = DatablockUtils::getUnderlyingDb(dep);
  return isa_and_nonnull<DbAcquireOp, DepDbAcquireOp>(underlyingDb);
}

static void collectMemrefAccesses(ForOp op, SmallVector<Value> &reads,
                                  SmallVector<Value> &writes) {
  llvm::SmallPtrSet<Value, 8> readSet;
  llvm::SmallPtrSet<Value, 8> writeSet;

  op.getRegion().walk([&](Operation *inner) {
    if (auto load = dyn_cast<memref::LoadOp>(inner)) {
      Value base = getBaseMemrefStopAtBlockArg(load.getMemref());
      if (readSet.insert(base).second)
        reads.push_back(base);
      return;
    }
    if (auto store = dyn_cast<memref::StoreOp>(inner)) {
      Value base = getBaseMemrefStopAtBlockArg(store.getMemref());
      if (writeSet.insert(base).second)
        writes.push_back(base);
      return;
    }
  });
}

struct GpuForLoweringPass : public arts::GpuForLoweringBase<GpuForLoweringPass> {
  GpuForLoweringPass() = default;
  explicit GpuForLoweringPass(llvm::StringRef config) {
    configPath = config.str();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    ArtsAbstractMachine machine(configPath);
    if (!machine.hasGpuSupport())
      return;

    MLIRContext *context = module.getContext();

    SmallVector<ForOp, 8> candidates;
    module.walk([&](ForOp forOp) {
      bool shouldLower = false;
      if (auto profitability = forOp->getAttrOfType<GpuProfitabilityAttr>(
              AttrNames::Operation::GpuProfitability)) {
        if (profitability.getIsEmbarrassinglyParallel().getValue())
          shouldLower = true;
      }
      if (auto targetAttr = forOp->getAttrOfType<GpuTargetAttr>(
              AttrNames::Operation::GpuTarget)) {
        if (targetAttr.getValue() != GpuTarget::none)
          shouldLower = true;
      }
      if (shouldLower)
        candidates.push_back(forOp);
    });

    for (ForOp forOp : candidates) {
      bool forceGpuLowering = false;
      if (auto targetAttr = forOp->getAttrOfType<GpuTargetAttr>(
              AttrNames::Operation::GpuTarget)) {
        auto target = targetAttr.getValue();
        if (target == GpuTarget::gpu || target == GpuTarget::Auto)
          forceGpuLowering = true;
      }

      int64_t tripCount = 0;
      int64_t lb = 0;
      int64_t step = 1;
      if (!getConstantTripCount(forOp, tripCount, lb, step))
        continue;
      if (lb != 0 || step != 1)
        continue;
      if (!forceGpuLowering && tripCount < machine.getGpuMinIterations())
        continue;

      auto parentEdt = forOp->getParentOfType<EdtOp>();
      if (!parentEdt)
        continue;

      Location loc = forOp.getLoc();
      OpBuilder builder(forOp);

      SmallVector<Value> reads;
      SmallVector<Value> writes;
      collectMemrefAccesses(forOp, reads, writes);
      if (reads.empty() && writes.empty())
        continue;

      llvm::SmallPtrSet<Value, 8> depsSet;
      SmallVector<Value> deps;
      auto recordDep = [&](Value dep) {
        if (depsSet.insert(dep).second)
          deps.push_back(dep);
      };

      for (Value dep : reads)
        recordDep(dep);
      for (Value dep : writes)
        recordDep(dep);

      bool allDepsDbBacked = llvm::all_of(
          deps, [](Value dep) { return isDbBackedDependency(dep); });
      if (!allDepsDbBacked) {
        ARTS_DEBUG("Skipping GPU lowering: non-DB dependency detected in loop "
                   << forOp.getLoc());
        continue;
      }

      for (Value dep : deps) {
        if (auto alloc =
                dyn_cast_or_null<DbAllocOp>(DatablockUtils::getUnderlyingDbAlloc(dep))) {
          DbMode mode = DbMode::read;
          bool isWriter = llvm::is_contained(writes, dep);
          if (isWriter)
            mode = DbMode::gpu_write;
          else
            mode = DbMode::gpu_read;
          alloc.setDbModeAttr(DbModeAttr::get(context, mode));
        }
      }

      int64_t blockSize = 256;
      int64_t gridSize = (tripCount + blockSize - 1) / blockSize;
      auto launchConfig = GpuLaunchConfigAttr::get(
          context, gridSize, 1, 1, blockSize, 1, 1);

      Value route = parentEdt.getRoute();
      if (!route)
        route = builder.create<arith::ConstantIntOp>(loc, 0, 32);

      builder.setInsertionPoint(forOp);
      // Note: Explicit GpuMemcpyOp not needed - ARTS runtime handles H2D/D2H
      // transfers automatically based on dependency modes (gpu_read, gpu_write)

      auto gpuEdt = builder.create<GpuEdtOp>(loc, route, launchConfig, deps,
                                             Value(), Value(), Value());

      Region &gpuRegion = gpuEdt.getBody();
      if (gpuRegion.empty())
        gpuRegion.push_back(new Block());
      Block &gpuBlock = gpuRegion.front();
      builder.setInsertionPointToStart(&gpuBlock);

      Value tid = builder.create<gpu::ThreadIdOp>(loc, gpu::Dimension::x);
      Value bid = builder.create<gpu::BlockIdOp>(loc, gpu::Dimension::x);
      Value blockDim = builder.create<gpu::BlockDimOp>(loc, gpu::Dimension::x);
      Value globalIdx = builder.create<arith::AddIOp>(
          loc, tid, builder.create<arith::MulIOp>(loc, bid, blockDim));

      Value upperBound = forOp.getUpperBound()[0];
      Value cmp = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::slt,
                                                globalIdx, upperBound);
      auto ifOp = builder.create<scf::IfOp>(loc, cmp, false);

      IRMapping mapping;
      mapping.map(forOp.getRegion().front().getArguments()[0], globalIdx);
      Block &thenBlock = ifOp.getThenRegion().front();
      // Remove any auto-generated terminator so we can clone in our ops
      if (!thenBlock.empty() && thenBlock.back().hasTrait<OpTrait::IsTerminator>())
        thenBlock.back().erase();
      builder.setInsertionPointToEnd(&thenBlock);
      for (Operation &op : forOp.getRegion().front().without_terminator())
        builder.clone(op, mapping);
      builder.create<scf::YieldOp>(loc);

      builder.setInsertionPointToEnd(&gpuBlock);
      builder.create<arts::YieldOp>(loc);

      // Note: lc_sync for GPU EDT completion is not needed here because:
      // 1. The GPU EDT is nested within the parent EDT's region
      // 2. Sequential execution within the EDT ensures completion before db_release
      // TODO: Add proper GPU synchronization when implementing async GPU execution

      forOp.erase();
    }
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createGpuForLoweringPass() {
  return std::make_unique<GpuForLoweringPass>();
}

std::unique_ptr<Pass>
mlir::arts::createGpuForLoweringPass(llvm::StringRef configPath) {
  return std::make_unique<GpuForLoweringPass>(configPath);
}
