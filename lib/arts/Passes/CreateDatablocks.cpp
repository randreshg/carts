///==========================================================================
/// File: CreateDatablocks.cpp
//
// This pass analyzes EDT regions within a function to discover candidate
// datablocks. A candidate datablock is a memref value used in an EDT region
// that is defined outside that region. In addition, the pass classifies the
// candidate into one of three categories based on its access pattern:
// read‑only, write‑only, or read–write.
///==========================================================================

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsIR.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsTypes.h"
#include "arts/Utils/ArtsUtils.h"
/// Others
#include "arts/Analysis/StringAnalysis.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/RegionUtils.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
/// Debug
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#define DEBUG_TYPE "create-datablocks"
#define line "-----------------------------------------\n"
#define smallline "------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::arts;
using namespace mlir::arts::types;

/// CandidateDatablock: Structure representing a candidate datablock.
struct CandidateDatablock {
  /// Constructors.
  CandidateDatablock() = default;
  CandidateDatablock(Value ptr, bool isString = false)
      : ptr(ptr), isString(isString) {}

  /// Memref value.
  Value ptr = nullptr;
  /// Pinned indices (if invariant).
  SmallVector<Value> pinnedIndices;
  /// Classification: read‑only, write‑only, or read–write.
  DatablockAccessType access = DatablockAccessType::Unknown;
  /// Indicates if the datablock is a string, which is a special case.
  bool isString = false;

  /// Set Access based on read and write flags.
  void setAccess(bool hasRead, bool hasWrite) {
    if (hasRead && hasWrite)
      access = DatablockAccessType::ReadWrite;
    else if (hasWrite)
      access = DatablockAccessType::WriteOnly;
    else if (hasRead)
      access = DatablockAccessType::ReadOnly;
    else
      access = DatablockAccessType::Unknown;
  }

