///==========================================================================
/// File: CreateDbs.cpp
///
/// This pass creates ARTS Datablocks (DB) operations to handle memory
/// dependencies for EDTs (Event-Driven Tasks). Each EDT may execute in a
/// separate memory environment, so external memory references must be properly
/// managed.
///
/// Pass Overview:
/// 1. Identify external memory allocations used by EDTs
/// 2. Create arts.db_alloc operations for each allocation
/// 3. Analyze memory access patterns within each EDT
/// 4. Insert arts.db_acquire operations before EDTs with computed dependencies
/// 5. Update load/store operations to use acquired memory views
/// 6. Insert arts.db_release operations before EDT terminators
/// 7. Insert arts.db_free operations for db_alloc operations
///
/// This pass assumes the worst-case scenario for access modes, always using
/// inout.
//==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/StringAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"

#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(create_dbs);

using namespace mlir;
using namespace mlir::arts;

/// Dependency information for Datablocks acquisition.
/// Contains the access mode and slice parameters for acquiring a memory region.
struct DepInfo {
  /// Access mode (in, out, inout)
  ArtsMode mode;
  /// Offset values for each dimension
  SmallVector<Value> offsets;
  /// Size values for each dimension
  SmallVector<Value> sizes;
  /// Result type after acquisition
  MemRefType resultType;
};

//===----------------------------------------------------------------------===//
/// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct CreateDbsPass : public arts::CreateDbsBase<CreateDbsPass> {
  CreateDbsPass(bool identifyDbs) { this->identifyDbs = identifyDbs; }

  void runOnOperation() override;

private:
  bool identifyDbs;
  ModuleOp module;
  DenseMap<Value, DbAllocOp> allocToDbAlloc;
  SmallVector<DbAllocOp, 8> createdDbAllocs;
  StringAnalysis *stringAnalysis;

  /// Core helper functions
  void cleanupExistingDbOps();
  SetVector<Value> collectUsedAllocations();
  void createDbAllocOps(const SetVector<Value> &allocs, OpBuilder &builder);
  SetVector<Value> getEdtExternalValues(EdtOp edt);
  void processEdtDeps(EdtOp edt, OpBuilder &builder);

  /// Infer allocation type from the defining operation
  DbAllocType inferAllocType(Value basePtr);

  /// Dependency analysis
  DepInfo computeDepInfo(OpBuilder &builder, EdtOp edt, Value originalValue,
                         Value source);

  /// Access adjustment
  void adjustAccesses(Region &region, Value originalValue, Value view,
                      ArrayRef<Value> offsets);

  /// DbAlloc cleanup
  void insertDbFreeOperations(DbAllocOp dbAlloc, OpBuilder &builder);

  /// Helper function to check if a value is related to the original allocation
  bool isRelatedToAllocation(Value value, DbAllocOp dbAlloc);
};
} // namespace

/// Transforms the module to create DB Acquire/Release Ops for EDT deps
void CreateDbsPass::runOnOperation() {
  module = getOperation();
  ARTS_DEBUG_HEADER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););

  OpBuilder builder(module.getContext());

  /// Run StringAnalysis
  stringAnalysis = new StringAnalysis(module);
  stringAnalysis->run();
  assert(stringAnalysis && "StringAnalysis must be created");

  /// Phase 1: Cleanup existing DB Ops
  cleanupExistingDbOps();

  /// Phase 2: Collect and convert allocations to DBs
  SetVector<Value> allUsedAllocs = collectUsedAllocations();
  ARTS_INFO("Found " << allUsedAllocs.size()
                     << " allocations to convert to DB");
  createDbAllocOps(allUsedAllocs, builder);

  /// Phase 3: Process EDTs for dependencies
  SmallVector<EdtOp> allEdts;
  module.walk([&](EdtOp edt) { allEdts.push_back(edt); });
  ARTS_INFO("Processing " << allEdts.size() << " EDT operations");
  for (auto it = allEdts.rbegin(); it != allEdts.rend(); ++it)
    processEdtDeps(*it, builder);

  /// Phase 4: Insert DbFree operations
  ARTS_INFO("Inserting DbFree operations for " << createdDbAllocs.size()
                                               << " DB allocations");
  for (DbAllocOp dbAlloc : createdDbAllocs)
    insertDbFreeOperations(dbAlloc, builder);

  ARTS_DEBUG_FOOTER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

