//===---------------------- IdentifyDatablocks.cpp ------------------------===//
//
// This pass analyzes EDT regions within a function to discover candidate
// datablocks. A candidate datablock is a memref value used in an EDT region
// that is defined outside (or remains invariant in) that region. In addition,
// the pass classifies the candidate into one of three categories based on its
// access pattern: read‑only, write‑only, or read–write.
///===----------------------------------------------------------------------===//

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Others
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
/// Debug
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#define DEBUG_TYPE "identify-datablocks"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "\n[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::arts;

/// AccessType: Classification of memref accesses.
enum class AccessType { ReadOnly, WriteOnly, ReadWrite, Unknown };

/// CandidateDatablock: Structure representing a candidate datablock.
struct CandidateDatablock {
  /// Constructors.
  CandidateDatablock() = default;
  CandidateDatablock(Value ptr) : ptr(ptr) {}

  /// Memref value.
  Value ptr = nullptr;
  /// Pinned indices (if invariant).
  SmallVector<Value, 4> pinnedIndices;
  /// Classification: read‑only, write‑only, or read–write.
  AccessType access = AccessType::Unknown;

  /// Set Access based on read and write flags.
  void setAccess(bool hasRead, bool hasWrite) {
    if (hasRead && hasWrite)
      access = AccessType::ReadWrite;
    else if (hasWrite)
      access = AccessType::WriteOnly;
    else if (hasRead)
      access = AccessType::ReadOnly;
    else
      access = AccessType::Unknown;
  }

  /// Update Access when combining different operations.
  void updateAccess(AccessType newAccess) {
    if (access == AccessType::Unknown)
      access = newAccess;
    else if (access != newAccess)
      access = AccessType::ReadWrite;
  }
};

///===----------------------------------------------------------------------===///
/// Implementation of DenseMap for CandidateDatablock.
///===----------------------------------------------------------------------===///
namespace llvm {
template <> struct DenseMapInfo<CandidateDatablock> {
  static CandidateDatablock getEmptyKey() { return {}; }

  static CandidateDatablock getTombstoneKey() { return {nullptr}; }

  static unsigned getHashValue(const CandidateDatablock &cand) {
    std::size_t hash =
        llvm::hash_value(static_cast<const void *>(cand.ptr.getImpl()));
    for (auto idx : cand.pinnedIndices)
      hash = llvm::hash_combine(hash, static_cast<const void *>(idx.getImpl()));
    return static_cast<unsigned>(hash);
  }

  static bool isEqual(const CandidateDatablock &lhs,
                      const CandidateDatablock &rhs) {
    if (lhs.ptr != rhs.ptr)
      return false;
    if (lhs.pinnedIndices.size() != rhs.pinnedIndices.size())
      return false;
    for (unsigned i = 0, e = lhs.pinnedIndices.size(); i < e; ++i)
      if (lhs.pinnedIndices[i] != rhs.pinnedIndices[i])
        return false;
    return true;
  }
};
} // namespace llvm

///===----------------------------------------------------------------------===///
/// Pass Implementation
///===----------------------------------------------------------------------===///
namespace {
struct IdentifyDatablocksPass
    : public arts::IdentifyDatablocksBase<IdentifyDatablocksPass> {
  /// Main entry of the pass.
  void runOnOperation() override;

  /// Identify datablocks in a given EDT op.
  void identifyDatablocks(arts::EdtOp edtOp);
  /// Analyze the EDT region to collect candidate datablocks.
  void analyzeEdtRegion(arts::EdtOp edtOp);
  /// Analyze a memref value's uses in the region and update candidate
  /// datablocks.
  void analyzeValue(Value dbPtr, Region &region,
                    SetVector<Value> &invariantValues, arts::EdtOp edtOp);

private:
  /// Checks if a value is invariant in the region. A value is assummed
  /// invariant if it is defined outside the region and is never written to
  /// within the region, or if defined inside, its definition dominates all uses
  /// in the region.
  bool isInvariantInRegion(Value value, Region &region);

  /// Map to store candidate datablocks
  DenseMap<arts::EdtOp, DenseMap<CandidateDatablock, SmallVector<Operation *>>>
      candidateDatablocks;
};
} // end namespace

void IdentifyDatablocksPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG(dbgs() << line << "IdentifyDatablocksPass STARTED\n" << line);

  module.walk([&](func::FuncOp func) {
    LLVM_DEBUG(DBGS() << "Analyzing candidate datablocks in function: "
                      << func.getName() << "\n");
    func.walk<mlir::WalkOrder::PostOrder>(
        [&](arts::EdtOp edt) { identifyDatablocks(edt); });
  });

  LLVM_DEBUG(dbgs() << line << "IdentifyDatablocksPass FINISHED\n" << line);
}

void IdentifyDatablocksPass::identifyDatablocks(arts::EdtOp edtOp) {
  analyzeEdtRegion(edtOp);
}

