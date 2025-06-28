///==========================================================================
/// File: CreateDbs.cpp
//
// This pass analyzes EDT regions within a function to discover candidate
// datablocks. A candidate DB is a memref value used in an EDT region
// that is defined outside that region. In addition, the pass classifies the
// candidate into one of three categories based on its dep pattern:
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
#include <algorithm>
#include <cstdint>
#include <unordered_set>

#define DEBUG_TYPE "create-dbs"
#define LINE "-----------------------------------------\n"
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
  DbDepType dep = DbDepType::Unknown;
  /// Indicates if the db is a string, which is a special case.
  bool isString = false;

  /// Set Dep based on read and write flags.
  void setDep(bool hasRead, bool hasWrite) {
    if (hasRead && hasWrite)
      dep = DbDepType::ReadWrite;
    else if (hasWrite)
      dep = DbDepType::WriteOnly;
    else if (hasRead)
      dep = DbDepType::ReadOnly;
    else
      dep = DbDepType::Unknown;
  }

  /// Update Dep when combining different operations.
  void updateDep(DbDepType newDep) {
    if (dep == DbDepType::Unknown)
      dep = newDep;
    else if (dep != newDep)
      dep = DbDepType::ReadWrite;
  }

  /// Validate the candidate
  bool isValid() const {
    if (!ptr || !ptr.getType().isa<MemRefType>())
      return false;

    auto memrefType = ptr.getType().cast<MemRefType>();
    return pinnedIndices.size() <= static_cast<size_t>(memrefType.getRank());
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

private:
  /// Main analysis and processing methods
  void processModule(ModuleOp module);
  void collectEdtOps(ModuleOp module);
  void processAllEdtOps(ModuleOp module);

  /// Analysis methods
  void analyzeEdtRegion(EdtOp &edtOp);
  void analyzeMemRefUsage(Value dbPtr,
                          const SetVector<Value> &edtInvariantValues,
                          EdtOp &edtOp);
  SetVector<Value> getEdtInvariantValues(EdtOp &edtOp);
  void optimizeCandidates(EdtOp &edtOp);

  /// Validation methods
  bool isValidMemRef(Value ptr) const;
  bool isValidCandidate(const DbCandidate &candidate) const;
  bool isMemRefAccessValid(Value ptr, EdtOp edtOp);

  /// Datablock creation methods
  void createDbs(EdtOp edtOp, OpBuilder &builder, Location loc);
  DbAllocOp getOrCreateDbAlloc(Value basePtr, DbDepType depType,
                               OpBuilder &builder, Location loc);
  SmallVector<Value> createDbDeps(EdtOp edtOp, OpBuilder &builder,
                                  Location loc);
  DbDepOp createDbDep(OpBuilder &builder, Location loc, DbAllocOp dbAllocOp,
                      const SmallVector<Value> &pinnedIndices,
                      DbDepType depType);
  DbAllocOp createDbAlloc(OpBuilder &builder, Location loc, Value basePtr,
                          DbDepType dep);

  /// EDT operation methods
  EdtOp createNewEdtOp(EdtOp oldEdtOp, const SmallVector<Value> &edtDeps,
                       OpBuilder &builder, Location loc);
  void updateOpsInNewEdt(EdtOp oldEdtOp, EdtOp newEdtOp,
                         const SmallVector<Value> &edtDeps);
  void replaceMemRefOps(const SmallVector<Operation *> &operations,
                        Value newMemRef, unsigned indicesToDrop);

  /// Cleanup methods
  void cleanupOldOps(ModuleOp module, OpBuilder &builder);
  void pruneUnusedDbAllocs(OpBuilder &builder);
  void collectOpsToRemove(EdtOp edtOp, SetVector<Operation *> &opsToRemove);

  /// Helper methods
  using OpsPerCandidate = std::pair<DbCandidate, SmallVector<Operation *> *>;
  DbDepType combineDepTypes(const SmallVector<OpsPerCandidate> &candidates);
  void logCandidateInfo(EdtOp edtOp) const;
  void logDbCreation(EdtOp edtOp, size_t dbCount) const;

  /// Data members
  using CandidateMap = DenseMap<DbCandidate, SmallVector<Operation *>>;
  DenseMap<EdtOp, CandidateMap> DbCandidates;
  DenseMap<Value, DbAllocOp> createdDbs;
  SmallVector<EdtOp, 4> edtOps;
  SetVector<Operation *> opsToRemove;
  StringAnalysis *strAnalysis = nullptr;

  /// Caching for performance
  DenseMap<Value, bool> memrefValidityCache;
  DenseMap<EdtOp, SetVector<Value>> edtInvariantCache;
};
} // namespace