DbAllocType CreateDbsPass::inferAllocType(Value basePtr) {
  if (!basePtr)
    return DbAllocType::heap;

  if (auto definingOp = basePtr.getDefiningOp()) {
    if (isa<memref::AllocaOp>(definingOp))
      return DbAllocType::stack;
    if (isa<memref::AllocOp>(definingOp))
      return DbAllocType::heap;
    if (isa<memref::GetGlobalOp>(definingOp))
      return DbAllocType::global;
  }
  return DbAllocType::unknown;
}

void CreateDbsPass::cleanupExistingDbOps() {
  /// Erase all db_control ops, replacing with their pointer
  module.walk([&](DbControlOp dbControl) {
    dbControl.getSubview().replaceAllUsesWith(dbControl.getPtr());
    dbControl.erase();
  });

  /// Clear all existing EDT dependencies and block args that are not used
  /// within the block
  module.walk([&](EdtOp edt) {
    Block &block = edt.getBody().front();

    /// Only erase block arguments that are not used within the block
    block.eraseArguments([&](BlockArgument arg) {
      /// Check if this argument is used anywhere in the block
      bool isUsed = false;
      block.walk([&](Operation *op) {
        for (Value operand : op->getOperands()) {
          if (operand == arg) {
            isUsed = true;
            return WalkResult::interrupt();
          }
        }
        return WalkResult::advance();
      });

      /// Only erase if the argument is not used
      return !isUsed;
    });

    /// Preserve the route operand when clearing dependencies
    SmallVector<Value> preservedOperands;
    if (Value route = edt.getRoute())
      preservedOperands.push_back(route);
    edt->setOperands(preservedOperands);
  });
}

SetVector<Value> CreateDbsPass::collectUsedAllocations() {
  SetVector<Value> allUsedAllocs;
  module.walk([&](EdtOp edt) {
    edt.walk([&](Operation *op) {
      Value mem;
      if (auto load = dyn_cast<memref::LoadOp>(op))
        mem = load.getMemref();
      else if (auto store = dyn_cast<memref::StoreOp>(op))
        mem = store.getMemref();
      else
        return;

      Value base = arts::getUnderlyingValue(mem);
      if (base)
        allUsedAllocs.insert(base);
    });
  });
  return allUsedAllocs;
}

void CreateDbsPass::createDbAllocOps(const SetVector<Value> &allocs,
                                     OpBuilder &builder) {

  for (Value alloc : allocs) {
    Operation *allocOp = alloc.getDefiningOp();
    if (!allocOp)
      continue;

    Location loc = allocOp->getLoc();
    builder.setInsertionPointAfter(allocOp);

    /// Create DbAllocOp
    DbAllocType allocType = inferAllocType(alloc);
    ArtsMode mode = ArtsMode::inout;
    DbMode dbMode = DbMode::write;
    ARTS_DEBUG("DbAlloc mode=" << static_cast<int>(mode)
                               << " dbMode=" << static_cast<int>(dbMode));
    auto memRefType = alloc.getType().cast<MemRefType>();
    Type elementType = memRefType.getElementType();

    /// Optimized: Pre-allocate and compute sizes more efficiently
    SmallVector<Value> sizes, payloadSizes;
    const int rank = memRefType.getRank();
    sizes.reserve(rank);

    for (int i = 0; i < rank; ++i) {
      if (memRefType.isDynamicDim(i)) {
        sizes.push_back(builder.create<arts::DbDimOp>(loc, alloc, (int64_t)i));
      } else {
        sizes.push_back(builder.create<arith::ConstantIndexOp>(
            loc, memRefType.getDimSize(i)));
      }
    }

    auto zeroRoute = builder.create<arith::ConstantIntOp>(loc, 0, 32);
    auto dbAllocOp =
        builder.create<DbAllocOp>(loc, mode, zeroRoute, allocType, dbMode,
                                  elementType, alloc, sizes, payloadSizes);

    alloc.replaceUsesWithIf(dbAllocOp.getPtr(), [&](OpOperand &use) -> bool {
      Operation *user = use.getOwner();
      /// Skip operands to the db_alloc, db_dim ops and EDT regions.
      return !isa<DbAllocOp>(user) && !isa<DbDimOp>(user) &&
             !user->getParentOfType<EdtOp>();
    });

    /// Store mappings for later use
    allocToDbAlloc[alloc] = dbAllocOp;
    createdDbAllocs.push_back(dbAllocOp);
  }
}

