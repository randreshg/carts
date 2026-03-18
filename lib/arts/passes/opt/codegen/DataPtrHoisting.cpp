///==========================================================================///
/// File: DataPtrHoisting.cpp
///
/// This pass hoists data pointer loads from the ARTS deps struct out of loops.
/// Without this optimization, pointer loads from deps happen O(n^k) times for
/// k-nested loops, causing severe performance degradation.
///
/// Before (O(n^2) loads - pointer loaded every iteration):
///   scf.for %i = ... {
///     scf.for %j = ... {
///       %ptr = llvm.load %dep_ptr_addr : !llvm.ptr  /// Redundant load!
///       %val = polygeist.load %ptr[%i, %j] : f64
///     }
///   }
///
/// After (O(1) loads - pointer loaded once before loop nest):
///   %ptr = llvm.load %dep_ptr_addr : !llvm.ptr  /// Hoisted out
///   scf.for %i = ... {
///     scf.for %j = ... {
///       %val = polygeist.load %ptr[%i, %j] : f64
///     }
///   }
///
/// LLVM's LICM cannot hoist these loads without TBAA metadata proving
/// they don't alias with data stores.
///==========================================================================///

#include "arts/passes/PassDetails.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "polygeist/Ops.h"

#include "arts/Dialect.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/passes/Passes.h"
#include "arts/utils/LoopInvarianceUtils.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(data_ptr_hoisting);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Check if an LLVM load is loading a data pointer from deps struct.
/// Pattern: llvm.load from a GEP that accesses the ptr field (offset 2) of
/// artsEdtDep_t struct.
static bool isDepsPtrLoad(LLVM::LoadOp loadOp) {
  Value addr = loadOp.getAddr();

  /// Check if loading a pointer type (ptr -> ptr)
  auto resultType = loadOp.getResult().getType();
  if (!isa<LLVM::LLVMPointerType>(resultType))
    return false;

  /// Check if the address comes from arts.dep_gep
  if (auto defOp = addr.getDefiningOp()) {
    /// arts.dep_gep returns (guid, ptr) - we want loads from the ptr result
    if (dyn_cast<DepGepOp>(defOp))
      return true;

    /// Also check for GEP chains that access struct fields
    if (auto gepOp = dyn_cast<LLVM::GEPOp>(defOp)) {
      /// GEP into a struct type accessing field 2 (ptr field)
      auto sourceType = gepOp.getSourceElementType();
      if (auto structType = dyn_cast<LLVM::LLVMStructType>(sourceType)) {
        /// artsEdtDep_t is struct { i64, i32, ptr, i32, i1 }
        /// Field 2 is the ptr field
        return true;
      }
    }
  }

  return false;
}

/// Check if an LLVM load is loading a datablock pointer from a db_gep.
static bool isDbPtrLoad(LLVM::LoadOp loadOp) {
  auto resultType = loadOp.getResult().getType();
  if (!isa<LLVM::LLVMPointerType>(resultType))
    return false;
  if (auto defOp = loadOp.getAddr().getDefiningOp<DbGepOp>())
    return true;
  return false;
}

/// Find the highest loop that can legally hoist the address computation.
static scf::ForOp findHoistTarget(Operation *op, Operation *addrOp,
                                  DominanceInfo &domInfo) {
  scf::ForOp target = nullptr;
  for (Operation *parent = op->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (!loop)
      continue;
    Region &loopRegion = loop.getRegion();
    if (!allOperandsDefinedOutside(addrOp, loopRegion))
      break;
    if (!allOperandsDominate(addrOp, loop, domInfo))
      break;
    target = loop;
  }
  return target;
}

/// Hoist a load and its address computation (GEP) out of loops.
static bool hoistLoadOutOfLoop(LLVM::LoadOp loadOp, scf::ForOp targetLoop) {
  /// Get the address defining op (should be a GEP or arts op)
  Value addr = loadOp.getAddr();
  Operation *addrOp = addr.getDefiningOp();

  if (!addrOp)
    return false;
  if (!targetLoop || !targetLoop->isAncestor(loadOp))
    return false;

  if (targetLoop->isAncestor(addrOp))
    addrOp->moveBefore(targetLoop);
  loadOp->moveBefore(targetLoop);

  ARTS_INFO("Hoisted data pointer load: " << loadOp);
  return true;
}