void CreateDbsPass::runOnOperation() {
  ModuleOp module = getOperation();
  strAnalysis = &getAnalysis<StringAnalysis>();

  LLVM_DEBUG({
    dbgs() << "\n" << LINE << "CreateDbsPass STARTED\n" << LINE;
    module.dump();
  });

  processModule(module);

  LLVM_DEBUG({
    dbgs() << "\n" << LINE << "CreateDbsPass FINISHED\n" << LINE;
    module.dump();
  });
}

void CreateDbsPass::processModule(ModuleOp module) {
  /// Step 1: Collect all EDT operations in post-order
  collectEdtOps(module);

  /// Step 2: Analyze each EDT region to identify datablock candidates
  for (auto edtOp : edtOps) {
    analyzeEdtRegion(edtOp);
  }

  /// Step 3: Process all EDT operations to create datablocks
  processAllEdtOps(module);

  /// Step 4: Cleanup old operations
  OpBuilder builder(module.getContext());
  cleanupOldOps(module, builder);
  pruneUnusedDbAllocs(builder);
}

void CreateDbsPass::collectEdtOps(ModuleOp module) {
  edtOps.clear();
  module.walk([&](func::FuncOp func) {
    LLVM_DEBUG(DBGS() << "Processing function: " << func.getName() << "\n");
    func.walk<mlir::WalkOrder::PostOrder>(
        [&](EdtOp edtOp) { edtOps.push_back(edtOp); });
  });

  LLVM_DEBUG(DBGS() << "Found " << edtOps.size() << " EDT operations\n");
}

void CreateDbsPass::processAllEdtOps(ModuleOp module) {
  const auto ctx = module.getContext();
  Location loc = UnknownLoc::get(ctx);
  OpBuilder builder(ctx);

  uint32_t edtCounter = 0;
  for (auto edtOp : edtOps) {
    logDbCreation(edtOp, DbCandidates[edtOp].size());
    createDbs(edtOp, builder, loc);
    LLVM_DEBUG(DBGS() << "Processed EDT #" << edtCounter++ << "\n");
  }
}

bool CreateDbsPass::isValidMemRef(Value ptr) const {
  if (!ptr || !ptr.getType().isa<MemRefType>()) {
    return false;
  }

  auto memrefType = ptr.getType().cast<MemRefType>();
  /// Skip complex-typed memrefs as they're not supported
  if (memrefType.getElementType().isa<mlir::ComplexType>()) {
    LLVM_DEBUG(DBGS() << "Skipping complex-typed memref: " << ptr << "\n");
    return false;
  }

  return true;
}

bool CreateDbsPass::isValidCandidate(const DbCandidate &candidate) const {
  return candidate.isValid() && isValidMemRef(candidate.ptr);
}

bool CreateDbsPass::isMemRefAccessValid(Value ptr, EdtOp edtOp) {
  /// Cache validation results for performance
  if (auto it = memrefValidityCache.find(ptr);
      it != memrefValidityCache.end()) {
    return it->second;
  }

  bool isValid = isValidMemRef(ptr);
  memrefValidityCache[ptr] = isValid;
  return isValid;
}

SetVector<Value> CreateDbsPass::getEdtInvariantValues(EdtOp &edtOp) {
  /// Check cache first
  if (auto it = edtInvariantCache.find(edtOp); it != edtInvariantCache.end())
    return it->second;

  SetVector<Value> externalValues;
  Region &region = edtOp.getRegion();
  getUsedValuesDefinedAbove(region, externalValues);

  /// Filter to keep only memrefs and invariant values
  SetVector<Value> result;
  for (Value v : externalValues) {
    if (v.getType().isa<MemRefType>() || isInvariantInEdt(region, v)) {
      result.insert(v);
    }
  }

  /// Cache the result
  edtInvariantCache[edtOp] = result;
  return result;
}

