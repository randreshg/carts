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
/// 7. Insert arts.db_release operations for db_alloc operations
///
/// The pass computes optimal acquisition regions based on actual access
/// patterns, balancing between reducing dependency overhead and preserving
/// parallelism. Nested EDTs inherit parent acquisitions when possible.
//==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/StringAnalysis.h"
#include "arts/ArtsDialect.h"

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

/// Information about memory access patterns for a memref base.
/// Tracks whether the base is read/written and the access patterns per
/// dimension.
struct AccessInfo {
  /// True if base is read from
  bool hasRead = false;
  /// True if base is written to
  bool hasWrite = false;
  /// Constant indices per dimension
  SmallVector<SetVector<int64_t>> dimConstants;
  /// Non-constant access per dimension
  SmallVector<bool> hasNonConstant;

  AccessInfo(int rank) : dimConstants(rank), hasNonConstant(rank, false) {}
};

/// Dependency information for Datablocks acquisition.
/// Contains the access mode and slice parameters for acquiring a memory region.
struct DepInfo {
  /// Access mode (in, out, inout)
  ArtsMode mode;
  /// Constant leading indices
  SmallVector<Value> pinned;
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
  CreateDbsPass() = default;
  CreateDbsPass(bool identifyDbs) { this->identifyDbs = identifyDbs; }

  void runOnOperation() override;

private:
  bool identifyDbs;
  ModuleOp module;
  DenseMap<Value, DbAllocOp> allocToDbAlloc;
  DenseMap<Operation *, DenseMap<Value, BlockArgument>> edtBaseToArg;
  SmallVector<DbAllocOp> createdDbAllocs;

  /// Cache for getUnderlyingObject to avoid redundant traversals
  DenseMap<Value, Value> underlyingObjectCache;

  /// Core helper functions
  Value getUnderlyingObject(Value v);
  void cleanupExistingDbOps();
  SetVector<Value> collectUsedAllocations();
  void createDbAllocOps(const SetVector<Value> &allocs, OpBuilder &builder);
  void processEdtDependencies(EdtOp edt, OpBuilder &builder,
                              const StringAnalysis &analysis);

  /// Infer allocation type from the defining operation
  DbAllocType inferAllocType(Value basePtr);

  /// Dependency analysis
  AccessInfo analyzeAccesses(EdtOp edt, Value originalBase);
  DepInfo computeDepInfo(OpBuilder &builder, EdtOp edt, Value originalBase,
                         Value source, const StringAnalysis &analysis);
  DepInfo createFullAccessDepInfo(OpBuilder &builder, EdtOp edt, Value source,
                                  ArtsMode mode);
  Value findParentSource(EdtOp edt, Value base);

  /// Access adjustment
  void adjustAccesses(OpBuilder &builder, Region &region, Value originalBase,
                      Value view, SmallVector<Value> pinned,
                      SmallVector<Value> offsets);

  /// DbAlloc cleanup
  void insertDbFreeOperations(DbAllocOp dbAlloc, OpBuilder &builder);

  /// Helper function to check if a value is related to the original allocation
  bool isRelatedToAllocation(Value value, DbAllocOp dbAlloc);
};
} // namespace

