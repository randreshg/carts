//===- ArtsDatablockIdentificationPass.cpp - Identify Candidate Datablocks ----===//
//
// This pass analyzes EDT regions within a function to discover candidate
// datablocks. A candidate datablock is a memref value used in an EDT region
// that is defined outside (or remains invariant in) that region. In addition,
// the pass classifies the candidate into one of three categories based on its
// access pattern: read‑only, write‑only, or read–write.
//
// The analysis proceeds as follows:
///   1. Collect all values used in the region that are defined in an ancestor.
///   2. Filter those values to keep only memrefs.
///   3. For each such value, use dominance analysis to check if its value remains
///      invariant (i.e. is not written to) throughout the region.
///   4. Use simple access analysis (by scanning load/store)
///      to determine if the value is used only for reading, only for writing,
///      or for both.
///   5. Print the summary of candidate datablocks.
///
///===----------------------------------------------------------------------===//

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Dominance.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;

///===----------------------------------------------------------------------===///
/// AccessType: Classification of memref accesses.
///===----------------------------------------------------------------------===///
enum class AccessType { ReadOnly, WriteOnly, ReadWrite, Unknown };

///===----------------------------------------------------------------------===///
/// CandidateDatablock: Structure representing a candidate datablock.
///===----------------------------------------------------------------------===///
struct CandidateDatablock {
  Value memref;                              ///< The memref value.
  bool invariant = false;                    ///< True if the memref remains invariant in the region.
  SmallVector<Value, 4> pinnedIndices;       ///< Pinned indices (if invariant).
  AccessType access = AccessType::Unknown;   ///< Classification: read-only, write-only, or read–write.
};

///===----------------------------------------------------------------------===///
/// Helper: Analyze Access Pattern
///
/// Walk the given region R and examine operations that access the memref v.
/// Returns an AccessType value indicating whether the memref is only read,
/// only written, or both.
///===----------------------------------------------------------------------===///
static AccessType analyzeAccessPattern(Value v, Region *R) {
  bool hasLoad = false;
  bool hasStore = false;
  // Iterate over all operations in region R (recursively).
  R->walk([&](Operation *op) {
    // Check for memref.load operations.
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      if (loadOp.getMemref() == v)
        hasLoad = true;
    }
    // Check for memref.store operations.
    else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      if (storeOp.getMemref() == v)
        hasStore = true;
    }
  });
  if (hasLoad && hasStore)
    return AccessType::ReadWrite;
  if (hasLoad)
    return AccessType::ReadOnly;
  if (hasStore)
    return AccessType::WriteOnly;
  return AccessType::Unknown;
}

///===----------------------------------------------------------------------===///
/// Helper: Get Values Defined Above the Regions
///
/// Collect all values used within any of the given regions that are defined
/// in an ancestor of those regions.
///===----------------------------------------------------------------------===///
static void getUsedValuesDefinedAbove(MutableArrayRef<Region> regions,
                                      llvm::SetVector<Value> &values) {
  for (Region &region : regions) {
    for (Block &block : region) {
      for (Operation &op : block) {
        for (Value operand : op.getOperands()) {
          if (!region.contains(operand.getDefiningOp()))
            values.insert(operand);
        }
        for (Region &child : op.getRegions())
          getUsedValuesDefinedAbove({&child}, values);
      }
    }
  }
}

///===----------------------------------------------------------------------===///
/// Helper: Check if a Value is Invariant in a Region
///
/// A value is invariant if it is defined outside the region and is never
/// written to within the region, or if defined inside, its definition dominates
/// all uses in the region.
///===----------------------------------------------------------------------===///
static bool isInvariantInRegion(Value v, Region *R, DominanceInfo &domInfo) {
  // If the value is defined outside, check that no store writes to it in R.
  if (!R->contains(v.getDefiningOp())) {
    bool written = false;
    R->walk([&](Operation *op) {
      if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
        if (storeOp.getMemref() == v)
          written = true;
      }
    });
    return !written;
  }
  // If defined inside, require that its defining op dominates all its uses in R.
  Operation *defOp = v.getDefiningOp();
  for (OpOperand &use : v.getUses()) {
    Operation *user = use.getOwner();
    if (!R->contains(user))
      continue;
    if (!domInfo.dominates(defOp, user))
      return false;
  }
  return true;
}