void CreateDbsPass::analyzeEdtRegion(EdtOp &edtOp) {
  LLVM_DEBUG({
    dbgs() << smallline;
    DBGS() << "Analyzing EDT region #" << edtOps.size() << "\n";
  });

  SetVector<Value> externalValues = getEdtInvariantValues(edtOp);

  /// Separate memref pointers and invariant values
  SmallVector<Value, 4> memrefPtrs;
  SetVector<Value> invariantValues;

  for (Value v : externalValues) {
    if (v.getType().isa<MemRefType>()) {
      if (isMemRefAccessValid(v, edtOp))
        memrefPtrs.push_back(v);
    } else {
      invariantValues.insert(v);
    }
  }

  /// Analyze each memref for datablock candidacy
  for (Value ptr : memrefPtrs)
    analyzeMemRefUsage(ptr, invariantValues, edtOp);

  /// Optimize candidates by removing redundant ones
  optimizeCandidates(edtOp);

  /// Log the results
  logCandidateInfo(edtOp);
}

void CreateDbsPass::analyzeMemRefUsage(
    Value dbPtr, const SetVector<Value> &edtInvariantValues, EdtOp &edtOp) {
  if (!isValidMemRef(dbPtr)) {
    LLVM_DEBUG(DBGS() << "Invalid memref value in analyzeMemRefUsage\n");
    return;
  }

  auto &candMap = DbCandidates[edtOp];
  auto &region = edtOp.getRegion();

  /// Process all uses of the memref value
  for (OpOperand &use : dbPtr.getUses()) {
    Operation *op = use.getOwner();

    /// Skip operations not in this EDT region
    if (!region.isAncestor(op->getParentRegion()))
      continue;

    /// Skip operations in nested EDT regions
    if (auto userEdt = op->getParentOfType<EdtOp>(); userEdt != edtOp)
      continue;

    /// Create candidate with string detection
    DbCandidate candidate(dbPtr, strAnalysis->isStringMemRef(dbPtr));
    SmallVector<Value> indices;

    /// Extract indices from load/store operations
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      indices = loadOp.getIndices();
      candidate.updateDep(DbDepType::ReadOnly);
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      indices = storeOp.getIndices();
      candidate.updateDep(DbDepType::WriteOnly);
    } else {
      candidate.updateDep(DbDepType::ReadWrite);
    }

    /// Find consecutive invariant (pinned) indices
    for (Value idx : indices) {
      if (!edtInvariantValues.count(idx))
        break;
      candidate.pinnedIndices.push_back(idx);
    }

    /// Add operation to candidate's usage list
    candMap[candidate].push_back(op);
  }
}

void CreateDbsPass::optimizeCandidates(EdtOp &edtOp) {
  auto &candMap = DbCandidates[edtOp];

  /// Group candidates by base pointer
  DenseMap<Value, SmallVector<const DbCandidate *, 4>> candidatesByPtr;
  for (const auto &entry : candMap)
    candidatesByPtr[entry.first.ptr].push_back(&entry.first);

  /// Sort candidates by number of pinned indices
  for (auto &group : candidatesByPtr) {
    auto &candidates = group.second;
    std::sort(candidates.begin(), candidates.end(),
              [](const DbCandidate *a, const DbCandidate *b) {
                return a->pinnedIndices.size() < b->pinnedIndices.size();
              });
  }

  /// Remove candidates that are supersets of others
  SetVector<DbCandidate> candidatesToRemove;
  for (auto &group : candidatesByPtr) {
    auto &candidates = group.second;

    for (size_t i = 0; i < candidates.size(); ++i) {
      for (size_t j = i + 1; j < candidates.size(); ++j) {
        const DbCandidate *prefix = candidates[i];
        const DbCandidate *superset = candidates[j];

        /// Check if prefix is actually a prefix of superset
        if (prefix->pinnedIndices.size() >= superset->pinnedIndices.size())
          continue;

        bool isPrefix = true;
        for (size_t k = 0; k < prefix->pinnedIndices.size(); ++k) {
          if (prefix->pinnedIndices[k] != superset->pinnedIndices[k]) {
            isPrefix = false;
            break;
          }
        }

        if (isPrefix) {
          candidatesToRemove.insert(*superset);
          LLVM_DEBUG(DBGS()
                     << "Removing superset candidate with "
                     << superset->pinnedIndices.size() << " pinned indices\n");
        }
      }
    }
  }

  /// Remove the superset candidates
  for (const DbCandidate &candidate : candidatesToRemove)
    candMap.erase(candidate);
}