Value CreateDbsPass::getUnderlyingObject(Value v) {
  /// Check cache first to avoid redundant traversals
  auto cachedResult = underlyingObjectCache.find(v);
  if (cachedResult != underlyingObjectCache.end())
    return cachedResult->second;

  Value result = nullptr;

  if (v.isa<BlockArgument>()) {
    Block *block = v.getParentBlock();
    Operation *owner = block->getParentOp();
    if (auto edt = dyn_cast<EdtOp>(owner)) {
      unsigned argIndex = v.cast<BlockArgument>().getArgNumber();
      result = (argIndex < edt.getNumOperands())
                   ? getUnderlyingObject(edt.getOperand(argIndex))
                   : v; // Function argument
    } else {
      result = v; // Function argument
    }
  } else if (auto op = v.getDefiningOp()) {
    /// Handle different operation types
    if (isa<DbAllocOp, memref::AllocOp, memref::AllocaOp, memref::GetGlobalOp>(
            op)) {
      result = v; // Root objects
    } else if (auto dbAcquire = dyn_cast<DbAcquireOp>(op)) {
      result = getUnderlyingObject(dbAcquire.getSourcePtr());
    } else if (auto dbGep = dyn_cast<DbGepOp>(op)) {
      result = getUnderlyingObject(dbGep.getBasePtr());
    } else if (auto subview = dyn_cast<memref::SubViewOp>(op)) {
      result = getUnderlyingObject(subview.getSource());
    } else if (auto castOp = dyn_cast<memref::CastOp>(op)) {
      result = getUnderlyingObject(castOp.getSource());
    } else {
      result = nullptr; // Not a root object
    }
  } else {
    /// Value has no defining operation and is not a block argument
    result = nullptr;
  }

  /// Cache the result for future lookups
  underlyingObjectCache[v] = result;
  return result;
}

/// Infer allocation type from the defining operation
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

      Value base = getUnderlyingObject(mem);
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
    DbMode dbMode = DbMode::pinned;
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
    auto dbAllocOp = builder.create<DbAllocOp>(loc, ArtsMode::inout, zeroRoute,
                                               allocType, dbMode, elementType,
                                               alloc, sizes, payloadSizes);

    alloc.replaceUsesWithIf(dbAllocOp.getPtr(), [&](OpOperand &use) -> bool {
      Operation *user = use.getOwner();
      /// Skip operands to the db_alloc itself and EDT regions
      return !isa<DbAllocOp>(user) && !user->getParentOfType<EdtOp>();
    });

    /// Store mappings for later use
    allocToDbAlloc[alloc] = dbAllocOp;
    createdDbAllocs.push_back(dbAllocOp);
  }
}

AccessInfo CreateDbsPass::analyzeAccesses(EdtOp edt, Value originalBase) {
  Value anyMem;
  edt.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (getUnderlyingObject(load.getMemref()) == originalBase) {
        anyMem = load.getMemref();
        return;
      }
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (getUnderlyingObject(store.getMemref()) == originalBase) {
        anyMem = store.getMemref();
        return;
      }
    }
  });

  /// If no accesses found, return access info with rank 0
  if (!anyMem)
    return AccessInfo(0);

  MemRefType type = anyMem.getType().cast<MemRefType>();
  AccessInfo info(type.getRank());

  edt.walk([&](Operation *op) {
    Value mem;
    SmallVector<Value> indices;
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      mem = load.getMemref();
      indices = load.getIndices();
      info.hasRead = true;
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      mem = store.getMemref();
      indices = store.getIndices();
      info.hasWrite = true;
    } else {
      return;
    }

    /// Only analyze accesses to the original base
    if (getUnderlyingObject(mem) != originalBase)
      return;

    for (int d = 0; d < type.getRank(); ++d) {
      Value idx = indices[d];
      if (auto constOp = idx.getDefiningOp<arith::ConstantIndexOp>()) {
        info.dimConstants[d].insert(
            constOp.getValue().cast<IntegerAttr>().getInt());
      } else {
        info.hasNonConstant[d] = true;
      }
    }
  });
  return info;
}

