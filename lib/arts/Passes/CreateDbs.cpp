///==========================================================================
/// File: CreateDbs.cpp
///
/// This pass creates ARTS Dbs (DB) operations to handle memory
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
/// Allocation Strategy:
/// This pass analyzes memory access patterns from task dependencies
/// (DbControlOps) to determine the optimal datablock allocation granularity:
///
/// 1. Fine-grained allocation (preferred when possible):
///    - Detects consistent single-dimension indexing patterns (e.g., A[i])
///    - Verifies ALL db_acquires have indices for fine-grained access
///    - Creates multiple datablocks: sizes=[N], elementSizes=[remaining dims]
///
/// 2. Coarse-grained allocation (fallback):
///    - Used when no task dependencies are provided (no DbControlOps)
///    - Used when access patterns are inconsistent across EDTs
///    - Used when some db_acquires lack indices (full array access needed)
///
/// Access Mode Strategy:
/// Access modes are extracted from DbControlOps (from OpenMP depend clauses).
/// If no control DB is found, conservatively defaults to inout mode.
//==========================================================================

#include "ArtsPassDetails.h"
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
#include "polygeist/Ops.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(create_dbs);

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Helper Functions
//===----------------------------------------------------------------------===//
namespace {

//===----------------------------------------------------------------------===//
// Access Mode Combination
/// Combine two access modes and return the more permissive mode
//===----------------------------------------------------------------------===//
static ArtsMode combineAccessModes(ArtsMode mode1, ArtsMode mode2) {
  /// If either mode is inout, the result is inout (most permissive)
  if (mode1 == ArtsMode::inout || mode2 == ArtsMode::inout)
    return ArtsMode::inout;

  /// If one is 'in' and the other is 'out', the result is inout
  if ((mode1 == ArtsMode::in && mode2 == ArtsMode::out) ||
      (mode1 == ArtsMode::out && mode2 == ArtsMode::in))
    return ArtsMode::inout;

  /// If both are the same, return that mode
  if (mode1 == mode2)
    return mode1;

  /// Default to inout for any other combination (shouldn't happen)
  return ArtsMode::inout;
}
//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//

struct CreateDbsPass : public arts::CreateDbsBase<CreateDbsPass> {
  CreateDbsPass(bool identifyDbs) { this->identifyDbs = identifyDbs; }

  void runOnOperation() override;

private:
  bool identifyDbs;
  ModuleOp module;
  DenseMap<Operation *, Operation *> dbPtrToOriginalAlloc;
  SetVector<Operation *> opsToRemove;

  /// Consolidated structure to track all usage information for a memref
  struct MemrefInfo {
    /// Base allocation
    Operation *alloc = nullptr;
    /// Parent EDT
    EdtOp parentEdt = nullptr;
    /// Combined access mode for this memref
    ArtsMode accessMode = ArtsMode::in;
    /// Whether this memref used fine-grained allocation
    bool usedFineGrained = false;
    /// The DB allocation operation created for this memref
    DbAllocOp dbAllocOp = nullptr;

    /// Structure to track a specific DB access (mode + index)
    struct DbDep {
      ArtsMode mode;
      SmallVector<Value, 4> indices;
    };

    /// Map from EDT operation to all DB accesses for that EDT
    DenseMap<EdtOp, SmallVector<DbDep>> edtToDeps;

    /// Add an access for this memref and update combined mode
    void addAccess(EdtOp edtOp, ArtsMode mode, SmallVector<Value, 4> indices) {
      accessMode = combineAccessModes(accessMode, mode);
      edtToDeps[edtOp].push_back({mode, indices});
    }

    /// Compute index pattern information from stored accesses
    struct AccessPatternInfo {
      /// Whether all accesses have the same number of indices
      bool isConsistent = true;
      /// Whether all accesses have indices
      bool allAccessesHaveIndices = true;
      /// Minimum number of pinned dimensions across all accesses
      unsigned pinnedDimCount = 0;
    };