DepInfo CreateDbsPass::computeDepInfo(OpBuilder &builder, EdtOp edt,
                                      Value originalValue, Value source) {
  MemRefType sourceType = source.getType().dyn_cast<MemRefType>();
  assert(sourceType && "Source must be memref");

  /// For strings, force coarse full-region acquisition.
  if (stringAnalysis->isStringMemRef(originalValue)) {
    int rank = sourceType.getRank();
    SmallVector<Value> offsets, sizes;
    for (int d = 0; d < rank; ++d) {
      offsets.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), 0));
      Value sizeVal;
      if (sourceType.isDynamicDim(d)) {
        auto dimIdx = builder.create<arith::ConstantIndexOp>(edt.getLoc(), d);
        sizeVal = builder.create<arts::DbDimOp>(edt.getLoc(), source, dimIdx);
      } else
        sizeVal = builder.create<arith::ConstantIndexOp>(
            edt.getLoc(), sourceType.getDimSize(d));
      sizes.push_back(sizeVal);
    }

    MemRefType resultType =
        MemRefType::get(sourceType.getShape(), sourceType.getElementType());
    return {ArtsMode::inout, offsets, sizes, resultType};
  }
  /// For other memrefs, use a fine-grained approach.
  SmallVector<Value> offsets, sizes;
  MemRefType resultType = sourceType;
  return {ArtsMode::inout, offsets, sizes, resultType};
}

SetVector<Value> CreateDbsPass::getEdtExternalValues(EdtOp edt) {
  SetVector<Value> externalValues;
  auto &edtRegion = edt.getRegion();

  /// Collect external values used by this EDT (excluding nested EDTs)
  edt.walk([&](Operation *op) -> WalkResult {
    if (isa<EdtOp>(op) && op != edt.getOperation())
      return WalkResult::skip();

    Value mem;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      mem = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      mem = store.getMemref();
    else
      return WalkResult::advance();

    /// Mark as external if defined outside the EDT region
    Value underlyingValue = arts::getUnderlyingValue(mem);
    if (!underlyingValue)
      return WalkResult::advance();
    Operation *definingOp = underlyingValue.getDefiningOp();
    if (definingOp && !edtRegion.isAncestor(definingOp->getParentRegion()))
      externalValues.insert(underlyingValue);
    return WalkResult::advance();
  });

  return externalValues;
}

void CreateDbsPass::processEdtDeps(EdtOp edt, OpBuilder &builder) {
  SetVector<Value> externalValues = getEdtExternalValues(edt);

  /// Only process if there are external values to acquire
  if (externalValues.empty())
    return;

  /// Process each external value
  for (Value value : externalValues) {
    /// Find source (db_alloc, dbAcquire)
    Operation *sourceOp = nullptr;
    if (allocToDbAlloc.count(value))
      sourceOp = allocToDbAlloc[value];
    else
      sourceOp = arts::getUnderlyingDb(value);

    /// Check that the source operation is a DbAllocOp or DbAcquireOp
    assert(sourceOp && "Source operation must be non-null");
    assert(sourceOp->getNumResults() == 2 &&
           "Source operation must have 2 results");
    auto sourceGuid = sourceOp->getResult(0);
    auto sourcePtr = sourceOp->getResult(1);
    assert((sourceGuid && sourcePtr) && "Source guid and ptr must be non-null");

    /// Create acquire operation (pass both source guid and ptr)
    builder.setInsertionPoint(edt);
    DepInfo info = computeDepInfo(builder, edt, value, sourcePtr);
    DbAcquireOp acquire = builder.create<DbAcquireOp>(
        edt.getLoc(), info.mode, sourceGuid, sourcePtr, SmallVector<Value>{},
        info.offsets, info.sizes);

    /// Add dependency to EDT
    SmallVector<Value> newOperands;
    if (Value route = edt.getRoute())
      newOperands.push_back(route);
    auto existingDeps = edt.getDependenciesAsVector();
    newOperands.append(existingDeps.begin(), existingDeps.end());
    newOperands.push_back(acquire.getPtr());
    edt->setOperands(newOperands);

    BlockArgument arg =
        edt.getBody().front().addArgument(info.resultType, edt.getLoc());

    /// Adjust accesses and add release
    adjustAccesses(edt.getRegion(), value, arg, info.offsets);
    builder.setInsertionPoint(edt.getBody().front().getTerminator());
    builder.create<DbReleaseOp>(edt.getLoc(), arg);
  }
}

