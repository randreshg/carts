///==========================================================================
/// File: CreateDbs.cpp
//
// This pass analyzes EDT regions within a function to discover candidate
// datablocks. A candidate DB is a memref value used in an EDT region
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
#define DEBUG_TYPE "create-dbs"
#define line "-----------------------------------------\n"
#define smallline "------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::arts;
using namespace mlir::arts::types;

///  DbCandidate: Structure representing a candidate db.
struct DbCandidate {
  /// Constructors.
  DbCandidate() = default;
  DbCandidate(Value ptr, bool isString = false)
      : ptr(ptr), isString(isString) {}

  /// Memref value.
  Value ptr = nullptr;
  /// Pinned indices (if invariant).
  SmallVector<Value> pinnedIndices;
  /// Classification: read‑only, write‑only, or read–write.
  DbAccessType access = DbAccessType::Unknown;
  /// Indicates if the db is a string, which is a special case.
  bool isString = false;

  /// Set Access based on read and write flags.
  void setAccess(bool hasRead, bool hasWrite) {
    if (hasRead && hasWrite)
      access = DbAccessType::ReadWrite;
    else if (hasWrite)
      access = DbAccessType::WriteOnly;
    else if (hasRead)
      access = DbAccessType::ReadOnly;
    else
      access = DbAccessType::Unknown;
  }

  /// Update Access when combining different operations.
  void updateAccess(DbAccessType newAccess) {
    if (access == DbAccessType::Unknown)
      access = newAccess;
    else if (access != newAccess)
      access = DbAccessType::ReadWrite;
  }
};