    AccessPatternInfo getAccessPatternInfo() const {
      AccessPatternInfo info;
      std::optional<unsigned> expectedDimCount;
      bool hasPinnedAccess = false;

      for (const auto &[edtOp, accesses] : edtToDeps) {
        for (const auto &access : accesses) {
          if (access.indices.empty()) {
            info.allAccessesHaveIndices = false;
          } else {
            unsigned dimCount = access.indices.size();

            /// Track minimum pinned dimensions
            if (!hasPinnedAccess) {
              info.pinnedDimCount = dimCount;
              hasPinnedAccess = true;
            } else {
              info.pinnedDimCount = std::min(info.pinnedDimCount, dimCount);
            }

            /// Check consistency
            if (!expectedDimCount) {
              expectedDimCount = dimCount;
              info.pinnedDimCount = dimCount;
            } else if (*expectedDimCount != dimCount) {
              info.isConsistent = false;
            }
          }
        }
      }

      return info;
    }
  };

  /// Map from memref value to its consolidated usage information
  DenseMap<Operation *, MemrefInfo> memrefInfo;

  /// Map an edtOp to its external values
  DenseMap<EdtOp, SetVector<Value>> edtExternalValues;

  /// Core helper functions
  void collectMemrefs();
  void collectControlDbOps();
  void createDbAllocOps();

  /// Helper functions for collectControlDbOps
  void createDbAcquireOps(EdtOp edt, SetVector<Value> &externalDeps);

  /// Infer allocation type from the defining operation
  DbAllocType inferAllocType(Operation *alloc);

  /// Insert DbFreeOp for a DbAllocOp at the appropriate location
  void insertDbFreeForDbAlloc(DbAllocOp dbAlloc, Operation *alloc,
                              OpBuilder &builder);

  /// Rewrite uses of datablocks
  bool rewriteOperation(Operation *op, Type elementMemRefType, Value basePtr,
                        Operation *dbOp, OpBuilder &builder,
                        uint64_t initialIndex = 0);
  void rewriteUsesInEdt(EdtOp edt, Value originalValue, DbAcquireOp dbAcquire,
                        SmallVector<Value> &acquireIndices,
                        BlockArgument dbAcquireArg);
  void rewriteUsesInParentEdt(MemrefInfo &memrefInfo);
};
} // namespace