void CreateDbsPass::createDbs(EdtOp edtOp, OpBuilder &builder, Location loc) {
  auto &candidateMap = DbCandidates[edtOp];
  if (candidateMap.empty()) {
    return;
  }

  /// Create dependencies for this EDT
  SmallVector<Value> edtDeps = createDbDeps(edtOp, builder, loc);

  if (!edtDeps.empty()) {
    /// Create new EDT operation with dependencies
    EdtOp newEdtOp = createNewEdtOp(edtOp, edtDeps, builder, loc);

    /// Update operations in the new EDT
    updateOpsInNewEdt(edtOp, newEdtOp, edtDeps);

    /// Mark old EDT for removal
    collectOpsToRemove(edtOp, opsToRemove);
  }
}

SmallVector<Value> CreateDbsPass::createDbDeps(EdtOp edtOp, OpBuilder &builder,
                                               Location loc) {
  auto &candidateMap = DbCandidates[edtOp];
  SmallVector<Value> edtDeps;

  /// Group candidates by base pointer to avoid duplicates
  DenseMap<Value, SmallVector<OpsPerCandidate>> candidatesByPtr;

  for (auto &entry : candidateMap) {
    if (!isValidCandidate(entry.first)) {
      LLVM_DEBUG(DBGS() << "Skipping invalid candidate\n");
      continue;
    }
    candidatesByPtr[entry.first.ptr].push_back({entry.first, &entry.second});
  }

  /// Create dependencies for each base pointer
  for (auto &ptrGroup : candidatesByPtr) {
    Value basePtr = ptrGroup.first;
    auto &candidates = ptrGroup.second;

    /// Get or create DbAlloc for this base pointer
    DbDepType combinedDep = combineDepTypes(candidates);
    DbAllocOp dbAllocOp =
        getOrCreateDbAlloc(basePtr, combinedDep, builder, loc);

    if (!dbAllocOp) {
      LLVM_DEBUG(DBGS() << "Failed to create DbAllocOp for " << basePtr
                        << "\n");
      continue;
    }

    /// Create DbDep operations for each candidate
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPoint(edtOp);

    for (auto &[candidate, operations] : candidates) {
      auto dbDepOp = createDbDep(builder, loc, dbAllocOp,
                                 candidate.pinnedIndices, candidate.dep);
      if (dbDepOp) {
        edtDeps.push_back(dbDepOp.getResult());
      }
    }
  }

  return edtDeps;
}

DbDepType
CreateDbsPass::combineDepTypes(const SmallVector<OpsPerCandidate> &candidates) {
  DbDepType combinedDep = DbDepType::Unknown;

  for (auto &[candidate, operations] : candidates) {
    if (combinedDep == DbDepType::Unknown) {
      combinedDep = candidate.dep;
    } else if (combinedDep != candidate.dep) {
      combinedDep = DbDepType::ReadWrite;
      /// No need to continue once we know it's ReadWrite
      break;
    }
  }

  return combinedDep;
}

DbAllocOp CreateDbsPass::getOrCreateDbAlloc(Value basePtr, DbDepType depType,
                                            OpBuilder &builder, Location loc) {
  /// Check if we already created a DbAlloc for this pointer
  if (auto it = createdDbs.find(basePtr); it != createdDbs.end()) {
    return it->second;
  }

  /// Set insertion point
  OpBuilder::InsertionGuard guard(builder);
  if (auto *defOp = basePtr.getDefiningOp()) {
    builder.setInsertionPointAfter(defOp);
  } else if (auto arg = basePtr.dyn_cast<BlockArgument>()) {
    builder.setInsertionPointToStart(arg.getOwner());
  } else {
    LLVM_DEBUG(DBGS() << "Unsupported base pointer type\n");
    return nullptr;
  }

  /// Create the DbAlloc operation
  DbAllocOp dbAllocOp = createDbAlloc(builder, loc, basePtr, depType);
  if (dbAllocOp) {
    createdDbs[basePtr] = dbAllocOp;
  }

  return dbAllocOp;
}

EdtOp CreateDbsPass::createNewEdtOp(EdtOp oldEdtOp,
                                    const SmallVector<Value> &edtDeps,
                                    OpBuilder &builder, Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(oldEdtOp);

  /// Create new EDT with dependencies
  auto newEdtOp = createEdtOp(builder, loc, getEdtType(oldEdtOp), edtDeps);

  /// Move the region body from old to new EDT
  Region &newRegion = newEdtOp.getRegion();
  newRegion.getBlocks().clear();
  newRegion.takeBody(oldEdtOp.getRegion());

  return newEdtOp;
}

