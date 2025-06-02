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
    : public CreateDatablocksBase<CreateDatablocksPass> {

  /// Constructor
  CreateDatablocksPass() = default;
  CreateDatablocksPass(bool identifyDbs) { this->identifyDbs = identifyDbs; }

  /// Main entry of the pass.
  void runOnOperation() override;

  /// Identify datablocks in the module and create them
  void identifyDatablocks(ModuleOp module);
  /// Create a DbCreateOp for a base datablock
  DbCreateOp createDatablockAllocation(OpBuilder &builder, Location loc,
                                       Value basePtr,
                                       DatablockAccessType access);
  /// Create a memref.subview into a created datablock
  memref::SubViewOp createDatablockSubview(OpBuilder &builder, Location loc,
                                           DbCreateOp dbCreateOp,
                                           SmallVector<Value> pinnedIndices);
  /// Rewrites datablock uses for memref.subview
  void rewireDatablockUses(memref::SubViewOp &subviewOp, EdtOp &edtOp,
                           SmallVector<Operation *> &uses);
  /// Analyze the EDT region to collect candidate datablocks.
  void analyzeEdtRegion(EdtOp &edtOp);
  /// Analyze a memref value's uses in the Edt and update candidate
  /// datablocks.
  void analyzeValueInEdt(Value dbPtr, SetVector<Value> edtInvariantValues,
                         EdtOp &edtOp);

private:
  /// Map to store candidate datablocks
  DenseMap<EdtOp, DenseMap<CandidateDatablock, SmallVector<Operation *>>>
      candidateDatablocks;

  /// Map to store created datablocks for reuse (by base pointer)
  DenseMap<Value, DbCreateOp> createdDatablocks;

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
  LLVM_DEBUG({
    dbgs() << "\n" << line << "CreateDatablocksPass FINISHED\n" << line;
    module.dump();
  });
}