void IdentifyDatablocksPass::analyzeEdtRegion(arts::EdtOp edtOp) {
  LLVM_DEBUG(DBGS() << "EDT region\n" << edtOp << "\n");
  /// Get the primary region of the EDT op.
  Region &region = edtOp.getRegion();
  SetVector<Value> externalValues;
  getUsedValuesDefinedAbove(region, externalValues);

  /// Separate memref pointers and invariant values.
  SmallVector<Value, 4> ptrs;
  SetVector<Value> invariantValues;
  for (Value v : externalValues) {
    /// If the value is a memref, add it.
    if (v.getType().isa<MemRefType>()) {
      ptrs.push_back(v);
      continue;
    }
    /// If the value is constant or invariant, add to invariantValues.
    if (isValueConstant(v) || isInvariantInRegion(v, region))
      invariantValues.insert(v);
  }

  /// Process each memref value within the region.
  for (Value ptr : ptrs)
    analyzeValue(ptr, region, invariantValues, edtOp);
}

void IdentifyDatablocksPass::analyzeValue(Value dbPtr, Region &region,
                                          SetVector<Value> &invariantValues,
                                          arts::EdtOp edtOp) {
  /// Retrieve the candidate map for the given EDT op.
  auto &candMap = candidateDatablocks[edtOp];

  /// Iterate over all uses of the memref value.
  for (OpOperand &use : dbPtr.getUses()) {
    Operation *op = use.getOwner();
    if (!region.isAncestor(op->getParentRegion()))
      continue;

    /// Only consider load and store operations.
    if (!isa<memref::LoadOp>(op) && !isa<memref::StoreOp>(op))
      continue;

    CandidateDatablock db(dbPtr);
    SmallVector<Value, 4> indices;
    if (auto loadOp = dyn_cast<memref::LoadOp>(op))
      indices = loadOp.getIndices();
    else if (auto storeOp = dyn_cast<memref::StoreOp>(op))
      indices = storeOp.getIndices();

    /// Append consecutive invariant (pinned) indices.
    for (Value idx : indices) {
      if (!invariantValues.count(idx))
        break;
      db.pinnedIndices.push_back(idx);
    }

    /// Insert or update the candidate database entry.
    candMap[db].push_back(op);
  }

  /// Identify candidate datablocks that are subsets of others.
  SetVector<CandidateDatablock> toRemove;
  for (const auto &entry1 : candMap) {
    const CandidateDatablock &db1 = entry1.first;
    for (const auto &entry2 : candMap) {
      const CandidateDatablock &db2 = entry2.first;
      if (db1.ptr != db2.ptr)
        continue;
      if (db1.pinnedIndices.size() >= db2.pinnedIndices.size())
        continue;
      bool isPrefix = true;
      for (size_t i = 0, e = db1.pinnedIndices.size(); i < e; ++i) {
        if (db1.pinnedIndices[i] != db2.pinnedIndices[i]) {
          isPrefix = false;
          break;
        }
      }
      if (isPrefix)
        toRemove.insert(db2);
    }
  }

  /// Remove subset candidates.
  for (const CandidateDatablock &db : toRemove)
    candMap.erase(db);

  /// Analyze each candidate's access type.
  for (auto &entry : candMap) {
    CandidateDatablock &db = const_cast<CandidateDatablock &>(entry.first);
    for (Operation *op : entry.second) {
      if (isa<memref::LoadOp>(op))
        db.updateAccess(AccessType::ReadOnly);
      else if (isa<memref::StoreOp>(op))
        db.updateAccess(AccessType::WriteOnly);
    }
    /// Debug print candidate datablock.
    LLVM_DEBUG({
      dbgs() << "\n- Candidate Datablock\n";
      dbgs() << "  Memref: " << db.ptr << "\n";
      dbgs() << "  Access Type: ";
      switch (db.access) {
      case AccessType::ReadOnly:
        dbgs() << "read-only";
        break;
      case AccessType::WriteOnly:
        dbgs() << "write-only";
        break;
      case AccessType::ReadWrite:
        dbgs() << "read-write";
        break;
      default:
        dbgs() << "unknown";
        break;
      }
      dbgs() << "\n  Pinned Indices:";
      if (db.pinnedIndices.empty())
        dbgs() << " none\n";
      else {
        for (Value idx : db.pinnedIndices)
          dbgs() << "\n    - " << idx;
        dbgs() << "\n";
      }
    });
  }
}

bool IdentifyDatablocksPass::isInvariantInRegion(Value value, Region &region) {
  /// If the value is defined in the region, ignore it.
  if (region.isAncestor(value.getParentRegion()))
    return false;

  /// Iterate over each user of the value.
  for (Operation *user : value.getUsers()) {
    /// Consider only users inside the region.
    if (!region.isAncestor(user->getParentRegion()))
      continue;
    /// If the user is a store op writing to the memref, the value is not
    /// invariant.
    if (auto storeOp = dyn_cast<memref::StoreOp>(user))
      if (storeOp.getMemRef() == value)
        return false;
  }
  return true;
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createIdentifyDatablocksPass() {
  return std::make_unique<IdentifyDatablocksPass>();
}
} // namespace arts
} // namespace mlir