//===----------------------------------------------------------------------===//
// Pass Entry Point
//===----------------------------------------------------------------------===//
void CreateDbsPass::runOnOperation() {
  module = getOperation();
  opsToRemove.clear();
  memrefInfo.clear();

  ARTS_INFO_HEADER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Phase 1: Collect memrefs used in EDTs
  ARTS_INFO("Phase 1: Collecting memrefs used in EDTs");
  collectMemrefs();
  ARTS_DEBUG(" - Found " << memrefInfo.size() << " memrefs used in EDTs");

  /// Phase 2: collect and clean up existing Control DB operations
  ARTS_INFO("Phase 2: collect and clean up existing Control DB operations");
  collectControlDbOps();

  /// Phase 3: Create DbAlloc operations
  ARTS_INFO("Phase 3: Creating DbAlloc operations for " << memrefInfo.size()
                                                        << " memrefs");
  createDbAllocOps();

  /// Phase 4: Process EDTs for dependencies
  ARTS_INFO("Phase 4: Creating DbAcquire operations for "
            << edtExternalValues.size() << " EDTs");
  for (auto &entry : edtExternalValues) {
    EdtOp edt = entry.first;
    SetVector<Value> &externalDeps = entry.second;
    createDbAcquireOps(edt, externalDeps);
  }

  /// Phase 5: Clean up legacy allocations
  ARTS_INFO("Phase 5: Cleaning up legacy allocations");
  removeOps(module, opsToRemove, true);

  /// Phase 6: Enabling verification for EDTs
  module.walk([](EdtOp edt) { edt.removeNoVerifyAttr(); });

  /// Phase 7: Simplify the module
  DominanceInfo domInfo(module);
  arts::simplifyIR(module, domInfo);

  ARTS_INFO_FOOTER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
// Allocation Type Inference
//===----------------------------------------------------------------------===//
DbAllocType CreateDbsPass::inferAllocType(Operation *alloc) {
  assert(alloc && "Allocation operation not found");

  if (isa<memref::AllocaOp>(alloc))
    return DbAllocType::stack;
  if (isa<memref::AllocOp>(alloc))
    return DbAllocType::heap;
  if (isa<memref::GetGlobalOp>(alloc))
    return DbAllocType::global;

  /// Default to heap for unknown allocation types
  assert(false && "Unknown allocation type");
  return DbAllocType::heap;
}

//===----------------------------------------------------------------------===//
// Collect Allocations Used in EDTs
//===----------------------------------------------------------------------===//
void CreateDbsPass::collectMemrefs() {
  memrefInfo.clear();
  module.walk([&](EdtOp edt) {
    ARTS_DEBUG(" - Collecting memrefs used in EDT\n" << edt << "\n");
    edt.walk([&](Operation *op) {
      /// Skip self-references
      if (auto edtOp = dyn_cast<EdtOp>(op)) {
        if (edtOp == edt)
          return;
      }

      ARTS_DEBUG(" Analyzing operation: " << *op);

      /// Check all operands of all operations for memory references
      for (Value operand : op->getOperands()) {
        if (!operand.getType().isa<MemRefType>())
          continue;
        ARTS_DEBUG("   - Operand: " << operand);
        /// Get the underlying value of the memory reference
        Operation *underlyingOp = arts::getUnderlyingOperation(operand);
        if (!underlyingOp) {
          ARTS_ERROR("Value with no underlying operation: " << operand);
          assert(false && "Value with no underlying operation");
          return;
        }

        /// If it is found, check the parent edt
        EdtOp parentEdt = underlyingOp->getParentOfType<EdtOp>();

        /// Create memref info for the underlying operation
        MemrefInfo &info = memrefInfo[underlyingOp];
        info.alloc = underlyingOp;
        info.parentEdt = parentEdt;

        /// If the parent edt is different from the current edt, it means
        /// it is an external dependency
        if (parentEdt != edt) {
          ARTS_DEBUG("     - External value");
          edtExternalValues[edt].insert(operand);
        }
      }
    });
  });
}

//===----------------------------------------------------------------------===//
// Collect Control DB Operations
//===----------------------------------------------------------------------===//
void CreateDbsPass::collectControlDbOps() {
  /// First, extract dependency mode and index information from all db_control
  /// ops before erasing them
  SmallVector<DbControlOp, 4> dbControlOps;
  module.walk([&](DbControlOp dbControl) {
    dbControlOps.push_back(dbControl);
    Value ptr = dbControl.getPtr();
    ArtsMode mode = dbControl.getMode();
    auto indices = dbControl.getIndices();

    /// Get the underlying operation for this pointer
    Operation *underlyingOp = arts::getUnderlyingOperation(ptr);
    if (!underlyingOp)
      return;

    ARTS_DEBUG(" - Found DbControlOp for " << *underlyingOp
                                           << " with mode: " << mode << " and "
                                           << indices.size() << " indices");

    /// Get user EDT
    EdtOp userEdt = nullptr;
    for (Operation *user : dbControl.getResult().getUsers()) {
      if (auto edt = dyn_cast<EdtOp>(user)) {
        userEdt = edt;
        break;
      }
    }

    SmallVector<Value, 4> indexValues(indices.begin(), indices.end());
    memrefInfo[underlyingOp].addAccess(userEdt, mode, indexValues);
  });

  /// Now erase all db_control ops, replacing uses with pointer
  for (DbControlOp dbControl : dbControlOps) {
    dbControl.getSubview().replaceAllUsesWith(dbControl.getPtr());
    dbControl.erase();
  }

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

//===----------------------------------------------------------------------===//
// Create DB Allocation Operations
//===----------------------------------------------------------------------===//
void CreateDbsPass::createDbAllocOps() {
  OpBuilder builder(module.getContext());
  /// All memrefs in memrefInfo are converted to DBs
  for (auto &entry : memrefInfo) {
    Operation *alloc = entry.first;
    MemrefInfo &memrefInfo = entry.second;
    Location loc = alloc->getLoc();
    builder.setInsertionPointAfter(alloc);

    /// Infer allocation type based on the defining operation
    DbAllocType allocType = inferAllocType(alloc);

    /// Use the dependency mode from DbControlOp
    ArtsMode mode = memrefInfo.accessMode;
    ARTS_DEBUG(" - Using access mode " << mode << " for memref " << *alloc);

    /// Set DbMode based on the access mode
    DbMode dbMode = DbMode::write;
    if (mode == ArtsMode::in)
      dbMode = DbMode::read;
    else if (mode == ArtsMode::out || mode == ArtsMode::inout)
      dbMode = DbMode::write;

    /// Get the element type of the memref
    Value allocValue = alloc->getResult(0);
    MemRefType memRefType = allocValue.getType().cast<MemRefType>();
    Type elementType = memRefType.getElementType();
    while (auto nestedMemRef = elementType.dyn_cast<MemRefType>())
      elementType = nestedMemRef.getElementType();

    /// Determine allocation granularity based on index access pattern
    /// - If all accesses use a consistent dimension, allocate fine-grained DBs
    /// - Otherwise, use coarse-grained allocation (single DB with full memref)
    SmallVector<Value> sizes, elementSizes;
    const unsigned rank = memRefType.getRank();
    const auto accessPatternInfo = memrefInfo.getAccessPatternInfo();
    bool useFineGrainedAllocation = accessPatternInfo.isConsistent &&
                                    accessPatternInfo.pinnedDimCount > 0 &&
                                    accessPatternInfo.allAccessesHaveIndices;

    if (useFineGrainedAllocation) {
      memrefInfo.usedFineGrained = true;
      const auto pinnedDimCount = accessPatternInfo.pinnedDimCount;

      /// All indexed dimensions go into sizes (outer array
      /// dimensions) Non-indexed dimensions go into elementSizes (inner
      /// datablock dimensions)
      ///
      /// Example: memref<?x?x?xf64> with indices [i,j] (indexing dims 0,1)
      ///   sizes=[N,M], elementSizes=[K] -> memref<?x?xmemref<?xf64>>
      ///   Acquire: indices=[i,j] -> selects one memref<?xf64> datablock
      ///
      /// This allows fine-grained element-level distribution based on all
      /// indexed dimensions, with db_acquire using indices to directly select
      /// datablocks from the N-dimensional outer array.

      ARTS_DEBUG(" - Using fine-grained allocation with "
                 << pinnedDimCount << " pinned dimensions");

      /// Add all pinned dimensions to sizes
      for (unsigned dim = 0; dim < pinnedDimCount; ++dim) {
        if (memRefType.isDynamicDim(dim)) {
          sizes.push_back(
              builder.create<arts::DbDimOp>(loc, allocValue, (int64_t)dim));
        } else {
          sizes.push_back(builder.create<arith::ConstantIndexOp>(
              loc, memRefType.getDimSize(dim)));
        }
      }

      /// All non-pinned dimensions go into elementSizes
      for (unsigned i = pinnedDimCount; i < rank; ++i) {
        if (memRefType.isDynamicDim(i)) {
          elementSizes.push_back(
              builder.create<arts::DbDimOp>(loc, allocValue, (int64_t)i));
        } else {
          elementSizes.push_back(builder.create<arith::ConstantIndexOp>(
              loc, memRefType.getDimSize(i)));
        }
      }

      ARTS_DEBUG("   - Created fine-grained DB: Outer rank="
                 << sizes.size() << ", Inner rank=" << elementSizes.size());
    } else {
      /// Coarse-grained: all dimensions go to elementSizes, single DB
      ARTS_DEBUG(" - Falling back to coarse-grained allocation");
      elementSizes.reserve(rank);
      for (unsigned i = 0; i < rank; ++i) {
        if (memRefType.isDynamicDim(i)) {
          elementSizes.push_back(
              builder.create<arts::DbDimOp>(loc, allocValue, (int64_t)i));
        } else {
          elementSizes.push_back(builder.create<arith::ConstantIndexOp>(
              loc, memRefType.getDimSize(i)));
        }
      }

      /// Single datablock
      sizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
    }

    /// By default, use zero route
    auto route = builder.create<arith::ConstantIntOp>(loc, 0, 32);

    /// Create the db_alloc operation
    auto dbAllocOp = builder.create<DbAllocOp>(
        loc, mode, route, allocType, dbMode, elementType, sizes, elementSizes);

    /// Store mappings for later use
    memrefInfo.dbAllocOp = dbAllocOp;
    dbPtrToOriginalAlloc[dbAllocOp] = alloc;

    /// Insert DbFreeOp and remove associated dealloc operations
    insertDbFreeForDbAlloc(dbAllocOp, alloc, builder);

    /// Rewire uses of the original allocation to use the db_alloc operation
    rewriteUsesInParentEdt(memrefInfo);
  }
}

//===----------------------------------------------------------------------===//
// Process EDT Dependencies
/// For each EDT, analyze external memory dependencies and insert
/// acquire/release
//===----------------------------------------------------------------------===//
void CreateDbsPass::createDbAcquireOps(EdtOp edt,
                                       SetVector<Value> &externalDeps) {
  OpBuilder builder(module.getContext());
  ARTS_DEBUG(" - Creating DbAcquire operations for "
             << externalDeps.size() << " external dependencies");

  /// For each external value, create acquire and release operations
  for (Value externalDep : externalDeps) {
    /// Locate the underlying operation
    ARTS_DEBUG(" - Getting underlying operation for " << externalDep);
    Operation *underlyingOp = arts::getUnderlyingOperation(externalDep);
    if (!underlyingOp) {
      ARTS_ERROR("Value with no underlying operation: " << externalDep);
      assert(false && "Value with no underlying operation");
      continue;
    }

    /// Get the memref info for the underlying operation
    MemrefInfo &info = memrefInfo[underlyingOp];
    DbAllocOp dbAllocOp = info.dbAllocOp;
    if (!dbAllocOp) {
      ARTS_ERROR("DbAllocOp not found for: " << *underlyingOp);
      assert(false && "DbAllocOp not found for underlying operation");
      continue;
    }

    /// Get the source guid and ptr
    auto sourceGuid = dbAllocOp.getGuid();
    auto sourcePtr = dbAllocOp.getPtr();
    assert((sourceGuid && sourcePtr) && "Source guid and ptr must be non-null");

    /// Create acquire operation (pass both source guid and ptr)
    builder.setInsertionPoint(edt);

    /// We use fine-grained acquire if the allocation was also fine-grained
    if (info.usedFineGrained) {
      /// Fine-grained acquire: Multiple separate acquires for stencil patterns
      /// Example: A stencil accessing u[i-1], u[i], u[i+1] creates 3 acquires
      const auto &deps = info.edtToDeps[edt];

      /// Combine access modes across all deps (e.g., all 'in' for inputs)
      ArtsMode combinedMode = deps[0].mode;
      for (size_t i = 1; i < deps.size(); ++i)
        combinedMode = combineAccessModes(combinedMode, deps[i].mode);

      /// Create an acquire for each dependency
      ARTS_DEBUG(" - Creating " << deps.size() << " fine-grained acquires");

      for (const auto &dep : deps) {
        const auto &depIndices = dep.indices;

        /// For fine-grained element-level access: split indices between
        /// acquire.indices and acquire.offsets/sizes
        SmallVector<Value> acquireIndices, acquireOffsets, acquireSizes;
        acquireIndices.assign(depIndices.begin(), depIndices.end());
        auto acquireOp = builder.create<DbAcquireOp>(
            edt.getLoc(), dep.mode, sourceGuid, sourcePtr, acquireIndices,
            acquireOffsets, acquireSizes);

        /// Add each acquire as a separate EDT dependency and update operands
        SmallVector<Value> newOperands;
        if (Value route = edt.getRoute())
          newOperands.push_back(route);
        auto existingDeps = edt.getDependenciesAsVector();
        newOperands.append(existingDeps.begin(), existingDeps.end());
        newOperands.push_back(acquireOp.getPtr());
        edt->setOperands(newOperands);

        /// Add corresponding block argument for each acquired view
        auto sourceType = sourcePtr.getType().dyn_cast<MemRefType>();
        BlockArgument dbAcquireArg =
            edt.getBody().front().addArgument(sourceType, edt.getLoc());

        /// Rewrite uses in edt
        rewriteUsesInEdt(edt, externalDep, acquireOp, acquireIndices,
                         dbAcquireArg);

        /// Insert release for this acquire before EDT terminator
        OpBuilder::InsertionGuard IG(builder);
        builder.setInsertionPoint(edt.getBody().front().getTerminator());
        builder.create<DbReleaseOp>(edt.getLoc(), dbAcquireArg);
      }
      /// We've handled everything for this memref, continue to next
      continue;
    } else {
      /// Coarse-grained acquire: Acquire entire DB as a single block
      ARTS_DEBUG(" - Using coarse-grained acquire");

      SmallVector<Value> acquireIndices, acquireOffsets, acquireSizes;
      acquireOffsets.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), 0));
      acquireSizes.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), 1));

      /// Use the dependency mode from DbControlOp if available
      ArtsMode acquireMode = ArtsMode::inout;
      auto acquireOp = builder.create<DbAcquireOp>(
          edt.getLoc(), acquireMode, sourceGuid, sourcePtr, acquireIndices,
          acquireOffsets, acquireSizes);

      /// Add acquire pointer as EDT dependency
      SmallVector<Value> newOperands;
      if (Value route = edt.getRoute())
        newOperands.push_back(route);
      auto existingDeps = edt.getDependenciesAsVector();
      newOperands.append(existingDeps.begin(), existingDeps.end());
      newOperands.push_back(acquireOp.getPtr());
      edt->setOperands(newOperands);

      /// Add corresponding block argument for the acquired view
      auto sourceType = sourcePtr.getType().dyn_cast<MemRefType>();
      BlockArgument dbAcquireArg =
          edt.getBody().front().addArgument(sourceType, edt.getLoc());

      /// Rewrite uses in edt
      rewriteUsesInEdt(edt, externalDep, acquireOp, acquireIndices,
                       dbAcquireArg);

      /// Insert release for this acquire before EDT terminator
      OpBuilder::InsertionGuard IG(builder);
      builder.setInsertionPoint(edt.getBody().front().getTerminator());
      builder.create<DbReleaseOp>(edt.getLoc(), dbAcquireArg);
    }
  }
}