///===----------------------------------------------------------------------===///
/// Analysis of EDT Regions to Identify Candidate Datablocks
///
/// This function examines an EDT region (first region of an arts.edt op)
/// and collects candidate datablocks. A candidate datablock is any memref
/// value used in the region that is defined outside it (or remains invariant
/// inside). For invariant values, we assume a "pinned" index (here, constant 0).
/// Additionally, we classify the access pattern.
///===----------------------------------------------------------------------===///
static void analyzeEdtRegion(Operation *edtOp,
                             SmallVectorImpl<CandidateDatablock> &candidates) {
  // Get the primary region of the EDT op.
  Region &region = edtOp->getRegion(0);
  DominanceInfo domInfo(edtOp);
  llvm::SetVector<Value> externalValues;
  getUsedValuesDefinedAbove({&region}, externalValues);

  // For each value defined outside the region that has a memref type, create a candidate.
  for (Value v : externalValues) {
    if (!v.getType().isa<MemRefType>())
      continue;
    CandidateDatablock cand;
    cand.memref = v;
    /// Determine if v is invariant in the region.
    cand.invariant = isInvariantInRegion(v, &region, domInfo);
    /// If invariant, assume a constant pinned index (here, zero).
    if (cand.invariant) {
      OpBuilder builder(edtOp);
      auto c0 = builder.create<ConstantIndexOp>(edtOp->getLoc(), 0);
      cand.pinnedIndices.push_back(c0);
    }
    /// Classify the access pattern of v within the region.
    cand.access = analyzeAccessPattern(v, &region);
    candidates.push_back(cand);
  }
}

///===----------------------------------------------------------------------===///
/// Pass: ArtsDatablockIdentificationPass
///
/// This pass runs on each function and analyzes all EDT regions within the
/// function to identify candidate datablocks. It prints the candidate datablock
/// information including whether the candidate is invariant and its access type.
///===----------------------------------------------------------------------===///
struct ArtsDatablockIdentificationPass
    : public PassWrapper<ArtsDatablockIdentificationPass, OperationPass<FuncOp>> {
  void runOnOperation() override {
    FuncOp func = getOperation();
    llvm::outs() << "Analyzing candidate datablocks in function: " << func.getName() << "\n";

    SmallVector<CandidateDatablock, 8> candidates;

    // Walk over each operation in the function and analyze EDT regions.
    func.walk([&](Operation *op) {
      if (op->getName().getStringRef() == "arts.edt") {
        llvm::outs() << "Analyzing EDT region:\n";
        analyzeEdtRegion(op, candidates);
      }
    });

    llvm::outs() << "Found " << candidates.size() << " candidate datablock(s):\n";
    for (const auto &cand : candidates) {
      llvm::outs() << "Candidate memref: " << cand.memref << "\n";
      llvm::outs() << "  Invariant: " << (cand.invariant ? "yes" : "no") << "\n";
      llvm::outs() << "  Access type: ";
      switch (cand.access) {
      case AccessType::ReadOnly:
        llvm::outs() << "read-only"; break;
      case AccessType::WriteOnly:
        llvm::outs() << "write-only"; break;
      case AccessType::ReadWrite:
        llvm::outs() << "read-write"; break;
      default:
        llvm::outs() << "unknown"; break;
      }
      llvm::outs() << "\n";
      llvm::outs() << "  Pinned indices: ";
      for (Value idx : cand.pinnedIndices)
        llvm::outs() << idx << " ";
      llvm::outs() << "\n";
    }
  }
};

///===----------------------------------------------------------------------===///
/// Pass Registration
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createArtsDatablockIdentificationPass() {
  return std::make_unique<ArtsDatablockIdentificationPass>();
}
} // namespace arts
} // namespace mlir