void CreateDatablocksPass::identifyDatablocks(ModuleOp module) {
  /// Create a list of EDT ops ordered by post-order traversal.
  module.walk([&](func::FuncOp func) {
    LLVM_DEBUG(DBGS() << "Candidate datablocks in function: " << func.getName()
                      << "\n");
    func.walk<mlir::WalkOrder::PostOrder>([&](EdtOp edtOp) {
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

    auto edtParent = edtOp->getParentOfType<EdtOp>();

    /// Group candidates by base pointer to avoid creating duplicate datablocks
    DenseMap<
        Value,
        SmallVector<std::pair<CandidateDatablock, SmallVector<Operation *> *>>>
        candidatesByPtr;
    for (auto &candEntry : dbCandidates) {
      candidatesByPtr[candEntry.first.ptr].push_back(
          {candEntry.first, &candEntry.second});
    }

    for (auto &ptrGroup : candidatesByPtr) {
      Value basePtr = ptrGroup.first;
      auto &candidates = ptrGroup.second;

      /// Set insertion point to the current EDT op.
      OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPoint(edtOp);
      if (auto *defOp = basePtr.getDefiningOp()) {
        /// If the memref allocation is within the same parent EDT, insert after
        /// it.
        if (isa<memref::AllocaOp>(defOp) &&
            defOp->getParentOfType<EdtOp>() == edtParent)
          builder.setInsertionPointAfter(defOp);
      }

      /// Create or reuse a datablock allocation for this base pointer
      DbCreateOp dbCreateOp = nullptr;
      if (auto it = createdDatablocks.find(basePtr);
          it != createdDatablocks.end()) {
        dbCreateOp = it->second;
      } else {
        /// Determine the most permissive access type for this base pointer
        DatablockAccessType combinedAccess = DatablockAccessType::Unknown;
        for (auto &[cand, uses] : candidates) {
          if (combinedAccess == DatablockAccessType::Unknown)
            combinedAccess = cand.access;
          else if (combinedAccess != cand.access)
            combinedAccess = DatablockAccessType::ReadWrite;
        }

        dbCreateOp =
            createDatablockAllocation(builder, loc, basePtr, combinedAccess);
        createdDatablocks[basePtr] = dbCreateOp;
      }

      /// Create subview operations for each candidate
      for (auto &[dbCand, dbUses] : candidates) {
        auto subviewOp = createDatablockSubview(builder, loc, dbCreateOp,
                                                dbCand.pinnedIndices);
        rewireDatablockUses(subviewOp, edtOp, *dbUses);
        edtDeps.push_back(subviewOp.getResult());
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
      if (auto dbOp = oldDep.getDefiningOp<DbControlOp>())
        opsToRemove.insert(dbOp);
    }
    opsToRemove.insert(edtOp);

    /// Increment the EDT op counter.
    LLVM_DEBUG(DBGS() << "Created EDT #" << edtItr++ << "\n");
  }

  /// Remove the old ops.
  LLVM_DEBUG(dbgs() << "Removing old datablocks\n");
  removeOps(module, builder, opsToRemove);
}

DbCreateOp
CreateDatablocksPass::createDatablockAllocation(OpBuilder &builder,
                                                Location loc, Value basePtr,
                                                DatablockAccessType access) {
  /// Determine the mode string
  StringRef modeStr = types::toString(access);

  /// Use the utility function to create the DbCreateOp with empty sizes
  return arts::createDbCreateOp(builder, loc, modeStr, basePtr, {});
}

memref::SubViewOp
CreateDatablocksPass::createDatablockSubview(OpBuilder &builder, Location loc,
                                             DbCreateOp dbCreateOp,
                                             SmallVector<Value> pinnedIndices) {
  Value dbPtr =
      dbCreateOp.getPtr(); // Get the pointer from the created datablock
  auto dbPtrType = dbPtr.getType().cast<MemRefType>();

  int64_t rank = dbPtrType.getRank();
  const auto pinnedCount = static_cast<int64_t>(pinnedIndices.size());
  assert(pinnedCount <= rank &&
         "Pinned indices exceed the rank of the memref.");

  /// Compute offsets, sizes, and strides for the subview
  SmallVector<OpFoldResult> offsets, sizes, strides;

  for (int64_t i = 0; i < rank; ++i) {
    if (i < pinnedCount) {
      // For pinned dimensions, use the pinned index as offset and size 1
      offsets.push_back(pinnedIndices[i]);
      sizes.push_back(builder.getIndexAttr(1));
    } else {
      // For non-pinned dimensions, start from 0 and use the full dimension size
      offsets.push_back(builder.getIndexAttr(0));
      if (dbPtrType.isDynamicDim(i)) {
        sizes.push_back(
            builder.create<memref::DimOp>(loc, dbPtr, i).getResult());
      } else {
        sizes.push_back(builder.getIndexAttr(dbPtrType.getDimSize(i)));
      }
    }
    strides.push_back(builder.getIndexAttr(1));
  }

  return builder.create<memref::SubViewOp>(loc, dbPtr, offsets, sizes, strides);
}

void CreateDatablocksPass::rewireDatablockUses(memref::SubViewOp &subviewOp,
                                               EdtOp &edtOp,
                                               SmallVector<Operation *> &uses) {
  MLIRContext *ctx = subviewOp.getContext();
  auto builder = OpBuilder(ctx);

  const unsigned drop = 0; // We don't drop indices for subviews

  auto updateOp = [&](auto opToUpdate) {
    OpBuilder::InsertionGuard IG(builder);
    builder.setInsertionPoint(opToUpdate);
    auto origIndices = opToUpdate.getIndices();

    /// Create a new indices vector and remove the 'drop' leading indices
    SmallVector<Value, 4> newIndices;
    for (unsigned i = drop; i < origIndices.size(); ++i)
      newIndices.push_back(origIndices[i]);

    /// Update the memref operand and indices.
    opToUpdate.getMemrefMutable().assign(subviewOp.getResult());
    opToUpdate.getIndicesMutable().assign(newIndices);
  };

  for (Operation *op : uses) {
    /// Just rewire if the parent of type EdtOp is the same
    if (auto parentOp = op->getParentOfType<EdtOp>();
        parentOp && parentOp != edtOp)
      continue;
    /// Update the operation with the new datablock.
    if (auto loadOp = dyn_cast<memref::LoadOp>(op))
      updateOp(loadOp);
    else if (auto storeOp = dyn_cast<memref::StoreOp>(op))
      updateOp(storeOp);
  }

  /// Replace all uses of the original ptr with the subview result
  DominanceInfo domInfo(edtOp->getParentOfType<FuncOp>());
  // We would need to be more careful about replacing uses here
  // but for now, the updateOp lambda handles the main cases
}

void CreateDatablocksPass::analyzeEdtRegion(EdtOp &edtOp) {
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
    LLVM_DEBUG(dbgs() << "  - Found value: " << v << "\n");
    /// If the value is a memref, add it.
    if (v.getType().isa<MemRefType>()) {
      ptrs.push_back(v);
      continue;
    }

    /// Check if the value is invariant in the EDT region.
    if (isInvariantInEdt(region, v))
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
    Value dbPtr, SetVector<Value> edtInvariantValues, EdtOp &edtOp) {
  /// Retrieve the candidate map for the given EDT op.
  auto &candMap = candidateDatablocks[edtOp];
  auto &region = edtOp.getRegion();

  /// Process all uses of the memref value.
  for (OpOperand &use : dbPtr.getUses()) {
    Operation *op = use.getOwner();
    if (!region.isAncestor(op->getParentRegion()))
      continue;

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
        const CandidateDatablock *prefixCand = vec[i];
        const CandidateDatablock *cand = vec[j];
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
      else
        db.updateAccess(DatablockAccessType::ReadWrite);
    }
  }
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