DepInfo CreateDbsPass::computeDepInfo(OpBuilder &builder, EdtOp edt,
                                      Value originalBase, Value source,
                                      const StringAnalysis &analysis) {
  MemRefType sourceType = source.getType().dyn_cast<MemRefType>();
  assert(sourceType && "Source must be memref");
  int rank = sourceType.getRank();

  /// Handle scalar case (rank 0)
  if (rank == 0) {
    ArtsMode mode = ArtsMode::inout;
    MemRefType resultType =
        MemRefType::get(sourceType.getShape(), sourceType.getElementType());
    return {mode, SmallVector<Value>{}, SmallVector<Value>{},
            SmallVector<Value>{}, resultType};
  }

  AccessInfo accInfo = analyzeAccesses(edt, originalBase);

  ArtsMode mode;
  if (accInfo.hasWrite)
    mode = accInfo.hasRead ? ArtsMode::inout : ArtsMode::out;
  else if (accInfo.hasRead)
    mode = ArtsMode::in;
  else
    mode = ArtsMode::inout;

  /// String memrefs: acquire the whole memory
  if (analysis.isStringMemRef(originalBase))
    return createFullAccessDepInfo(builder, edt, source, mode);

  /// Compute pinned indices (leading constant dimensions)
  SmallVector<Value> pinned;
  int prefix = 0;
  for (int d = 0; d < rank; ++d) {
    if (accInfo.dimConstants[d].size() == 1 && !accInfo.hasNonConstant[d]) {
      int64_t val = *accInfo.dimConstants[d].begin();
      pinned.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), val));
      prefix++;
    } else
      break;
  }

  /// Compute offsets and sizes for remaining dimensions
  SmallVector<Value> offsets, sizes;
  bool isFull = true;
  for (int d = prefix; d < rank; ++d) {
    Value offVal, sizeVal;

    if (accInfo.hasNonConstant[d] || accInfo.dimConstants[d].empty()) {
      /// Use full dimension
      offVal = builder.create<arith::ConstantIndexOp>(edt.getLoc(), 0);
      if (sourceType.isDynamicDim(d)) {
        auto dimIdx = builder.create<arith::ConstantIndexOp>(edt.getLoc(), d);
        sizeVal = builder.create<arts::DbDimOp>(edt.getLoc(), source, dimIdx);
      } else
        sizeVal = builder.create<arith::ConstantIndexOp>(
            edt.getLoc(), sourceType.getDimSize(d));
    } else {
      /// Use bounding box of constant accesses
      int64_t minVal = *std::min_element(accInfo.dimConstants[d].begin(),
                                         accInfo.dimConstants[d].end());
      int64_t maxVal = *std::max_element(accInfo.dimConstants[d].begin(),
                                         accInfo.dimConstants[d].end());
      offVal = builder.create<arith::ConstantIndexOp>(edt.getLoc(), minVal);
      sizeVal = builder.create<arith::ConstantIndexOp>(edt.getLoc(),
                                                       maxVal - minVal + 1);
      isFull = false;
    }
    offsets.push_back(offVal);
    sizes.push_back(sizeVal);
  }

  /// If accessing full dims with no pinning, clear offsets/sizes
  if (isFull && prefix == 0) {
    offsets.clear();
    sizes.clear();
  }

  SmallVector<int64_t> newShape(rank - prefix, ShapedType::kDynamic);
  MemRefType resultType =
      MemRefType::get(newShape, sourceType.getElementType());
  return {mode, pinned, offsets, sizes, resultType};
}