//===----------------------------------------------------------------------===//
// Insert DB Free Operations
/// Insert db_free for DbAllocOp at the appropriate location and remove
/// associated dealloc operations
//===----------------------------------------------------------------------===//
void CreateDbsPass::insertDbFreeForDbAlloc(DbAllocOp dbAlloc, Operation *alloc,
                                           OpBuilder &builder) {
  Location loc = dbAlloc.getLoc();
  Value allocResult = alloc->getResult(0);

  /// Find associated dealloc operation
  std::optional<Operation *> deallocOp = memref::findDealloc(allocResult);
  if (deallocOp.has_value() && *deallocOp) {
    ARTS_DEBUG("Found dealloc operation for alloc: " << *deallocOp);
    opsToRemove.insert(*deallocOp);
  }

  /// Determine where to insert DbFreeOp based on where dbAlloc is located
  Operation *insertionPoint = nullptr;

  /// Check if dbAlloc is within an EDT
  if (EdtOp parentEdt = dbAlloc->getParentOfType<EdtOp>()) {
    /// Insert before EDT terminator
    Block &edtBlock = parentEdt.getBody().front();
    insertionPoint = edtBlock.getTerminator();

  } else {
    /// Insert at the end of the block containing dbAlloc to ensure dominance
    Block *allocBlock = dbAlloc->getBlock();
    insertionPoint = allocBlock->getTerminator();
    ARTS_DEBUG("Inserting DbFreeOp before block terminator "
               << *insertionPoint);
  }

  /// Insert DbFreeOp if we found a valid insertion point
  assert(insertionPoint && "Could not find insertion point for DbFreeOp");
  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(insertionPoint);
  builder.create<DbFreeOp>(loc, dbAlloc.getGuid());
  builder.create<DbFreeOp>(loc, dbAlloc.getPtr());
}