struct DataPtrHoistingPass
    : public arts::DataPtrHoistingBase<DataPtrHoistingPass> {
  void runOnOperation() override;
};

} // namespace

void DataPtrHoistingPass::runOnOperation() {
  ModuleOp module = getOperation();
  ARTS_INFO_HEADER(DataPtrHoistingPass);

  int hoistedCount = 0, divRemHoisted = 0, dbPtrHoisted = 0, m2rHoisted = 0;
  module.walk([&](func::FuncOp funcOp) {
    bool isEdt = funcOp.getName().starts_with("__arts_edt_");
    SmallVector<std::pair<LLVM::LoadOp, scf::ForOp>> loadsToHoist;
    DominanceInfo domInfo(funcOp);
    if (isEdt) {
      funcOp.walk([&](LLVM::LoadOp loadOp) {
        /// Check if this is a deps pointer load
        if (!isDepsPtrLoad(loadOp))
          return;

        /// Check if address operands are loop-invariant
        Value addr = loadOp.getAddr();
        Operation *addrOp = addr.getDefiningOp();
        if (!addrOp)
          return;

        scf::ForOp targetLoop = findHoistTarget(loadOp, addrOp, domInfo);
        if (!targetLoop)
          return;

        loadsToHoist.push_back({loadOp, targetLoop});
      });
    }

    /// Now hoist the collected loads
    for (auto &[loadOp, targetLoop] : loadsToHoist) {
      if (hoistLoadOutOfLoop(loadOp, targetLoop))
        hoistedCount++;
    }

    /// Hoist datablock pointer loads (db_gep + llvm.load) out of inner loops.
    SmallVector<std::tuple<LLVM::LoadOp, Operation *, scf::ForOp>>
        dbLoadsToHoist;
    funcOp.walk([&](LLVM::LoadOp loadOp) {
      if (!isDbPtrLoad(loadOp))
        return;

      Operation *addrOp = loadOp.getAddr().getDefiningOp();
      if (!addrOp)
        return;

      scf::ForOp targetLoop = findHoistTarget(loadOp, addrOp, domInfo);
      if (!targetLoop)
        return;

      dbLoadsToHoist.push_back({loadOp, addrOp, targetLoop});
    });

    for (auto &[loadOp, addrOp, targetLoop] : dbLoadsToHoist) {
      if (!targetLoop || !targetLoop->isAncestor(loadOp))
        continue;
      if (targetLoop->isAncestor(addrOp))
        addrOp->moveBefore(targetLoop);
      loadOp->moveBefore(targetLoop);
      dbPtrHoisted++;

      /// Also hoist pointer2memref users if they live inside the loop.
      for (Operation *user :
           llvm::make_early_inc_range(loadOp.getResult().getUsers())) {
        if (auto m2r = dyn_cast<polygeist::Pointer2MemrefOp>(user)) {
          if (!targetLoop->isAncestor(m2r))
            continue;
          m2r->moveBefore(targetLoop);
          m2rHoisted++;
        }
      }
    }

    /// Hoist div/rem out of inner loops when operands are invariant and the
    /// divisor is provably non-zero.
    funcOp.walk([&](scf::ForOp loop) {
      SmallVector<Operation *> divRemOps;
      for (Operation &op : loop.getBody()->getOperations()) {
        if (op.hasTrait<OpTrait::IsTerminator>())
          continue;
        if (isa<scf::ForOp>(op))
          continue;
        if (!isa<arith::DivUIOp, arith::RemUIOp>(op))
          continue;
        if (!isSafeDivRemToHoist(&op, loop, domInfo))
          continue;
        divRemOps.push_back(&op);
      }
      for (Operation *op : divRemOps) {
        op->moveBefore(loop);
        divRemHoisted++;
      }
    });
  });

  ARTS_INFO("Hoisted " << hoistedCount << " data pointer loads out of loops");
  ARTS_INFO("Hoisted " << dbPtrHoisted
                       << " datablock pointer loads out of loops");
  ARTS_INFO("Hoisted " << m2rHoisted << " pointer2memref ops out of loops");
  ARTS_INFO("Hoisted " << divRemHoisted << " div/rem ops out of loops");
  ARTS_INFO_FOOTER(DataPtrHoistingPass);
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {

std::unique_ptr<Pass> createDataPtrHoistingPass() {
  return std::make_unique<DataPtrHoistingPass>();
}

} // namespace arts
} // namespace mlir