  /// Update Access when combining different operations.
  void updateAccess(DatablockAccessType newAccess) {
    if (access == DatablockAccessType::Unknown)
      access = newAccess;
    else if (access != newAccess)
      access = DatablockAccessType::ReadWrite;
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
struct CreateDatablocksPass
    : public arts::CreateDatablocksBase<CreateDatablocksPass> {

  /// Constructor
  CreateDatablocksPass() = default;
  CreateDatablocksPass(bool identifyDbs) { this->identifyDbs = identifyDbs; }

  /// Main entry of the pass.
  void runOnOperation() override;

  /// Identify datablocks in the module and create them
  void identifyDatablocks(ModuleOp module);
  /// Rewrites datablock uses
  void rewireDatablockUses(arts::DataBlockOp &dbOp, arts::EdtOp &edtOp,
                           SmallVector<Operation *> &uses);
  /// Analyze the EDT region to collect candidate datablocks.
  void analyzeEdtRegion(arts::EdtOp &edtOp);
  /// Analyze a memref value's uses in the Edt and update candidate
  /// datablocks.
  void analyzeValueInEdt(Value dbPtr, SetVector<Value> edtInvariantValues,
                         arts::EdtOp &edtOp);

private:
  /// Checks if a value is invariant in the region. A value is assummed
  /// invariant if it is defined outside the region and is never written to
  /// within the region, or if defined inside, its definition dominates all uses
  /// in the region.
  bool isInvariantInRegion(Value value, Region &region);

  /// Map to store candidate datablocks
  DenseMap<arts::EdtOp, DenseMap<CandidateDatablock, SmallVector<Operation *>>>
      candidateDatablocks;

  /// List of EDT ops in post-order traversal.
  SmallVector<EdtOp, 4> edtOps;

  /// String analysis
  StringAnalysis *strAnalysis;
};
} // end namespace

void CreateDatablocksPass::runOnOperation() {
  ModuleOp module = getOperation();
  strAnalysis = &getAnalysis<StringAnalysis>();
  LLVM_DEBUG({
    dbgs() << "\n" << line << "CreateDatablocksPass STARTED\n" << line;
    module.dump();
  });
  identifyDatablocks(module);
  LLVM_DEBUG(dbgs() << line << "CreateDatablocksPass FINISHED\n" << line);
}

void CreateDatablocksPass::identifyDatablocks(ModuleOp module) {
  /// Create a list of EDT ops ordered by post-order traversal.
  module.walk([&](func::FuncOp func) {
    LLVM_DEBUG(DBGS() << "Candidate datablocks in function: " << func.getName()
                      << "\n");
    func.walk<mlir::WalkOrder::PostOrder>([&](arts::EdtOp edtOp) {
      analyzeEdtRegion(edtOp);
      edtOps.push_back(edtOp);
    });
  });

  /// For each candidate datablock, create new datablocks in the EDT region.
  auto ctx = module.getContext();
  Location loc = UnknownLoc::get(ctx);
  OpBuilder builder(ctx);
  uint32_t edtItr = 0;
  llvm::SetVector<Operation *> opsToRemove;
  for (auto edtOp : edtOps) {
    auto &dbCandidates = candidateDatablocks[edtOp];
    SmallVector<Value> edtDeps;
    LLVM_DEBUG({
      dbgs() << line;
      DBGS() << "Creating " << dbCandidates.size() << " datablocks for EDT #"
             << edtItr << "\n";
    });

    auto edtParent = edtOp->getParentOfType<arts::EdtOp>();
    for (auto &candEntry : dbCandidates) {
      auto &dbCand = candEntry.first;
      auto &dbUses = candEntry.second;
      {
        /// Set insertion point to the current EDT op.
        OpBuilder::InsertionGuard guard(builder);
        builder.setInsertionPoint(edtOp);
        if (auto *defOp = dbCand.ptr.getDefiningOp()) {
          /// If the memref allocation is within the same parent EDT, insert
          /// after it.
          if (isa<memref::AllocaOp>(defOp) &&
              defOp->getParentOfType<arts::EdtOp>() == edtParent)
            builder.setInsertionPointAfter(defOp);
        }
        /// Create a new datablock operation.
        auto dbOp = createDatablockOp(builder, loc, dbCand.access, dbCand.ptr,
                                      dbCand.pinnedIndices, dbCand.isString);
        rewireDatablockUses(dbOp, edtOp, dbUses);
        edtDeps.push_back(dbOp.getResult());
      }
    }

    /// Create a new EDT op with the new dependency operands.
    {
      OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPoint(edtOp);
      /// Move the region body from the old EDT op to the new one.
      auto newEdtOp = createEdtOp(builder, loc, getEdtType(edtOp), edtDeps);
      newEdtOp.getRegion().takeBody(edtOp.getRegion());
    }

    /// Collect old dependencies to remove them later.
    for (auto oldDep : edtOp.getDependencies()) {
      opsToRemove.insert(cast<DataBlockOp>(oldDep.getDefiningOp()));
    }
    opsToRemove.insert(edtOp);

    /// Increment the EDT op counter.
    LLVM_DEBUG(DBGS() << "Created EDT #" << edtItr++ << "\n");
  }

  /// Remove the old ops.
  LLVM_DEBUG(dbgs() << "Removing old datablocks\n");
  removeOps(module, builder, opsToRemove);
}

void CreateDatablocksPass::analyzeEdtRegion(arts::EdtOp &edtOp) {
  LLVM_DEBUG({
    dbgs() << smallline;
    DBGS() << "EDT region #" << edtOps.size() << ":\n";
  });
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
    analyzeValueInEdt(ptr, invariantValues, edtOp);

  /// Debug print the candidate datablocks.
  LLVM_DEBUG({
    auto candItr = 0;
    auto &candMap = candidateDatablocks[edtOp];
    for (auto &entry : candMap) {
      auto &db = entry.first;
      auto &uses = entry.second;
      dbgs() << "  - Candidate Datablock #" << candItr++ << ":\n";
      dbgs() << "    Memref: " << db.ptr << "\n";
      dbgs() << "    IsString: " << (db.isString ? "true" : "false") << "\n";
      dbgs() << "    Access Type: " << types::toString(db.access) << "\n";
      dbgs() << "    Pinned Indices:";
      if (db.pinnedIndices.empty())
        dbgs() << "  none\n";
      else {
        for (Value idx : db.pinnedIndices)
          dbgs() << "\n      - " << idx;
        dbgs() << "\n";
      }
      dbgs() << "    Uses:\n";
      for (Operation *op : uses)
        dbgs() << "      - " << *op << "\n";
    }
  });
}

void CreateDatablocksPass::analyzeValueInEdt(
    Value dbPtr, SetVector<Value> edtInvariantValues, arts::EdtOp &edtOp) {
  /// Retrieve the candidate map for the given EDT op.
  auto &candMap = candidateDatablocks[edtOp];
  auto &region = edtOp.getRegion();

  /// Process all uses of the memref value.
  for (OpOperand &use : dbPtr.getUses()) {
    Operation *op = use.getOwner();
    if (!region.isAncestor(op->getParentRegion()))
      continue;

    /// Only consider load and store operations.
    if (!(isa<memref::LoadOp>(op) || isa<memref::StoreOp>(op))) {
      continue;
    }

    CandidateDatablock db(dbPtr, strAnalysis->isStringMemRef(dbPtr));
    SmallVector<Value> indices;
    if (auto loadOp = dyn_cast<memref::LoadOp>(op))
      indices = loadOp.getIndices();
    else if (auto storeOp = dyn_cast<memref::StoreOp>(op))
      indices = storeOp.getIndices();

    /// Append consecutive invariant (pinned) indices.
    for (Value idx : indices) {
      if (!edtInvariantValues.count(idx))
        break;
      db.pinnedIndices.push_back(idx);
    }
    candMap[db].push_back(op);
  }

  /// Group candidate datablocks by pointer
  DenseMap<Value, SmallVector<const CandidateDatablock *, 4>> candByPtr;
  for (const auto &entry : candMap)
    candByPtr[entry.first.ptr].push_back(&entry.first);

  /// For each group, sort candidates by the number of pinned indices.
  for (auto &group : candByPtr) {
    auto &vec = group.second;
    std::sort(vec.begin(), vec.end(),
              [](const CandidateDatablock *a, const CandidateDatablock *b) {
                return a->pinnedIndices.size() < b->pinnedIndices.size();
              });
  }

  /// Identify and mark for removal candidates that are supersets of
  /// candidates with a prefix of index values.
  SetVector<CandidateDatablock> toRemove;
  for (auto &group : candByPtr) {
    auto &vec = group.second;
    /// Compare each candidate with those having more pinned indices.
    for (uint32_t i = 0, n = vec.size(); i < n; ++i) {
      for (uint32_t j = i + 1; j < n; ++j) {
        const auto *prefixCand = vec[i];
        const auto *cand = vec[j];
        /// If db1 is a prefix of db2, mark db2 for removal.
        if (prefixCand->pinnedIndices.size() >= cand->pinnedIndices.size())
          continue;
        bool isPrefix = true;
        for (uint32_t k = 0, e = prefixCand->pinnedIndices.size(); k < e; ++k) {
          if (prefixCand->pinnedIndices[k] != cand->pinnedIndices[k]) {
            isPrefix = false;
            break;
          }
        }
        if (isPrefix)
          toRemove.insert(*cand);
      }
    }
  }

  /// Remove candidates that are supersets.
  for (const CandidateDatablock &db : toRemove) {
    LLVM_DEBUG(dbgs() << "Removing candidate datablock:\n");
    LLVM_DEBUG(dbgs() << "  Memref: " << db.ptr << "\n");
    candMap.erase(db);
  }

  /// Analyze each candidate's access type.
  for (auto &entry : candMap) {
    CandidateDatablock &db = entry.first;
    for (Operation *op : entry.second) {
      if (isa<memref::LoadOp>(op))
        db.updateAccess(DatablockAccessType::ReadOnly);
      else if (isa<memref::StoreOp>(op))
        db.updateAccess(DatablockAccessType::WriteOnly);
    }
  }
}

bool CreateDatablocksPass::isInvariantInRegion(Value value, Region &region) {
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

void CreateDatablocksPass::rewireDatablockUses(arts::DataBlockOp &dbOp,
                                               arts::EdtOp &edtOp,
                                               SmallVector<Operation *> &uses) {
  MLIRContext *ctx = dbOp.getContext();
  auto builder = OpBuilder(ctx);
  LLVM_DEBUG(dbgs() << "- Rewiring uses of:\n  " << dbOp << "\n");

  const unsigned drop = dbOp.getIndices().size();

  auto updateOp = [&](auto opToUpdate) {
    OpBuilder::InsertionGuard IG(builder);
    builder.setInsertionPoint(opToUpdate);
    auto origIndices = opToUpdate.getIndices();

    /// Create a new indices vector and remove the 'drop' leading indices
    SmallVector<Value, 4> newIndices;
    for (unsigned i = drop; i < origIndices.size(); ++i)
      newIndices.push_back(origIndices[i]);

    /// Update the memref operand and indices.
    opToUpdate.getMemrefMutable().assign(dbOp.getResult());
    opToUpdate.getIndicesMutable().assign(newIndices);
  };

  for (Operation *op : uses) {
    /// Just rewire if the parent of type EdtOp is the same
    if (auto parentOp = op->getParentOfType<arts::EdtOp>();
        parentOp && parentOp != edtOp)
      continue;
    /// Update the operation with the new datablock.
    if (auto loadOp = dyn_cast<memref::LoadOp>(op))
      updateOp(loadOp);
    else if (auto storeOp = dyn_cast<memref::StoreOp>(op))
      updateOp(storeOp);
  }

  /// If the operation has no offsets, replace all uses of the ptr
  /// with the new datablock except in the defining operation.
  if (drop == 0) {
    DominanceInfo domInfo(edtOp->getParentOfType<FuncOp>());
    replaceUses(dbOp.getPtr(), dbOp.getResult(), domInfo, dbOp);
  }
  LLVM_DEBUG(dbgs() << "  - OK\n");
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateDatablocksPass() {
  return std::make_unique<CreateDatablocksPass>();
}

std::unique_ptr<Pass> createCreateDatablocksPass(bool identifyDbs) {
  return std::make_unique<CreateDatablocksPass>(identifyDbs);
}
} // namespace arts
} // namespace mlir