DepInfo CreateDbsPass::createFullAccessDepInfo(OpBuilder &builder, EdtOp edt,
                                               Value source, ArtsMode mode) {
  MemRefType sourceType = source.getType().dyn_cast<MemRefType>();
  assert(sourceType && "Source must be memref");
  int rank = sourceType.getRank();

  SmallVector<Value> offsets, sizes;
  for (int d = 0; d < rank; ++d) {
    offsets.push_back(builder.create<arith::ConstantIndexOp>(edt.getLoc(), 0));
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
  return {mode, SmallVector<Value>{}, offsets, sizes, resultType};
}

Value CreateDbsPass::findParentSource(EdtOp edt, Value base) {
  auto parentEdt = edt->getParentOfType<EdtOp>();
  if (!parentEdt)
    return nullptr;

  /// Try to find base in parent's block arguments mapping
  auto parentIt = edtBaseToArg.find(parentEdt.getOperation());
  if (parentIt != edtBaseToArg.end()) {
    auto it = parentIt->second.find(base);
    if (it != parentIt->second.end())
      return it->second;
  }

  /// Fallback: scan parent block arguments
  for (BlockArgument arg : parentEdt.getBody().getArguments()) {
    if (getUnderlyingObject(arg) == base)
      return arg;
  }
  return nullptr;
}

void CreateDbsPass::processEdtDependencies(EdtOp edt, OpBuilder &builder,
                                           const StringAnalysis &analysis) {
  SetVector<Value> externalBases;

  /// Collect external bases used by this EDT (excluding nested EDTs)
  WalkResult result = edt.walk([&](Operation *op) -> WalkResult {
    if (isa<EdtOp>(op) && op != edt.getOperation())
      return WalkResult::skip();

    Value mem;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      mem = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      mem = store.getMemref();
    else
      return WalkResult::advance();

    Value base = getUnderlyingObject(mem);
    if (!base)
      return WalkResult::advance();

    /// Mark as external if defined outside the EDT region
    Operation *definingOp = base.getDefiningOp();
    if (definingOp &&
        !edt.getRegion().isAncestor(definingOp->getParentRegion()))
      externalBases.insert(base);
    return WalkResult::advance();
  });

  if (result.wasInterrupted())
    return;

  /// Only process if there are external bases to acquire
  if (externalBases.empty())
    return;

  /// Process each external base
  for (Value base : externalBases) {
    /// Find source (db_alloc, parent arg, or original base)
    Value source = base;
    if (allocToDbAlloc.count(base))
      source = allocToDbAlloc[base].getPtr();
    else if (Value parentSource = findParentSource(edt, base))
      source = parentSource;

    builder.setInsertionPoint(edt);
    DepInfo info = computeDepInfo(builder, edt, base, source, analysis);

    /// Create acquire operation (pass both source guid and ptr)
    Value sourceGuid;
    if (auto allocOp = source.getDefiningOp<DbAllocOp>())
      sourceGuid = allocOp.getGuid();
    else if (auto acqOp = source.getDefiningOp<DbAcquireOp>())
      sourceGuid = acqOp.getGuid();

    assert((sourceGuid && source) && "Source guid and ptr must be non-null");

    DbAcquireOp acquire =
        builder.create<DbAcquireOp>(edt.getLoc(), info.mode, sourceGuid, source,
                                    info.pinned, info.offsets, info.sizes);

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

    /// Record mapping for nested EDTs
    edtBaseToArg[edt.getOperation()][base] = arg;

    /// Adjust accesses and add release
    adjustAccesses(builder, edt.getRegion(), base, arg, info.pinned,
                   info.offsets);
    builder.setInsertionPoint(edt.getBody().front().getTerminator());
    builder.create<DbReleaseOp>(edt.getLoc(), arg);
  }
}

void CreateDbsPass::adjustAccesses(OpBuilder &builder, Region &region,
                                   Value originalBase, Value view,
                                   SmallVector<Value> pinned,
                                   SmallVector<Value> offsets) {
  int prefix = pinned.size();
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
    } else {
      return WalkResult::advance();
    }
    if (getUnderlyingObject(mem) != originalBase)
      return WalkResult::advance();

    /// Verify leading pinned indices
    for (int i = 0; i < prefix; ++i) {
      Value idx = oldIndices[i];
      auto constOp = idx.getDefiningOp<arith::ConstantIndexOp>();
      assert(constOp &&
             constOp.getValue() ==
                 pinned[i].getDefiningOp<arith::ConstantIndexOp>().getValue());
    }

    SmallVector<Value> newIndices;
    for (size_t i = prefix; i < oldIndices.size(); ++i) {
      Value idx = oldIndices[i];
      size_t trailIdx = i - prefix;
      if (trailIdx < offsets.size()) {
        auto offConst =
            offsets[trailIdx].getDefiningOp<arith::ConstantIndexOp>();
        if (offConst && offConst.getValue() != 0) {
          builder.setInsertionPoint(op);
          idx = builder.create<arith::SubIOp>(op->getLoc(), idx,
                                              offsets[trailIdx]);
        }
      }
      newIndices.push_back(idx);
    }

    if (isLoad) {
      auto load = cast<memref::LoadOp>(op);
      load.getMemrefMutable().assign(view);
      load.getIndicesMutable().assign(newIndices);
    } else {
      auto store = cast<memref::StoreOp>(op);
      store.getMemrefMutable().assign(view);
      store.getIndicesMutable().assign(newIndices);
    }
    return WalkResult::advance();
  });
}