//===----------------------------------------------------------------------===//
/// Rewrite Operation Helper
/// Rewrite a single operation to use DbRefOp pattern
//===----------------------------------------------------------------------===//
bool CreateDbsPass::rewriteOperation(Operation *op, Type elementMemRefType,
                                     Value basePtr, Operation *dbOp,
                                     OpBuilder &builder,
                                     uint64_t initialIndex) {
  /// Dbref for outer indices and new load for inner indices
  if (auto load = dyn_cast<memref::LoadOp>(op)) {
    Location loc = load.getLoc();
    builder.setInsertionPoint(load);
    SmallVector<Value> indices(load.getIndices().begin() + initialIndex,
                               load.getIndices().end());
    auto [outerIdx, innerIdx] =
        arts::splitDbIndices(dbOp, indices, builder, loc);
    auto dbRef =
        builder.create<DbRefOp>(loc, elementMemRefType, basePtr, outerIdx);
    auto newLoad = builder.create<memref::LoadOp>(loc, dbRef, innerIdx);
    load.replaceAllUsesWith(newLoad.getResult());
    opsToRemove.insert(load);
    return true;
  }

  /// Dbref for outer indices and new store for inner indices
  if (auto store = dyn_cast<memref::StoreOp>(op)) {
    Location loc = store.getLoc();
    builder.setInsertionPoint(store);
    SmallVector<Value> indices(store.getIndices().begin() + initialIndex,
                               store.getIndices().end());
    auto [outerIdx, innerIdx] =
        arts::splitDbIndices(dbOp, indices, builder, loc);

    auto dbRef =
        builder.create<DbRefOp>(loc, elementMemRefType, basePtr, outerIdx);
    builder.create<memref::StoreOp>(loc, store.getValueToStore(), dbRef,
                                    innerIdx);
    opsToRemove.insert(store);
    return true;
  }

  /// Memref2PointerOp loads the element 0 of the acquired memref and creates
  /// a new DbRefOp
  if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(op)) {
    Location loc = m2p.getLoc();
    builder.setInsertionPoint(m2p);
    SmallVector<Value> indicesConst0;
    indicesConst0.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
    auto dbRef =
        builder.create<DbRefOp>(loc, elementMemRefType, basePtr, indicesConst0);
    auto newM2p = builder.create<polygeist::Memref2PointerOp>(
        loc, m2p.getResult().getType(), dbRef.getResult());
    m2p.getResult().replaceAllUsesWith(newM2p.getResult());
    opsToRemove.insert(m2p);
    return true;
  }

  return false;
}