///===----------------------------------------------------------------------===///
/// Implementation of DenseMap for  DbCandidate.
///===----------------------------------------------------------------------===///
namespace llvm {
template <> struct DenseMapInfo<DbCandidate> {
  static DbCandidate getEmptyKey() { return {}; }

  static DbCandidate getTombstoneKey() { return {nullptr}; }

  static unsigned getHashValue(const DbCandidate &cand) {
    std::size_t hash =
        llvm::hash_value(static_cast<const void *>(cand.ptr.getImpl()));
    for (auto idx : cand.pinnedIndices)
      hash = llvm::hash_combine(hash, static_cast<const void *>(idx.getImpl()));
    return static_cast<unsigned>(hash);
  }

  static bool isEqual(const DbCandidate &lhs, const DbCandidate &rhs) {
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
struct CreateDbsPass : public CreateDbsBase<CreateDbsPass> {

  /// Constructor
  CreateDbsPass() = default;
  CreateDbsPass(bool identifyDbs) { this->identifyDbs = identifyDbs; }

  /// Main entry of the pass.
  void runOnOperation() override;

  /// Identify datablocks in the module and create them
  void identifyDbsInModule(ModuleOp module);

  /// Create a DbAccessOp into a created db
  DbAccessOp createDbAccess(OpBuilder &builder, Location loc,
                            DbAllocOp dbAllocOp,
                            SmallVector<Value> pinnedIndices);

  /// Create a DbAllocOp for a given base pointer
  DbAllocOp createDbAlloc(OpBuilder &builder, Location loc, Value basePtr,
                          types::DbAccessType access);

  /// Analyze the EDT region to collect candidate datablocks.
  void analyzeEdtRegion(EdtOp &edtOp);

  /// Analyze a memref value's uses in the Edt and update candidate
  /// datablocks.
  void analyzeValueInEdt(Value dbPtr, SetVector<Value> edtInvariantValues,
                         EdtOp &edtOp);

private:
  /// Map to store candidate datablocks
  DenseMap<EdtOp, DenseMap<DbCandidate, SmallVector<Operation *>>> DbCandidates;

  /// Map to store created datablocks for reuse (by base pointer)
  DenseMap<Value, DbAllocOp> createdDbs;

  /// List of EDT ops in post-order traversal.
  SmallVector<EdtOp, 4> edtOps;

  /// String analysis
  StringAnalysis *strAnalysis;
};
} // end namespace

void CreateDbsPass::runOnOperation() {
  ModuleOp module = getOperation();
  strAnalysis = &getAnalysis<StringAnalysis>();
  LLVM_DEBUG({
    dbgs() << "\n" << line << "CreateDbsPass STARTED\n" << line;
    module.dump();
  });
  identifyDbsInModule(module);
  LLVM_DEBUG({
    dbgs() << "\n" << line << "CreateDbsPass FINISHED\n" << line;
    module.dump();
  });
}

void CreateDbsPass::identifyDbsInModule(ModuleOp module) {
  /// Create a list of EDT ops ordered by post-order traversal.
  module.walk([&](func::FuncOp func) {
    LLVM_DEBUG(DBGS() << "Candidate datablocks in function: " << func.getName()
                      << "\n");
    func.walk<mlir::WalkOrder::PostOrder>([&](EdtOp edtOp) {
      analyzeEdtRegion(edtOp);
      edtOps.push_back(edtOp);
    });
  });

  /// For each candidate db, create new datablocks in the EDT region.
  const auto ctx = module.getContext();
  Location loc = UnknownLoc::get(ctx);
  OpBuilder builder(ctx);
  uint32_t edtItr = 0;
  llvm::SetVector<Operation *> opsToRemove;

  for (auto edtOp : edtOps) {
    auto &dbCandidates = DbCandidates[edtOp];
    SmallVector<Value> edtDeps;
    LLVM_DEBUG({
      dbgs() << line;
      DBGS() << "Creating " << dbCandidates.size() << " datablocks for EDT #"
             << edtItr << "\n";
    });

    struct DepInfo {
      SmallVector<Operation *> uses;
      unsigned indicesToDrop;
    };
    SmallVector<DepInfo> depInfos;

    /// Group candidates by base pointer to avoid creating duplicate datablocks
    DenseMap<Value,
             SmallVector<std::pair<DbCandidate, SmallVector<Operation *> *>>>
        candidatesByPtr;
    for (auto &candEntry : dbCandidates) {
      candidatesByPtr[candEntry.first.ptr].push_back(
          {candEntry.first, &candEntry.second});
    }

    for (auto &ptrGroup : candidatesByPtr) {
      Value basePtr = ptrGroup.first;
      auto &candidates = ptrGroup.second;

      /// Create or reuse a db allocation for this base pointer
      DbAllocOp dbAllocOp = nullptr;
      if (auto it = createdDbs.find(basePtr); it != createdDbs.end()) {
        dbAllocOp = it->second;
      } else {
        OpBuilder::InsertionGuard IG(builder);
        if (auto *defOp = basePtr.getDefiningOp())
          builder.setInsertionPointAfter(defOp);
        else if (auto arg = basePtr.dyn_cast<BlockArgument>())
          builder.setInsertionPointToStart(arg.getOwner());
        else
          llvm_unreachable("Support for global variables is not implemented");

        /// Determine the access type for this base pointer
        DbAccessType combinedAccess = DbAccessType::Unknown;
        for (auto &[cand, uses] : candidates) {
          if (combinedAccess == DbAccessType::Unknown)
            combinedAccess = cand.access;
          else if (combinedAccess != cand.access)
            combinedAccess = DbAccessType::ReadWrite;
        }

        dbAllocOp = createDbAlloc(builder, loc, basePtr, combinedAccess);
        createdDbs[basePtr] = dbAllocOp;
      }

      /// Create DbAccessOps for each candidate
      OpBuilder::InsertionGuard IG(builder);
      builder.setInsertionPoint(edtOp);
      for (auto &[dbCand, dbUses] : candidates) {
        auto dbAccessOp =
            createDbAccess(builder, loc, dbAllocOp, dbCand.pinnedIndices);
        depInfos.push_back({*dbUses, (unsigned)dbCand.pinnedIndices.size()});
        edtDeps.push_back(dbAccessOp.getResult());
      }
    }

    /// Create a new EDT op with the new dependency operands.
    {
      OpBuilder::InsertionGuard IG(builder);
      builder.setInsertionPoint(edtOp);
      /// Move the region body from the old EDT op to the new one.
      auto newEdtOp = createEdtOp(builder, loc, getEdtType(edtOp), edtDeps);
      Region &newRegion = newEdtOp.getRegion();

      /// The new EdtOp might be created with a region with one or more blocks.
      /// We are going to erase them before moving the body of the old op.
      newRegion.getBlocks().clear();

      /// Move the body from the old EDT.
      newRegion.takeBody(edtOp.getRegion());

      /// Rewire uses of the old datablocks directly with the DbAccessOp
      /// results.
      LLVM_DEBUG(DBGS() << "New EDT has " << edtDeps.size() << " dependencies, "
                        << depInfos.size() << " depInfos\n");

      for (unsigned i = 0; i < depInfos.size() && i < edtDeps.size(); ++i) {
        auto &info = depInfos[i];
        Value dbAccessResult = edtDeps[i];

        auto updateOp = [&](auto opToUpdate, unsigned drop) {
          OpBuilder::InsertionGuard IG(builder);
          builder.setInsertionPoint(opToUpdate);
          auto origIndices = opToUpdate.getIndices();

          SmallVector<Value, 4> newIndices;
          for (unsigned j = drop; j < origIndices.size(); ++j)
            newIndices.push_back(origIndices[j]);

          opToUpdate.getMemrefMutable().assign(dbAccessResult);
          opToUpdate.getIndicesMutable().assign(newIndices);
        };

        for (Operation *op : info.uses) {
          if (auto loadOp = dyn_cast<memref::LoadOp>(op))
            updateOp(loadOp, info.indicesToDrop);
          else if (auto storeOp = dyn_cast<memref::StoreOp>(op))
            updateOp(storeOp, info.indicesToDrop);
        }
      }
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

  for (auto &entry : createdDbs) {
    Value basePtr = entry.first;
    DbAllocOp dbAllocOp = entry.second;
    basePtr.replaceUsesWithIf(dbAllocOp.getPtr(), [&](OpOperand &use) {
      return use.getOwner() != dbAllocOp;
    });
  }
}

DbAccessOp CreateDbsPass::createDbAccess(OpBuilder &builder, Location loc,
                                         DbAllocOp dbAllocOp,
                                         SmallVector<Value> pinnedIndices) {
  /// Get the pointer from the created db
  Value dbPtr = dbAllocOp.getPtr();
  auto dbPtrType = dbPtr.getType().cast<MemRefType>();

  int64_t rank = dbPtrType.getRank();
  const auto pinnedCount = static_cast<int64_t>(pinnedIndices.size());
  assert(pinnedCount <= rank &&
         "Pinned indices exceed the rank of the memref.");

  /// Compute offsets, sizes, and strides for the subview
  SmallVector<OpFoldResult> offsets, sizes, strides;

  for (int64_t i = 0; i < rank; ++i) {
    if (i < pinnedCount) {
      /// For pinned dimensions, use the pinned index as offset and size 1
      offsets.push_back(pinnedIndices[i]);
      sizes.push_back(builder.getIndexAttr(1));
    } else {
      /// For non-pinned dimensions, start from 0 and use the full dimension
      /// size
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

  auto subviewOp =
      builder.create<memref::SubViewOp>(loc, dbPtr, offsets, sizes, strides);
  return DbAccessOp(subviewOp);
}

DbAllocOp CreateDbsPass::createDbAlloc(OpBuilder &builder, Location loc,
                                       Value basePtr,
                                       types::DbAccessType access) {
  /// Create the DbAllocOp using the existing function to ensure sizes are
  /// filled out
  auto dbAllocOp =
      createDbAllocOp(builder, loc, types::toString(access), basePtr);

  /// Set the allocation type attribute using the helper function
  auto allocType = arts::getAllocType(basePtr);
  setDbAllocType(dbAllocOp, allocType, builder);

  LLVM_DEBUG(DBGS() << "Created DbAllocOp with allocation type: "
                    << types::toString(allocType) << "\n");
  return dbAllocOp;
}

void CreateDbsPass::analyzeEdtRegion(EdtOp &edtOp) {
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
    /// If the value is a memref, add it to the list of pointers.
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
    auto &candMap = DbCandidates[edtOp];
    for (auto &entry : candMap) {
      auto &db = entry.first;
      auto &uses = entry.second;
      dbgs() << "  - Candidate Db #" << candItr++ << ":\n"
             << "    Memref: " << db.ptr << "\n"
             << "    IsString: " << (db.isString ? "true" : "false") << "\n"
             << "    Access Type: " << types::toString(db.access) << "\n"
             << "    Pinned Indices:";
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

void CreateDbsPass::analyzeValueInEdt(Value dbPtr,
                                      SetVector<Value> edtInvariantValues,
                                      EdtOp &edtOp) {
  /// Retrieve the candidate map for the given EDT op.
  auto &candMap = DbCandidates[edtOp];
  auto &region = edtOp.getRegion();

  /// Process all uses of the memref value.
  for (OpOperand &use : dbPtr.getUses()) {
    Operation *op = use.getOwner();
    if (!region.isAncestor(op->getParentRegion()))
      continue;

    if (auto userEdt = op->getParentOfType<EdtOp>())
      if (userEdt != edtOp)
        continue;

    DbCandidate db(dbPtr, strAnalysis->isStringMemRef(dbPtr));
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
  DenseMap<Value, SmallVector<const DbCandidate *, 4>> candByPtr;
  for (const auto &entry : candMap)
    candByPtr[entry.first.ptr].push_back(&entry.first);

  /// For each group, sort candidates by the number of pinned indices.
  for (auto &group : candByPtr) {
    auto &vec = group.second;
    std::sort(vec.begin(), vec.end(),
              [](const DbCandidate *a, const DbCandidate *b) {
                return a->pinnedIndices.size() < b->pinnedIndices.size();
              });
  }

  /// Identify and mark for removal candidates that are supersets of
  /// candidates with a prefix of index values.
  SetVector<DbCandidate> toRemove;
  for (auto &group : candByPtr) {
    auto &vec = group.second;
    /// Compare each candidate with those having more pinned indices.
    for (uint32_t i = 0, n = vec.size(); i < n; ++i) {
      for (uint32_t j = i + 1; j < n; ++j) {
        const DbCandidate *prefixCand = vec[i];
        const DbCandidate *cand = vec[j];
        /// If DB1 is a prefix of DB2, mark DB2 for removal.
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
  for (const DbCandidate &db : toRemove) {
    LLVM_DEBUG(dbgs() << "Removing candidate db:\n"
                      << "  Memref: " << db.ptr << "\n");
    candMap.erase(db);
  }

  /// Analyze each candidate's access type.
  for (auto &entry : candMap) {
    DbCandidate &db = entry.first;
    for (Operation *op : entry.second) {
      if (isa<memref::LoadOp>(op))
        db.updateAccess(DbAccessType::ReadOnly);
      else if (isa<memref::StoreOp>(op))
        db.updateAccess(DbAccessType::WriteOnly);
      else
        db.updateAccess(DbAccessType::ReadWrite);
    }
  }
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateDbsPass() {
  return std::make_unique<CreateDbsPass>();
}

std::unique_ptr<Pass> createCreateDbsPass(bool identifyDbs) {
  return std::make_unique<CreateDbsPass>(identifyDbs);
}
} // namespace arts
} // namespace mlir