/// Insert db_free operations for a Datablocks allocation.
/// Inserts DbFreeOp before all terminators/returns dominated by the db_alloc.
/// Also detects and replaces existing free calls with DbFree operations.
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
  auto replaceFreeCalls = [&](Operation *op) {
    Value operandToCheck = nullptr;
    Operation *opToReplace = nullptr;

    if (auto callOp = dyn_cast<func::CallOp>(op)) {
      if (callOp.getCallee() == "free") {
        for (Value operand : callOp.getOperands()) {
          if (isRelatedToAllocation(operand, dbAlloc)) {
            operandToCheck = operand;
            opToReplace = callOp;
            break;
          }
        }
      }
    } else if (auto deallocOp = dyn_cast<memref::DeallocOp>(op)) {
      Value memref = deallocOp.getMemref();
      if (isRelatedToAllocation(memref, dbAlloc)) {
        operandToCheck = memref;
        opToReplace = deallocOp;
      }
    }

    if (opToReplace) {
      builder.setInsertionPoint(opToReplace);
      /// Free both the datablock pointer and the GUID memref
      builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getPtr());
      builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getGuid());
      opsToErase.push_back(opToReplace);
      foundFreeCalls = true;
      ARTS_INFO("Replaced deallocation operation with DbFree");
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

/// Helper function to check if a value is related to the original allocation
/// that was replaced by a DbAlloc operation
bool CreateDbsPass::isRelatedToAllocation(Value value, DbAllocOp dbAlloc) {
  /// First check if the value is directly from this DbAllocOp
  if (auto dbAllocOp = value.getDefiningOp<DbAllocOp>()) {
    if (dbAllocOp == dbAlloc)
      return true;
  }

  /// Use getUnderlyingObject to find the root allocation for both values
  Value valueRoot = getUnderlyingObject(value);
  Value dbAllocRoot = getUnderlyingObject(dbAlloc.getAddress());

  /// If either root is null, we can't determine the relationship
  if (!valueRoot || !dbAllocRoot)
    return false;

  /// Check if the root allocations are the same
  return valueRoot == dbAllocRoot;
}

/// Main pass entry point.
/// Transforms the module to create ARTS Datablocks operations for EDT memory
/// dependencies. Process is divided into three phases for clarity and
/// correctness.
void CreateDbsPass::runOnOperation() {
  module = getOperation();
  ARTS_DEBUG_HEADER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Clear cache at the beginning for fresh analysis
  underlyingObjectCache.clear();

  OpBuilder builder(module.getContext());
  StringAnalysis stringAnalysis(module);

  /// Phase 1: Cleanup existing DB operations
  cleanupExistingDbOps();

  /// Phase 2: Collect and convert allocations to Datablockss
  SetVector<Value> allUsedAllocs = collectUsedAllocations();
  ARTS_INFO("Found " << allUsedAllocs.size()
                     << " allocations to convert to Datablockss");
  createDbAllocOps(allUsedAllocs, builder);

  /// Phase 3: Process EDTs for dependencies
  SmallVector<EdtOp> allEdts;
  module.walk([&](EdtOp edt) { allEdts.push_back(edt); });
  ARTS_INFO("Processing " << allEdts.size() << " EDT operations");
  for (auto it = allEdts.rbegin(); it != allEdts.rend(); ++it)
    processEdtDependencies(*it, builder, stringAnalysis);

  /// Phase 4: Insert DbFree operations
  ARTS_INFO("Inserting DbFree operations for " << createdDbAllocs.size()
                                               << " Datablocks allocations");
  for (DbAllocOp dbAlloc : createdDbAllocs)
    insertDbFreeOperations(dbAlloc, builder);

  /// Clear cache at the end to free memory
  underlyingObjectCache.clear();

  ARTS_DEBUG_FOOTER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
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