//===----------------------------------------------------------------------===//
/// Rewrite Alloc uses
/// Adjust load/store indices based on DB allocation granularity.
//===----------------------------------------------------------------------===//
void CreateDbsPass::rewriteUsesInParentEdt(MemrefInfo &memrefInfo) {
  assert(memrefInfo.dbAllocOp && "No DbAllocOp found");

  auto dbAlloc = memrefInfo.dbAllocOp;
  Type elementMemRefType = dbAlloc.getAllocatedElementType().getElementType();

  /// Collect users in the parent edt
  SmallVector<Operation *, 8> users;
  for (auto &use : memrefInfo.alloc->getUses()) {
    Operation *user = use.getOwner();
    if (user->getParentOfType<EdtOp>() == memrefInfo.parentEdt)
      users.push_back(user);
  }

  ARTS_DEBUG(" - Rewriting " << users.size() << " operations in parent EDT");

  OpBuilder builder(module.getContext());
  for (Operation *user : users) {
    rewriteOperation(user, elementMemRefType, dbAlloc.getPtr(),
                     dbAlloc.getOperation(), builder);
  }
}

//===----------------------------------------------------------------------===//
// Rewrite Uses in EDT
/// Rewrite loads/stores in an EDT to use acquired datablocks
/// Finds all operations that use external values and match the acquire indices
//===----------------------------------------------------------------------===//
void CreateDbsPass::rewriteUsesInEdt(EdtOp edt, Value originalValue,
                                     DbAcquireOp dbAcquire,
                                     SmallVector<Value> &acquireIndices,
                                     BlockArgument dbAcquireArg) {
  /// Get the DbAllocOp for index splitting
  Operation *underlyingOp = arts::getUnderlyingOperation(originalValue);
  assert(underlyingOp && "Underlying operation not found");

  DbAllocOp dbAlloc = memrefInfo[underlyingOp].dbAllocOp;
  assert(dbAlloc && "No DbAllocOp found for underlying operation");
  Type elementMemRefType = dbAlloc.getAllocatedElementType().getElementType();

  /// Helper function to check if indices have acquire indices as prefix
  auto indicesMatchPrefix = [&](ValueRange opIndices) -> bool {
    /// If the operation indices are less than the acquire indices, they cannot
    /// match
    if (opIndices.size() < acquireIndices.size())
      return false;
    /// Check if the operation indices match the acquire indices
    return std::equal(acquireIndices.begin(), acquireIndices.end(),
                      opIndices.begin());
  };

  /// Collect operations to rewrite by iterating over uses of external values
  SmallVector<Operation *, 16> opsToRewrite;

  /// Iterate over all uses of the original value
  for (Operation *user : originalValue.getUsers()) {
    /// Check that the operation's parent EDT matches the current EDT
    EdtOp parentEdt = user->getParentOfType<EdtOp>();
    if (parentEdt != edt)
      continue;

    /// Check memref.load operations
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      Value underlying = arts::getUnderlyingValue(load.getMemref());

      /// Only process if it matches the original value
      if (underlying == originalValue) {
        if (indicesMatchPrefix(load.getIndices()))
          opsToRewrite.push_back(user);
      }
    }
    /// Check memref.store operations
    else if (auto store = dyn_cast<memref::StoreOp>(user)) {
      Value underlying = arts::getUnderlyingValue(store.getMemref());

      /// Only process if it matches the original value
      if (underlying == originalValue) {
        if (indicesMatchPrefix(store.getIndices()))
          opsToRewrite.push_back(user);
      }
    }
    /// Check polygeist::Memref2PointerOp operations
    else if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(user)) {
      Value underlying = arts::getUnderlyingValue(m2p.getSource());

      /// Only process if it matches the original value
      if (underlying == originalValue && acquireIndices.empty())
        opsToRewrite.push_back(user);
    }
  }

  ARTS_DEBUG(" - Rewriting " << opsToRewrite.size() << " operations in EDT");

  /// Rewrite each operation with DbRefOp pattern
  OpBuilder builder(edt.getContext());
  for (Operation *user : opsToRewrite) {
    rewriteOperation(user, elementMemRefType, dbAcquireArg,
                     dbAcquire.getOperation(), builder, acquireIndices.size());
  }
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