void CreateDbsPass::updateOpsInNewEdt(EdtOp oldEdtOp, EdtOp newEdtOp,
                                      const SmallVector<Value> &edtDeps) {
  auto &candidateMap = DbCandidates[oldEdtOp];
  Region &newRegion = newEdtOp.getRegion();

  size_t depIndex = 0;

  /// Group candidates by base pointer for processing
  DenseMap<Value, SmallVector<OpsPerCandidate>> candidatesByPtr;
  for (auto &entry : candidateMap) {
    if (isValidCandidate(entry.first)) {
      candidatesByPtr[entry.first.ptr].push_back({entry.first, &entry.second});
    }
  }

  /// Update operations for each dependency
  for (auto &ptrGroup : candidatesByPtr) {
    for (auto &[candidate, operations] : ptrGroup.second) {
      if (depIndex < edtDeps.size()) {
        Value dbDepResult = edtDeps[depIndex];
        unsigned indicesToDrop = candidate.pinnedIndices.size();

        /// Replace memref operations
        replaceMemRefOps(*operations, dbDepResult, indicesToDrop);

        /// Update nested db_dep operations
        Value originalSource = dbDepResult.getDefiningOp<DbDepOp>().getSource();
        newRegion.walk([&](DbDepOp dbDepOp) {
          if (dbDepOp.getSource() == originalSource) {
            dbDepOp.getSourceMutable().assign(dbDepResult);
          }
        });

        depIndex++;
      }
    }
  }
}

void CreateDbsPass::replaceMemRefOps(const SmallVector<Operation *> &operations,
                                     Value newMemRef, unsigned indicesToDrop) {
  for (Operation *op : operations) {
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      auto indices = loadOp.getIndices();
      loadOp.getMemrefMutable().assign(newMemRef);
      loadOp.getIndicesMutable().assign(indices.drop_front(indicesToDrop));
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      auto indices = storeOp.getIndices();
      storeOp.getMemrefMutable().assign(newMemRef);
      storeOp.getIndicesMutable().assign(indices.drop_front(indicesToDrop));
    }
  }
}

DbDepOp CreateDbsPass::createDbDep(OpBuilder &builder, Location loc,
                                   DbAllocOp dbAllocOp,
                                   const SmallVector<Value> &pinnedIndices,
                                   DbDepType depType) {
  Value dbPtr = dbAllocOp.getPtr();
  auto dbPtrType = dbPtr.getType().cast<MemRefType>();

  int64_t rank = dbPtrType.getRank();
  int64_t pinnedCount = static_cast<int64_t>(pinnedIndices.size());

  assert(pinnedCount <= rank && "Pinned indices exceed memref rank");

  /// Compute offsets and sizes
  SmallVector<Value> offsetValues, sizeValues;

  for (int64_t i = 0; i < rank; ++i) {
    if (i < pinnedCount) {
      /// Pinned dimension: use pinned index as offset, size = 1
      offsetValues.push_back(pinnedIndices[i]);
      sizeValues.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
    } else {
      /// Non-pinned dimension: offset = 0, size = full dimension
      offsetValues.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
      if (dbPtrType.isDynamicDim(i)) {
        sizeValues.push_back(
            builder.create<memref::DimOp>(loc, dbPtr, i).getResult());
      } else {
        sizeValues.push_back(builder.create<arith::ConstantIndexOp>(
            loc, dbPtrType.getDimSize(i)));
      }
    }
  }

  return builder.create<DbDepOp>(
      loc, dbAllocOp.getResult().getType(),
      builder.getStringAttr(types::toString(depType)), dbAllocOp.getResult(),
      pinnedIndices, offsetValues, sizeValues);
}

DbAllocOp CreateDbsPass::createDbAlloc(OpBuilder &builder, Location loc,
                                       Value basePtr, DbDepType dep) {
  /// Create the DbAllocOp
  auto dbAllocOp = createDbAllocOp(builder, loc, types::toString(dep), basePtr);
  auto allocType = arts::getAllocType(basePtr);
  setDbAllocType(dbAllocOp, allocType, builder);

  LLVM_DEBUG(DBGS() << "Created DbAllocOp with allocation type: "
                    << types::toString(allocType) << "\n");
  return dbAllocOp;
}