void CreateDbsPass::adjustAccesses(Region &region, Value originalValue,
                                   Value view, ArrayRef<Value> offsets) {
  region.walk([&](Operation *op) -> WalkResult {
    /// Do not rewrite inside nested EDTs. Each EDT will be handled separately
    /// and should use its own acquired block arguments, not the parent's.
    if (isa<EdtOp>(op))
      return WalkResult::skip();
    Value mem;
    SmallVector<Value> oldIndices;
    bool isLoad = false;
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      mem = load.getMemref();
      oldIndices = load.getIndices();
      isLoad = true;
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      mem = store.getMemref();
      oldIndices = store.getIndices();
    } else if (auto gep = dyn_cast<DbGepOp>(op)) {
      mem = gep.getBasePtr();
      oldIndices.assign(gep.getIndices().begin(), gep.getIndices().end());
    } else {
      return WalkResult::advance();
    }
    if (arts::getUnderlyingValue(mem) != originalValue)
      return WalkResult::advance();

    /// Rewrite the memref to the acquired view.
    if (isLoad) {
      auto load = cast<memref::LoadOp>(op);
      load.getMemrefMutable().assign(view);
    } else if (auto st = dyn_cast<memref::StoreOp>(op)) {
      auto s = cast<memref::StoreOp>(op);
      s.getMemrefMutable().assign(view);
    }
    return WalkResult::advance();
  });
}

void CreateDbsPass::insertDbFreeOperations(DbAllocOp dbAlloc,
                                           OpBuilder &builder) {
  Region *region = dbAlloc->getParentRegion();
  if (!region)
    return;

  Operation *regionOwner = region->getParentOp();
  if (!regionOwner)
    return;

  DominanceInfo dom(regionOwner);
  SmallVector<Operation *, 8> opsToErase;
  bool foundFreeCalls = false;
  auto replaceFreeCalls = [&](memref::DeallocOp deallocOp) {
    Value memref = deallocOp.getMemref();
    if (isRelatedToAllocation(memref, dbAlloc)) {
      builder.setInsertionPoint(deallocOp);
      /// Free both the datablock pointer and the GUID memref
      builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getPtr());
      builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getGuid());
      opsToErase.push_back(deallocOp);
      foundFreeCalls = true;
    }
  };

  /// Skip outlined EDT functions
  if (auto parentFunc = dyn_cast<func::FuncOp>(regionOwner))
    if (parentFunc.getName().startswith("__arts_edt_"))
      return;

  /// Find and replace existing deallocation operations
  region->walk(replaceFreeCalls);

  /// If no existing deallocations found, insert DbFree before terminators
  if (!foundFreeCalls) {
    for (Block &block : *region) {
      if (Operation *terminator = block.getTerminator()) {
        if (dom.dominates(dbAlloc.getOperation(), terminator)) {
          builder.setInsertionPoint(terminator);
          /// Free both the datablock pointer and the GUID memref
          builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getPtr());
          builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getGuid());
        }
      }
    }
  }

  /// Erase all replaced operations in one batch
  for (Operation *op : opsToErase)
    op->erase();
}

bool CreateDbsPass::isRelatedToAllocation(Value value, DbAllocOp dbAlloc) {
  /// First check if the value is directly from this DbAllocOp
  if (auto dbAllocOp = value.getDefiningOp<DbAllocOp>()) {
    if (dbAllocOp == dbAlloc)
      return true;
  }

  /// Use getUnderlyingValue to find the root allocation for both values
  Value valueRoot = arts::getUnderlyingValue(value);
  Value dbAllocRoot = arts::getUnderlyingValue(dbAlloc.getAddress());

  /// If either root is null, we can't determine the relationship
  if (!valueRoot || !dbAllocRoot)
    return false;

  /// Check if the root allocations are the same
  return valueRoot == dbAllocRoot;
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateDbsPass(bool identifyDbs) {
  return std::make_unique<CreateDbsPass>(identifyDbs);
}
} // namespace arts
} // namespace mlir