///==========================================================================///
/// File: DataPointerHoisting.cpp
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

#include "../ArtsPassDetails.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_data_pointer_hoisting);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Check if a value is defined outside the given region
static bool isDefinedOutside(Region &region, Value value) {
  return !region.isAncestor(value.getParentRegion());
}

/// Check if an LLVM load is loading a data pointer from deps struct.
/// Pattern: llvm.load from a GEP that accesses the ptr field (offset 2) of
/// artsEdtDep_t struct.
static bool isDepsPtrLoad(LLVM::LoadOp loadOp) {
  Value addr = loadOp.getAddr();

  /// Check if loading a pointer type (ptr -> ptr)
  auto resultType = loadOp.getResult().getType();
  if (!isa<LLVM::LLVMPointerType>(resultType))
    return false;

  /// Check if the address comes from arts.dep_gep or arts.db_gep
  if (auto defOp = addr.getDefiningOp()) {
    /// arts.dep_gep returns (guid, ptr) - we want loads from the ptr result
    if (dyn_cast<DepGepOp>(defOp) || dyn_cast<DbGepOp>(defOp))
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

/// Find the outermost loop that contains the operation
static scf::ForOp findOutermostLoop(Operation *op) {
  scf::ForOp outermost = nullptr;
  Operation *parent = op->getParentOp();

  while (parent) {
    if (auto forOp = dyn_cast<scf::ForOp>(parent))
      outermost = forOp;
    parent = parent->getParentOp();
  }

  return outermost;
}

/// Check if all operands of an operation are defined outside the given region
static bool allOperandsDefinedOutside(Operation *op, Region &region) {
  for (Value operand : op->getOperands()) {
    if (!isDefinedOutside(region, operand))
      return false;
  }
  return true;
}

/// Hoist a load and its address computation (GEP) out of loops
static bool hoistLoadOutOfLoop(LLVM::LoadOp loadOp, scf::ForOp outermostLoop) {
  /// Get the address defining op (should be a GEP or arts op)
  Value addr = loadOp.getAddr();
  Operation *addrOp = addr.getDefiningOp();

  if (!addrOp)
    return false;

  Region &loopRegion = outermostLoop.getRegion();

  /// Check if we can hoist - all operands of the address op must be
  /// loop-invariant
  if (!allOperandsDefinedOutside(addrOp, loopRegion))
    return false;

  /// Move the address computation before the loop
  addrOp->moveBefore(outermostLoop);

  /// Move the load before the loop
  loadOp->moveBefore(outermostLoop);

  ARTS_INFO("Hoisted data pointer load: " << loadOp);
  return true;
}

struct DataPointerHoistingPass
    : public arts::ArtsDataPointerHoistingBase<DataPointerHoistingPass> {
  void runOnOperation() override;
};

} // namespace

void DataPointerHoistingPass::runOnOperation() {
  ModuleOp module = getOperation();
  ARTS_INFO_HEADER(DataPointerHoistingPass);

  int hoistedCount = 0;

  /// Process each function
  module.walk([&](func::FuncOp funcOp) {
    /// Only process EDT functions (start with __arts_edt_)
    if (!funcOp.getName().starts_with("__arts_edt_"))
      return;

    ARTS_DEBUG_TYPE("Processing EDT function: " << funcOp.getName());

    /// Collect loads to hoist (don't modify while iterating)
    SmallVector<std::pair<LLVM::LoadOp, scf::ForOp>> loadsToHoist;

    funcOp.walk([&](LLVM::LoadOp loadOp) {
      /// Check if this is a deps pointer load
      if (!isDepsPtrLoad(loadOp))
        return;

      /// Find outermost enclosing loop
      scf::ForOp outermostLoop = findOutermostLoop(loadOp);
      if (!outermostLoop)
        return;

      /// Check if address operands are loop-invariant
      Value addr = loadOp.getAddr();
      Operation *addrOp = addr.getDefiningOp();
      if (!addrOp)
        return;

      Region &loopRegion = outermostLoop.getRegion();
      if (!allOperandsDefinedOutside(addrOp, loopRegion))
        return;

      loadsToHoist.push_back({loadOp, outermostLoop});
    });

    /// Now hoist the collected loads
    for (auto &[loadOp, outermostLoop] : loadsToHoist) {
      if (hoistLoadOutOfLoop(loadOp, outermostLoop))
        hoistedCount++;
    }
  });

  ARTS_INFO("Hoisted " << hoistedCount << " data pointer loads out of loops");
  ARTS_INFO_FOOTER(DataPointerHoistingPass);
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {

std::unique_ptr<Pass> createDataPointerHoistingPass() {
  return std::make_unique<DataPointerHoistingPass>();
}

} // namespace arts
} // namespace mlir