void CreateDbsPass::collectOpsToRemove(EdtOp edtOp,
                                       SetVector<Operation *> &opsToRemove) {
  /// Add old dependencies to removal list
  for (auto oldDep : edtOp.getDependencies()) {
    if (auto dbOp = oldDep.getDefiningOp<DbDepOp>()) {
      opsToRemove.insert(dbOp);
    }
  }

  /// Add the old EDT operation
  opsToRemove.insert(edtOp);
}

void CreateDbsPass::cleanupOldOps(ModuleOp module, OpBuilder &builder) {
  LLVM_DEBUG(DBGS() << "Removing " << opsToRemove.size()
                    << " old operations\n");
  removeOps(module, builder, opsToRemove);
  opsToRemove.clear();
}

void CreateDbsPass::pruneUnusedDbAllocs(OpBuilder &builder) {
  LLVM_DEBUG(DBGS() << "Pruning unused DbAlloc operations\n");

  SmallVector<Value> toErase;

  for (auto &entry : createdDbs) {
    Value basePtr = entry.first;
    DbAllocOp dbAllocOp = entry.second;

    if (!dbAllocOp || !basePtr) {
      toErase.push_back(basePtr);
      continue;
    }

    /// Replace uses of base pointer with db pointer (except in the DbAlloc
    /// itself)
    basePtr.replaceUsesWithIf(dbAllocOp.getPtr(), [&](OpOperand &use) {
      return use.getOwner() != dbAllocOp;
    });

    /// Check if base pointer has any remaining uses outside the DbAlloc
    bool hasRemainingUses = false;
    for (auto &use : basePtr.getUses()) {
      if (use.getOwner() != dbAllocOp.getOperation()) {
        hasRemainingUses = true;
        break;
      }
    }

    /// If no remaining uses, remove the source operand from DbAlloc
    if (!hasRemainingUses && dbAllocOp.getAddress() == basePtr) {
      LLVM_DEBUG(DBGS() << "Removing unused source operand from DbAllocOp\n");

      OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPoint(dbAllocOp);

      auto sizes = dbAllocOp.getSizes();
      auto allocType = getDbAllocType(dbAllocOp);
      auto newDbAllocOp = createDbAllocOp(
          builder, dbAllocOp.getLoc(), dbAllocOp.getMode(), nullptr,
          SmallVector<Value>(sizes.begin(), sizes.end()));

      if (newDbAllocOp) {
        setDbAllocType(newDbAllocOp, allocType, builder);
        dbAllocOp.getResult().replaceAllUsesWith(newDbAllocOp.getResult());
        dbAllocOp.erase();
        entry.second = newDbAllocOp;
      }
    }
  }

  /// Clean up invalid entries
  for (Value ptr : toErase)
    createdDbs.erase(ptr);
}

void CreateDbsPass::logCandidateInfo(EdtOp edtOp) const {
  LLVM_DEBUG({
    auto &candMap = DbCandidates.find(edtOp)->second;
    size_t candidateIndex = 0;

    for (auto &entry : candMap) {
      auto &candidate = entry.first;
      auto &uses = entry.second;

      dbgs() << "  - Candidate Db #" << candidateIndex++ << ":\n"
             << "    Memref: " << candidate.ptr << "\n"
             << "    IsString: " << (candidate.isString ? "true" : "false")
             << "\n"
             << "    Dep Type: " << types::toString(candidate.dep) << "\n"
             << "    Pinned Indices:";

      if (candidate.pinnedIndices.empty()) {
        dbgs() << " none\n";
      } else {
        for (Value idx : candidate.pinnedIndices) {
          dbgs() << "\n      - " << idx;
        }
        dbgs() << "\n";
      }

      dbgs() << "    Uses:\n";
      for (Operation *op : uses) {
        dbgs() << "      - " << *op << "\n";
      }
    }
  });
}

void CreateDbsPass::logDbCreation(EdtOp edtOp, size_t dbCount) const {
  LLVM_DEBUG({
    static uint32_t edtCounter = 0;
    dbgs() << LINE;
    DBGS() << "Creating " << dbCount << " datablocks for EDT #" << edtCounter++
           << "\n";
  });
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
