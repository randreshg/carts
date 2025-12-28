///==========================================================================///
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

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Transforms/Datablock/DbChunkedRewriter.h"
#include "arts/Transforms/Datablock/DbElementWiseRewriter.h"
#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Transforms/DbTransforms.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include <optional>

#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(create_dbs);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Helper Functions
///===----------------------------------------------------------------------===///
namespace {

///===----------------------------------------------------------------------===///
// Pass Implementation
///===----------------------------------------------------------------------===///

struct CreateDbsPass : public arts::CreateDbsBase<CreateDbsPass> {
  CreateDbsPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
  ArtsCodegen *AC = nullptr;
  DenseMap<Operation *, Operation *> dbPtrToOriginalAlloc;
  SetVector<Operation *> opsToRemove;

  /// Consolidated structure to track all usage information for a memref
  struct MemrefInfo {
    Operation *alloc = nullptr;
    EdtOp parentEdt = nullptr;
    ArtsMode accessMode = ArtsMode::uninitialized;
    bool usedFineGrained = false;
    bool usedChunked = false;
    SmallVector<Value> chunkSizes;
    DbAllocOp dbAllocOp = nullptr;

    /// The computed allocation strategy plan
    std::optional<DbRewritePlan> rewritePlan;

    /// Structure to track a specific DB access (mode + indices + chunks)
    struct DbDep {
      ArtsMode mode;
      SmallVector<Value, 4> indices, offsets, sizes;
      SmallVector<Operation *, 4> operations;
    };

    /// Map from EDT operation to all DB accesses for that EDT
    DenseMap<EdtOp, SmallVector<DbDep>> edtToDeps;

    /// Add an access for this memref and update combined mode
    void addAccess(EdtOp edtOp, ArtsMode mode, SmallVector<Value, 4> indices,
                   SmallVector<Value, 4> offsets = {},
                   SmallVector<Value, 4> sizes = {},
                   SmallVector<Operation *, 4> operations = {}) {
      accessMode = combineAccessModes(accessMode, mode);
      edtToDeps[edtOp].push_back({mode, indices, offsets, sizes, operations});
    }

    /// Compute index pattern information from stored accesses
    struct AccessPatternInfo {
      /// Whether all accesses have the same number of indices
      bool isConsistent = true;
      /// Whether all accesses have indices
      bool allAccessesHaveIndices = true;
      /// Minimum number of pinned dimensions across all accesses
      unsigned pinnedDimCount = 0;

      /// Chunk dependency tracking (from DbControlOp.offsets/sizes)
      bool hasChunkDeps = false;     /// Any deps have offsets/sizes?
      unsigned chunkDimCount = 0;    /// Number of chunk dimensions
      SmallVector<Value> chunkSizes; /// SSA values for chunk sizes
      bool chunkSizesAreConsistent =
          true; /// All deps use same chunk size count?
    };

    AccessPatternInfo getAccessPatternInfo() const {
      AccessPatternInfo info;
      std::optional<unsigned> expectedDimCount;
      bool hasPinnedAccess = false;

      for (const auto &[edtOp, accesses] : edtToDeps) {
        for (const auto &access : accesses) {
          /// If no operations matched this dependency (SSA value mismatch),
          /// fallback to coarse-grained allocation
          if (access.operations.empty()) {
            info.isConsistent = false;
            info.allAccessesHaveIndices = false;
            continue;
          }

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

          /// Track chunk dependencies from DbControlOp.offsets/sizes
          /// Chunk deps have: indices=[], offsets=[offset], sizes=[chunkSize]
          if (!access.offsets.empty() && !access.sizes.empty()) {
            info.hasChunkDeps = true;

            if (info.chunkSizes.empty()) {
              /// First chunk dep - record the sizes
              info.chunkSizes.assign(access.sizes.begin(), access.sizes.end());
              info.chunkDimCount = access.sizes.size();
            } else {
              /// Subsequent chunk deps - check consistency (dimension count)
              /// We can't compare SSA values statically, so we just check count
              if (access.sizes.size() != info.chunkDimCount) {
                info.chunkSizesAreConsistent = false;
              }
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

  /// Helper functions
  void collectMemrefs();
  void collectControlDbOps();
  void createDbAllocOps();

  /// Helper functions for collectControlDbOps
  void createDbAcquireOps(EdtOp edt, SetVector<Value> &externalDeps);

  /// Checks if the parent EDT already has an acquired handle for the given
  /// DbAllocOp.
  Value findParentAcquireSource(EdtOp edt, Operation *dbAllocOp,
                                bool requiresIndexedAccess = false);

  /// Infer allocation type from the defining operation
  DbAllocType inferAllocType(Operation *alloc);

  /// Insert DbFreeOp for a DbAllocOp at the appropriate location
  void insertDbFreeForDbAlloc(DbAllocOp dbAlloc, Operation *alloc);

  /// Rewrite uses of datablocks
  void rewriteOpsToUseDbAcquire(EdtOp edt, SmallVector<Operation *> &operations,
                                Operation *dbOp,
                                SmallVector<Value> &acquireIndices,
                                SmallVector<Value> &acquireOffsets,
                                BlockArgument dbAcquireArg,
                                const DbRewritePlan &plan);
  void rewriteUsesInParentEdt(MemrefInfo &memrefInfo);
  void rewriteUsesEverywhereCoarse(Operation *alloc, DbAllocOp dbAlloc);

  ///===----------------------------------------------------------------------===///
  /// Helper Functions for Heuristic Decisions
  ///===----------------------------------------------------------------------===///

  /// Computes the number of outer DBs for fine-grained allocation.
  /// Returns the product of pinned dimensions, or kMaxOuterDBs+1 if dynamic.
  static int64_t computeOuterDBs(MemRefType memRefType,
                                 unsigned pinnedDimCount) {
    int64_t outerDBs = 1;
    for (unsigned d = 0; d < pinnedDimCount; ++d) {
      /// Conservative
      if (memRefType.isDynamicDim(d))
        return HeuristicsConfig::kMaxOuterDBs + 1;
      outerDBs *= memRefType.getDimSize(d);
    }
    return outerDBs;
  }

  /// Computes maximum dependencies per EDT for cost model evaluation.
  static int64_t computeMaxDepsPerEDT(const MemrefInfo &info) {
    int64_t maxDeps = 0;
    for (const auto &[edt, deps] : info.edtToDeps)
      maxDeps = std::max(maxDeps, static_cast<int64_t>(deps.size()));
    return maxDeps;
  }

  /// Computes total inner bytes (product of unpinned dimensions * element
  /// size).
  static int64_t computeInnerBytes(MemRefType memRefType,
                                   unsigned pinnedDimCount) {
    int64_t innerBytes = 1;
    Type elementType = memRefType.getElementType();
    while (auto nested = elementType.dyn_cast<MemRefType>())
      elementType = nested.getElementType();
    int64_t elemSize = elementType.isIntOrFloat()
                           ? (elementType.getIntOrFloatBitWidth() + 7) / 8
                           : 8;

    for (unsigned d = pinnedDimCount; d < memRefType.getRank(); ++d) {
      if (memRefType.isDynamicDim(d))
        return HeuristicsConfig::kMinInnerBytes + 1; /// Assume large enough
      innerBytes *= memRefType.getDimSize(d);
    }
    return innerBytes * elemSize;
  }
};
} // namespace

///===----------------------------------------------------------------------===///
// Pass Entry Point
///===----------------------------------------------------------------------===///
void CreateDbsPass::runOnOperation() {
  module = getOperation();
  opsToRemove.clear();
  memrefInfo.clear();

  /// Initialize ArtsCodegen
  AC = new ArtsCodegen(module, false);

  ARTS_INFO_HEADER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Phase 1: Collect memrefs used in EDTs
  ARTS_INFO("Phase 1: Collecting memrefs used in EDTs");
  collectMemrefs();
  ARTS_DEBUG(" - Found " << memrefInfo.size() << " memrefs used in EDTs");

  /// Phase 2: collect and clean up existing Control DB operations
  ARTS_INFO("Phase 2: collect and clean up existing Control DB operations");
  collectControlDbOps();

  /// Phase 3.5: Check for memrefs used without DbControlOps
  /// For each EDT with external dependencies, check if any memref is used
  /// without a corresponding DbControlOp. If so, set that memref to inout.
  for (auto &edtEntry : edtExternalValues) {
    EdtOp edt = edtEntry.first;
    SetVector<Value> &externalDeps = edtEntry.second;

    for (Value externalDep : externalDeps) {
      Operation *underlyingOp = ValueUtils::getUnderlyingOperation(externalDep);
      if (!underlyingOp)
        continue;

      MemrefInfo &info = memrefInfo[underlyingOp];

      /// Check if this EDT has any DbControlOps for this memref
      bool hasControlDb = !info.edtToDeps[edt].empty();
      if (!hasControlDb) {
        /// No DbControlOps for this memref in this EDT - set to inout
        ARTS_DEBUG(
            " - Memref "
            << *underlyingOp
            << " used in EDT without DbControlOps, setting to inout mode");
        info.accessMode = combineAccessModes(info.accessMode, ArtsMode::inout);
      }
    }
  }

  /// Also handle any memrefs that remain completely uninitialized
  for (auto &entry : memrefInfo) {
    MemrefInfo &info = entry.second;
    if (info.accessMode == ArtsMode::uninitialized) {
      ARTS_DEBUG(" - Memref "
                 << *entry.first
                 << " has no DbControlOps at all, defaulting to inout mode");
      info.accessMode = ArtsMode::inout;
    }
  }

  /// Phase 3: Create DbAlloc operations
  ARTS_INFO("Phase 3: Creating DbAlloc operations for " << memrefInfo.size()
                                                        << " memrefs");
  createDbAllocOps();

  /// Phase 4: Process EDTs for dependencies
  /// Use pre-order walk to naturally visit parents before children.
  ARTS_INFO("Phase 4: Creating DbAcquire operations for "
            << edtExternalValues.size() << " EDTs");

  module.walk<WalkOrder::PreOrder>([&](EdtOp edt) {
    auto it = edtExternalValues.find(edt);
    if (it != edtExternalValues.end())
      createDbAcquireOps(edt, it->second);
  });

  /// Phase 5: Clean up legacy allocations
  ARTS_INFO("Phase 5: Cleaning up legacy allocations");
  RemovalUtils removalMgr;
  for (Operation *op : opsToRemove)
    removalMgr.markForRemoval(op);
  removalMgr.removeAllMarked(module, /*recursive=*/true);

  /// Phase 6: Enabling verification for EDTs
  module.walk([](EdtOp edt) { edt.removeNoVerifyAttr(); });

  /// Phase 7: Simplify the module
  DominanceInfo domInfo(module);
  arts::simplifyIR(module, domInfo);

  /// Cleanup ArtsCodegen
  delete AC;
  AC = nullptr;

  ARTS_INFO_FOOTER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
// Allocation Type Inference
///===----------------------------------------------------------------------===///
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

///===----------------------------------------------------------------------===///
// Collect Allocations Used in EDTs
///===----------------------------------------------------------------------===///
void CreateDbsPass::collectMemrefs() {
  memrefInfo.clear();
  module.walk([&](EdtOp edt) {
    edt.walk([&](Operation *op) {
      /// Skip self-references
      if (auto edtOp = dyn_cast<EdtOp>(op)) {
        if (edtOp == edt)
          return;
      }

      /// Check all operands of all operations for memory references
      for (Value operand : op->getOperands()) {
        if (!operand.getType().isa<MemRefType>())
          continue;

        /// Get the underlying value of the memory reference
        Operation *underlyingOp = ValueUtils::getUnderlyingOperation(operand);
        if (!underlyingOp) {
          if (auto load = operand.getDefiningOp<memref::LoadOp>())
            underlyingOp = ValueUtils::getUnderlyingOperation(load.getMemref());
        }
        if (!underlyingOp) {
          ARTS_WARN("Skipping value with no underlying operation: " << operand);
          continue;
        }

        /// If it is found, check the parent edt
        EdtOp parentEdt = underlyingOp->getParentOfType<EdtOp>();

        /// Create memref info for the underlying operation
        MemrefInfo &info = memrefInfo[underlyingOp];
        info.alloc = underlyingOp;
        info.parentEdt = parentEdt;

        /// If the parent edt is different from the current edt, it means
        /// it is an external dependency
        if (parentEdt != edt)
          edtExternalValues[edt].insert(operand);
      }
    });
  });
}

///===----------------------------------------------------------------------===///
// Collect Control DB Operations
///===----------------------------------------------------------------------===///
void CreateDbsPass::collectControlDbOps() {
  /// First, extract dependency mode and index information from all db_control
  /// ops before erasing them
  SmallVector<DbControlOp, 4> dbControlOps;
  module.walk([&](DbControlOp dbControl) {
    dbControlOps.push_back(dbControl);
    Value ptr = dbControl.getPtr();
    ArtsMode mode = dbControl.getMode();
    auto indices = dbControl.getIndices();
    auto offsets = dbControl.getOffsets();
    auto sizes = dbControl.getSizes();

    /// Get the underlying operation for this pointer
    Operation *underlyingOp = ValueUtils::getUnderlyingOperation(ptr);
    if (!underlyingOp)
      return;

    ARTS_DEBUG(" - Found DbControlOp for "
               << *underlyingOp << " with mode: " << mode << ", "
               << indices.size() << " indices, " << offsets.size()
               << " offsets, " << sizes.size() << " sizes");

    /// Get user EDT
    EdtOp userEdt = nullptr;
    for (Operation *user : dbControl.getResult().getUsers()) {
      if (auto edt = dyn_cast<EdtOp>(user)) {
        userEdt = edt;
        break;
      }
    }

    SmallVector<Value, 4> indexValues(indices.begin(), indices.end());
    SmallVector<Value, 4> offsetValues(offsets.begin(), offsets.end());
    SmallVector<Value, 4> sizeValues(sizes.begin(), sizes.end());

    /// Track all operations in this EDT that use this memref AND match the
    /// indices
    SmallVector<Operation *, 4> opsToRewrite;
    if (userEdt) {
      /// Helper to check if operation indices match DbControlOp using SSA value
      /// equality This is strict but correct for distinguishing multiple chunks
      /// (e.g., A[i-1], A[i], A[i+1])
      auto indicesMatchPrefix = [&](ValueRange opIndices) -> bool {
        /// If operation has fewer indices than pinned dimensions, can't match
        if (opIndices.size() < indexValues.size())
          return false;

        /// Check if the first N indices match the pinned indices (N =
        /// indexValues.size()) using SSA value equality
        return std::equal(indexValues.begin(), indexValues.end(),
                          opIndices.begin());
      };

      /// Walk the user EDT and collect operations that match the indices
      userEdt.walk([&](Operation *op) {
        if (auto load = dyn_cast<memref::LoadOp>(op)) {
          if (ValueUtils::getUnderlyingOperation(load.getMemref()) ==
              underlyingOp) {
            if (indicesMatchPrefix(load.getIndices()))
              opsToRewrite.push_back(op);
          }
        } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
          if (ValueUtils::getUnderlyingOperation(store.getMemref()) ==
              underlyingOp) {
            if (indicesMatchPrefix(store.getIndices()))
              opsToRewrite.push_back(op);
          }
        }
      });
      ARTS_DEBUG("    - Found "
                 << opsToRewrite.size()
                 << " operations to rewrite for this specific dependency");
    }

    memrefInfo[underlyingOp].addAccess(userEdt, mode, indexValues, offsetValues,
                                       sizeValues, opsToRewrite);
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

///===----------------------------------------------------------------------===///
// Create DB Allocation Operations
///===----------------------------------------------------------------------===///
void CreateDbsPass::createDbAllocOps() {
  /// All memrefs in memrefInfo are converted to DBs
  for (auto &entry : memrefInfo) {
    Operation *alloc = entry.first;
    ARTS_DEBUG("Creating DB Alloc Op for memref " << *alloc);
    MemrefInfo &info = entry.second;
    Location loc = alloc->getLoc();
    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPointAfter(alloc);

    /// Infer allocation type based on the defining operation
    DbAllocType allocType = inferAllocType(alloc);

    /// Use the dependency mode from DbControlOp
    ArtsMode mode = info.accessMode;
    ARTS_DEBUG(" - Using access mode " << mode);

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

    /// Determine allocation granularity using unified heuristics
    SmallVector<Value> sizes, elementSizes;
    const bool isRankZero = memRefType.getRank() == 0;
    const unsigned rank = std::max<unsigned>(1, memRefType.getRank());
    const auto accessPatternInfo = info.getAccessPatternInfo();

    auto &heuristics = AM->getHeuristicsConfig();

    /// Build PartitioningContext for unified heuristic evaluation
    PartitioningContext ctx;
    ctx.machine = &AM->getAbstractMachine();
    ctx.memrefRank = memRefType.getRank();
    ctx.canElementWise = !isRankZero && accessPatternInfo.isConsistent &&
                         accessPatternInfo.pinnedDimCount > 0 &&
                         accessPatternInfo.allAccessesHaveIndices;
    ctx.canChunked = !isRankZero && accessPatternInfo.hasChunkDeps &&
                     accessPatternInfo.chunkSizesAreConsistent &&
                     accessPatternInfo.chunkDimCount > 0;
    ctx.pinnedDimCount = accessPatternInfo.pinnedDimCount;
    ctx.accessMode = info.accessMode;
    ctx.isUniformAccess = accessPatternInfo.isConsistent;
    ctx.depsPerEDT = computeMaxDepsPerEDT(info);

    /// Extract static chunk size for cost model if available
    if (ctx.canChunked && !accessPatternInfo.chunkSizes.empty()) {
      if (auto cst = accessPatternInfo.chunkSizes[0]
                         .getDefiningOp<arith::ConstantIndexOp>())
        ctx.chunkSize = cst.value();
    }

    /// Compute total elements for cost model if statically known
    int64_t totalElements = 1;
    bool allStaticDims = true;
    for (unsigned d = 0; d < memRefType.getRank(); ++d) {
      if (memRefType.isDynamicDim(d)) {
        allStaticDims = false;
        break;
      }
      totalElements *= memRefType.getDimSize(d);
    }
    if (allStaticDims)
      ctx.totalElements = totalElements;

    /// Get partitioning decision from unified heuristics
    /// The decision is FINAL - heuristics already respect capabilities
    PartitioningDecision decision = heuristics.getPartitioningMode(ctx);

    ARTS_DEBUG(" - Heuristic decision: " << decision.rationale);
    ARTS_DEBUG("   mode=" << (decision.isChunked() ? "Chunked" : "ElementWise")
                          << ", outerRank=" << decision.outerRank
                          << ", innerRank=" << decision.innerRank);

    /// Build sizes/elementSizes based on the decision
    if (decision.isChunked()) {
      info.usedFineGrained = true;
      info.usedChunked = true;
      info.chunkSizes.assign(accessPatternInfo.chunkSizes.begin(),
                             accessPatternInfo.chunkSizes.end());

      /// Chunked allocation: divide first dimension into chunks
      /// numChunks = ceildiv(dim0, chunkSize)
      /// sizes = [numChunks], elementSizes = [chunkSize, remaining dims]
      Value chunkSize = accessPatternInfo.chunkSizes[0];
      Value dim0;
      if (memRefType.isDynamicDim(0)) {
        dim0 = AC->create<arts::DbDimOp>(loc, allocValue, (int64_t)0);
      } else {
        dim0 = AC->createIndexConstant(memRefType.getDimSize(0), loc);
      }
      Value numChunks = AC->create<arith::CeilDivUIOp>(loc, dim0, chunkSize);
      sizes.push_back(numChunks);
      elementSizes.push_back(chunkSize);

      /// Remaining dimensions go to elementSizes
      for (unsigned i = 1; i < rank; ++i) {
        if (memRefType.isDynamicDim(i)) {
          elementSizes.push_back(
              AC->create<arts::DbDimOp>(loc, allocValue, (int64_t)i));
        } else {
          elementSizes.push_back(
              AC->createIndexConstant(memRefType.getDimSize(i), loc));
        }
      }

      ARTS_DEBUG(" - Using chunked allocation: numChunks computed at runtime, "
                 << "elementSizes has " << elementSizes.size() << " dims");
    } else if (decision.isFineGrained()) {
      info.usedFineGrained = true;
      const unsigned pinnedDimCount = decision.outerRank;

      /// Fine-grained element-wise allocation:
      /// All indexed dimensions go into sizes (outer array dimensions)
      /// Non-indexed dimensions go into elementSizes (inner datablock dims)
      ARTS_DEBUG(" - Using fine-grained allocation with "
                 << pinnedDimCount << " pinned dimensions");

      /// Add all pinned dimensions to sizes
      for (unsigned dim = 0; dim < pinnedDimCount; ++dim) {
        if (memRefType.isDynamicDim(dim)) {
          sizes.push_back(
              AC->create<arts::DbDimOp>(loc, allocValue, (int64_t)dim));
        } else {
          sizes.push_back(
              AC->createIndexConstant(memRefType.getDimSize(dim), loc));
        }
      }

      /// All non-pinned dimensions go into elementSizes
      for (unsigned i = pinnedDimCount; i < rank; ++i) {
        if (memRefType.isDynamicDim(i)) {
          elementSizes.push_back(
              AC->create<arts::DbDimOp>(loc, allocValue, (int64_t)i));
        } else {
          elementSizes.push_back(
              AC->createIndexConstant(memRefType.getDimSize(i), loc));
        }
      }

      ARTS_DEBUG("   - Created fine-grained DB: Outer rank="
                 << sizes.size() << ", Inner rank=" << elementSizes.size());
    } else {
      /// Coarse-grained: all dimensions go to elementSizes, single DB. For
      /// rank-0 memrefs, model them as a 1-D memref of size 1.
      ARTS_DEBUG(" - Using coarse-grained allocation");
      elementSizes.reserve(rank);
      for (unsigned i = 0; i < rank; ++i) {
        if (!isRankZero && memRefType.isDynamicDim(i)) {
          elementSizes.push_back(
              AC->create<arts::DbDimOp>(loc, allocValue, (int64_t)i));
        } else {
          int64_t dimSize =
              isRankZero ? 1 : static_cast<int64_t>(memRefType.getDimSize(i));
          elementSizes.push_back(AC->createIndexConstant(dimSize, loc));
        }
      }

      /// Single datablock
      sizes.push_back(AC->createIndexConstant(1, loc));
    }

    /// Create the db_alloc operation
    auto route = AC->createIntConstant(0, AC->Int32, loc);
    auto dbAllocOp = AC->create<DbAllocOp>(loc, mode, route, allocType, dbMode,
                                           elementType, sizes, elementSizes);

    /// Set partition attribute based on heuristic decision
    PromotionMode promotionMode = decision.isChunked() ? PromotionMode::chunked
                                  : decision.isFineGrained()
                                      ? PromotionMode::element_wise
                                      : PromotionMode::none;
    setPartitionMode(dbAllocOp, promotionMode);
    ARTS_DEBUG(
        " - Set partition attribute: " << static_cast<int>(promotionMode));

    /// Transfer metadata from original allocation to DbAllocOp
    if (AM->getMetadataManager().transferMetadata(alloc, dbAllocOp)) {
      ARTS_DEBUG(
          "  Transferred metadata from original allocation to DbAllocOp");
    } else {
      ARTS_DEBUG("  No metadata found for original allocation");
    }

    /// Create and store the rewrite plan directly from heuristic decision
    DbRewritePlan plan(decision);
    if (decision.isChunked())
      plan.chunkSize = accessPatternInfo.chunkSizes[0];
    info.rewritePlan = plan;

    /// Record allocation strategy decision for diagnostics
    std::string modeName =
        plan.mode == RewriterMode::Chunked
            ? "Chunked"
            : (plan.outerRank > 0 ? "FineGrained" : "Coarse");
    heuristics.recordDecision(
        "AllocationStrategy", true, "Selected " + modeName + " allocation mode",
        alloc,
        {{"outerRank", static_cast<int64_t>(plan.outerRank)},
         {"innerRank", static_cast<int64_t>(plan.innerRank)}});

    /// Store mappings for later use
    info.dbAllocOp = dbAllocOp;
    dbPtrToOriginalAlloc[dbAllocOp] = alloc;

    /// Insert DbFreeOp and remove associated dealloc operations
    insertDbFreeForDbAlloc(dbAllocOp, alloc);

    /// Rewire uses of the original allocation to use the db_alloc operation.
    /// If the allocation has a defining EDT, rewrite within that EDT. If it
    /// lives outside any EDT (host/global), rewrite all uses so host code sees
    /// the datablock contents as well.
    if (info.parentEdt) {
      rewriteUsesInParentEdt(info);
    } else {
      /// Use stored plan for parent region rewrites
      Value startChunk = AC->createIndexConstant(0, loc);
      Value elemOffset = AC->createIndexConstant(0, loc);
      Value elemSize = AC->createIndexConstant(1, loc);

      if (plan.mode == RewriterMode::Chunked) {
        DbChunkedRewriter rewriter(plan.chunkSize, startChunk, elemOffset,
                                   plan.outerRank, plan.innerRank);
        rewriter.rewriteUsesInParentRegion(alloc, dbAllocOp, *AC, opsToRemove);
      } else {
        DbElementWiseRewriter rewriter(elemOffset, elemSize, plan.outerRank,
                                       plan.innerRank, {});
        /// Step 1: Rewrite parent region uses (skips EDTs by design)
        rewriter.rewriteUsesInParentRegion(alloc, dbAllocOp, *AC, opsToRemove);

        /// Step 2: For coarse mode, also rewrite EDT uses without explicit deps
        if (plan.outerRank == 0) {
          Type elementMemRefType = dbAllocOp.getAllocatedElementType();
          SmallVector<Operation *> edtUsesToRewrite;
          for (auto &use : alloc->getUses()) {
            Operation *user = use.getOwner();
            if (user == dbAllocOp.getOperation())
              continue;
            if (auto parentEdt = user->getParentOfType<EdtOp>()) {
              if (info.edtToDeps.count(parentEdt) == 0)
                edtUsesToRewrite.push_back(user);
            }
          }
          if (!edtUsesToRewrite.empty()) {
            rewriter.rebaseOps(edtUsesToRewrite, dbAllocOp.getPtr(),
                               elementMemRefType, *AC, opsToRemove);
          }
        }
      }
    }
  }
}

/// Find parent scope's acquired handle for a datablock
/// Checks if the parent EDT already has an acquired handle for the given
/// DbAllocOp.
Value CreateDbsPass::findParentAcquireSource(EdtOp edt, Operation *dbAllocOp,
                                             bool requiresIndexedAccess) {
  auto dbAlloc = dyn_cast_or_null<DbAllocOp>(dbAllocOp);
  if (!dbAlloc)
    return nullptr;

  ARTS_DEBUG("   - Checking if parent has acquired handle for this memref");

  Block *parentBlock = edt->getBlock();
  if (!parentBlock)
    return nullptr;

  Type targetType = dbAlloc.getPtr().getType();

  auto matchesDb = [&](Value v) -> bool {
    if (!v || v.getType() != targetType)
      return false;

    Operation *db = DatablockUtils::getUnderlyingDbAlloc(v);
    if (!db || db != dbAllocOp)
      return false;

    /// If the caller intends to index into this handle, avoid reusing a
    /// single-element datablock view.
    if (requiresIndexedAccess) {
      if (Operation *dbOp = DatablockUtils::getUnderlyingDb(v)) {
        if (DatablockUtils::hasSingleSize(dbOp))
          return false;
      }
    }

    /// Avoid treating the DbAllocOp results themselves as reusable handles
    if (auto *defOp = v.getDefiningOp())
      if (isa<DbAllocOp>(defOp))
        return false;

    return true;
  };

  /// Check parent EDT's block arguments
  for (BlockArgument arg : parentBlock->getArguments()) {
    if (matchesDb(arg)) {
      ARTS_DEBUG("   - Found matching handle in parent block arguments");
      return arg;
    }
  }

  ARTS_DEBUG("   - No existing handle found in parent scope");
  return nullptr;
}

///===----------------------------------------------------------------------===///
// Process EDT Dependencies
/// For each EDT, analyze external memory dependencies and insert
/// acquire/release
///===----------------------------------------------------------------------===///
void CreateDbsPass::createDbAcquireOps(EdtOp edt,
                                       SetVector<Value> &externalDeps) {
  ARTS_DEBUG(" - Creating DbAcquire operations for "
             << externalDeps.size() << " external dependencies");

  /// Accumulate dependency operands to set on the EDT
  SmallVector<Value> dependencyOperands;

  /// Track which DbAllocOps have already been processed for coarse-grained
  /// acquires. For fine-grained acquires, we may need multiple acquires per
  /// DbAllocOp with different indices/offsets, so we don't skip those.
  DenseSet<Operation *> processedCoarseAllocs;

  /// For each external value, create acquire and release operations
  for (Value externalDep : externalDeps) {
    /// Locate the underlying operation
    Operation *underlyingOp = ValueUtils::getUnderlyingOperation(externalDep);
    if (!underlyingOp)
      if (auto load = externalDep.getDefiningOp<memref::LoadOp>())
        underlyingOp = ValueUtils::getUnderlyingOperation(load.getMemref());
    assert(underlyingOp && "Underlying operation not found");

    /// Get the memref info for the underlying operation
    MemrefInfo &info = memrefInfo[underlyingOp];
    DbAllocOp dbAllocOp = info.dbAllocOp;
    ARTS_DEBUG("DbAllocOp " << dbAllocOp);
    if (!dbAllocOp) {
      ARTS_ERROR("DbAllocOp not found for: " << *underlyingOp);
      assert(false && "DbAllocOp not found for underlying operation");
      continue;
    }

    /// Get the source guid and ptr
    auto sourceGuid = dbAllocOp.getGuid();
    auto sourcePtr = dbAllocOp.getPtr();
    auto sourceSizes = dbAllocOp.getSizes();
    assert((sourceGuid && sourcePtr) && "Source guid and ptr must be non-null");

    /// Create acquire operation (pass both source guid and ptr)
    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPoint(edt);

    /// We use fine-grained acquire if the allocation was also fine-grained
    /// AND this EDT has dependencies for this memref
    const auto &deps = info.edtToDeps[edt];
    if (info.usedFineGrained && !deps.empty()) {
      /// Fine-grained acquire: Multiple separate acquires for stencil patterns
      /// Example: A stencil accessing u[i-1], u[i], u[i+1] creates 3 acquires
      ARTS_DEBUG(" - Using fine-grained acquire with " << deps.size()
                                                       << " dependencies");

      for (const auto &dep : deps) {
        const auto &depIndices = dep.indices;
        const auto &depOffsets = dep.offsets;

        SmallVector<Value> acquireIndices(depIndices.begin(), depIndices.end());
        SmallVector<Value> acquireOffsets(depOffsets.begin(), depOffsets.end());
        assert(sourceSizes.size() >= depIndices.size() &&
               "Sizes must be greater than or equal to indices");

        /// CHUNKED MODE: Convert element offset to chunk index for DbAcquireOp
        /// Use stored plan instead of recomputing chunked detection
        const auto &plan = info.rewritePlan.value();
        bool isChunkedDep = plan.mode == RewriterMode::Chunked &&
                            depIndices.empty() && !depOffsets.empty();
        SmallVector<Value> rewriteOffsets(depOffsets.begin(), depOffsets.end());
        if (isChunkedDep) {
          Value elemOffset = depOffsets[0];
          Location loc = edt.getLoc();

          Value chunkIndex =
              AC->create<arith::DivUIOp>(loc, elemOffset, plan.chunkSize);
          acquireOffsets.clear();
          acquireOffsets.push_back(chunkIndex);
          ARTS_DEBUG(
              "   - Chunked dep: converted element offset to chunk index");
        }

        /// Source guid and ptr are the source of the acquire
        Value acqGuid = sourceGuid;
        Value acqPtr = sourcePtr;

        /// Check if there is a parent acquired handle for this memref
        if (Value availableHandle =
                findParentAcquireSource(edt, dbAllocOp, !depIndices.empty())) {
          ARTS_DEBUG("   - Reusing existing datablock handle in parent scope");
          acqGuid = Value();
          acqPtr = availableHandle;
        }

        /// Compute acquired view sizes based on allocation mode:
        /// - Chunked: acquire exactly 1 chunk
        /// - Element-wise: indexed dimensions become size 1
        SmallVector<Value> acquireSizes;
        auto allocSizes = dbAllocOp.getSizes();
        unsigned numIndices = acquireIndices.size();

        if (isChunkedDep) {
          /// Chunked mode: acquire exactly 1 chunk
          acquireSizes.push_back(AC->createIndexConstant(1, edt.getLoc()));
        } else {
          /// Element-wise or coarse-grained: existing logic
          /// DbAcquire works like memref.load - indexed dimensions become
          /// size 1. Example: DbAlloc sizes=[N,N] with indices=[i,j] ->
          /// acquireSizes=[1,1]
          for (unsigned i = 0; i < allocSizes.size(); ++i) {
            if (i < numIndices) {
              /// Dimension is indexed -> acquired view has size 1 in this dim
              acquireSizes.push_back(AC->createIndexConstant(1, edt.getLoc()));
            } else {
              /// Dimension is not indexed -> preserve original size
              acquireSizes.push_back(allocSizes[i]);
            }
          }
        }
        if (acquireSizes.empty())
          acquireSizes.push_back(AC->createIndexConstant(1, edt.getLoc()));
        if (acquireOffsets.size() < acquireSizes.size()) {
          auto zero = AC->createIndexConstant(0, edt.getLoc());
          acquireOffsets.resize(acquireSizes.size(), zero);
        }
        auto acquireOp = AC->create<DbAcquireOp>(
            edt.getLoc(), dep.mode, acqGuid, acqPtr, acquireIndices,
            acquireOffsets, acquireSizes);
        ARTS_DEBUG(" - Created fine-grained acquire operation: " << acquireOp);

        /// Twin-diff decision for fine-grained acquires:
        /// When DbControlOps provide indexed access patterns (from OpenMP
        /// depend clauses like `depend(inout: A[i])`), each EDT acquires a
        /// DISTINCT element of the datablock.
        /// Therefore, twin-diff is NOT needed - we can safely disable it.
        TwinDiffContext twinCtx;
        twinCtx.proof = TwinDiffProof::IndexedControl;
        twinCtx.op = acquireOp.getOperation();
        bool useTwinDiff = AM->getHeuristicsConfig().shouldUseTwinDiff(twinCtx);
        acquireOp.setTwinDiff(useTwinDiff);
        ARTS_DEBUG("   - Fine-grained acquire: twin_diff=" << useTwinDiff);

        /// Add corresponding block argument for each acquired/reused view
        Value acquirePtr = acquireOp.getPtr();
        auto sourceType = acquirePtr.getType().dyn_cast<MemRefType>();
        BlockArgument dbAcquireArg =
            edt.getBody().front().addArgument(sourceType, edt.getLoc());
        dependencyOperands.push_back(acquirePtr);

        /// Rewrite uses in edt
        SmallVector<Operation *> opsToRewrite(dep.operations.begin(),
                                              dep.operations.end());
        rewriteOpsToUseDbAcquire(edt, opsToRewrite, acquireOp, acquireIndices,
                                 rewriteOffsets, dbAcquireArg, plan);

        /// Insert release for each acquire before EDT terminator
        OpBuilder::InsertionGuard IG(AC->getBuilder());
        AC->setInsertionPoint(edt.getBody().front().getTerminator());
        AC->create<DbReleaseOp>(edt.getLoc(), dbAcquireArg);
      }
      /// We've handled everything for this memref, continue to next
      continue;
    } else {
      /// Coarse-grained acquire: Acquire entire DB as a single block
      ARTS_DEBUG(" - Using coarse-grained acquire");

      /// For coarse-grained acquires, all acquires for the same DbAllocOp have
      /// identical parameters (offsets=[0], sizes=[1]).
      if (!processedCoarseAllocs.insert(dbAllocOp.getOperation()).second) {
        ARTS_DEBUG("   - Skipping duplicate coarse-grained acquire for "
                   "already-processed DbAllocOp");
        continue;
      }

      SmallVector<Value> acquireIndices, acquireOffsets, acquireSizes;
      acquireOffsets.push_back(AC->createIndexConstant(0, edt.getLoc()));

      /// For coarse-grained acquire on fine-grained allocations, acquire ALL
      /// chunks. Use the DbAllocOp's sizes instead of hardcoded 1.
      SmallVector<Value> allocSizes(dbAllocOp.getSizes().begin(),
                                    dbAllocOp.getSizes().end());
      if (allocSizes.empty()) {
        acquireSizes.push_back(AC->createIndexConstant(1, edt.getLoc()));
      } else {
        for (Value s : allocSizes)
          acquireSizes.push_back(s);
      }

      /// Source guid and ptr are the source of the acquire
      Value acqGuid = sourceGuid;
      Value acqPtr = sourcePtr;

      /// Check if there is a parent acquired handle for this memref
      if (Value availableHandle = findParentAcquireSource(edt, dbAllocOp)) {
        ARTS_DEBUG(" - Reusing existing datablock handle in parent scope");
        acqGuid = Value();
        acqPtr = availableHandle;
      }

      /// Use the dependency mode from DbControlOp if available
      ArtsMode acquireMode = ArtsMode::inout;
      auto acquireOp =
          AC->create<DbAcquireOp>(edt.getLoc(), acquireMode, acqGuid, acqPtr,
                                  acquireIndices, acquireOffsets, acquireSizes);
      ARTS_DEBUG(" - Created coarse-grained acquire operation: " << acquireOp);

      /// Twin-diff decision for coarse-grained acquires:
      /// When we fall back to coarse-grained allocation, we CANNOT guarantee
      /// that EDTs access disjoint regions of the datablock. This happens when:
      ///   1. No DbControlOps were provided (no OpenMP depend clauses)
      ///   2. Access patterns are inconsistent across EDTs
      ///   3. Some EDTs need full array access without specific indices
      ///
      /// In these cases, we MUST enable twin-diff because:
      ///   - Multiple workers may write to overlapping regions
      ///   - Without twin-diff, concurrent writes cause DATA CORRUPTION
      ///   - The runtime twin-diff mechanism safely merges partial updates
      ///
      /// The DbPartitioning pass may later disable twin-diff if it can prove
      /// non-overlap through successful partitioning or alias analysis.
      TwinDiffContext twinCtx;
      twinCtx.proof = TwinDiffProof::None;
      twinCtx.isCoarseAllocation = true;
      twinCtx.op = acquireOp.getOperation();
      bool useTwinDiff = AM->getHeuristicsConfig().shouldUseTwinDiff(twinCtx);
      acquireOp.setTwinDiff(useTwinDiff);
      ARTS_DEBUG("   - Coarse-grained acquire: twin_diff=" << useTwinDiff);

      /// Add corresponding block argument for the acquired/reused view
      Value acquirePtr = acquireOp.getPtr();
      auto sourceType = acquirePtr.getType().dyn_cast<MemRefType>();
      BlockArgument dbAcquireArg =
          edt.getBody().front().addArgument(sourceType, edt.getLoc());
      dependencyOperands.push_back(acquirePtr);

      /// Collect all operations from all deps for coarse-grained rewrite
      SmallVector<Operation *> opsToRewrite;
      llvm::SmallPtrSet<Operation *, 16> seenOps;
      auto addOpIfNew = [&](Operation *op) {
        if (!op)
          return;
        if (seenOps.insert(op).second)
          opsToRewrite.push_back(op);
      };

      bool missingOpsForDeps = false;
      const auto &deps = info.edtToDeps[edt];
      for (const auto &dep : deps) {
        if (dep.operations.empty())
          missingOpsForDeps = true;
        for (Operation *op : dep.operations)
          addOpIfNew(op);
      }

      /// If we failed to match operations for any dependency (SSA mismatch)
      /// or if none were found, conservatively rewrite all uses of the memref
      /// within this EDT.
      if (opsToRewrite.empty() || missingOpsForDeps) {
        ARTS_DEBUG(" - Falling back: collecting all operations using this "
                   "memref in the EDT");
        edt.walk([&](Operation *op) {
          /// Skip operations that belong to nested EDTs - they will be
          /// processed when their containing EDT is handled
          if (op->getParentOfType<EdtOp>() != edt)
            return;

          for (Value operand : op->getOperands()) {
            Operation *operandUnderlyingOp =
                ValueUtils::getUnderlyingOperation(operand);
            if (operandUnderlyingOp == underlyingOp) {
              addOpIfNew(op);
              break;
            }
          }
        });
        ARTS_DEBUG(" - Found " << opsToRewrite.size()
                               << " operations to rewrite");
      }

      if (!opsToRewrite.empty()) {
        rewriteOpsToUseDbAcquire(edt, opsToRewrite, acquireOp, acquireIndices,
                                 acquireOffsets, dbAcquireArg,
                                 info.rewritePlan.value());
      }

      /// Insert release for this acquire before EDT terminator
      OpBuilder::InsertionGuard IG(AC->getBuilder());
      AC->setInsertionPoint(edt.getBody().front().getTerminator());
      AC->create<DbReleaseOp>(edt.getLoc(), dbAcquireArg);
    }
  }

  /// After processing all memrefs, set EDT dependencies
  edt.setDependencies(dependencyOperands);
}

///===----------------------------------------------------------------------===///
// Insert DB Free Operations
/// Insert db_free for DbAllocOp at the appropriate location and remove
/// associated dealloc operations
///===----------------------------------------------------------------------===///
void CreateDbsPass::insertDbFreeForDbAlloc(DbAllocOp dbAlloc,
                                           Operation *alloc) {
  Location loc = dbAlloc.getLoc();
  Value allocResult = alloc->getResult(0);

  /// Find associated dealloc operation
  std::optional<Operation *> deallocOp = memref::findDealloc(allocResult);
  if (deallocOp.has_value() && *deallocOp)
    opsToRemove.insert(*deallocOp);

  /// Determine where to insert DbFreeOp based on where dbAlloc is located
  /// Insert at the end of the block containing dbAlloc
  Block *allocBlock = dbAlloc->getBlock();
  Operation *insertionPoint = allocBlock->getTerminator();

  /// Insert DbFreeOp if we found a valid insertion point
  assert(insertionPoint && "Could not find insertion point for DbFreeOp");
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(insertionPoint);
  AC->create<DbFreeOp>(loc, dbAlloc.getGuid());
  AC->create<DbFreeOp>(loc, dbAlloc.getPtr());
}

///===----------------------------------------------------------------------===///
/// Rewrite Alloc uses
/// Adjust load/store indices based on DB allocation granularity.
///===----------------------------------------------------------------------===///
void CreateDbsPass::rewriteUsesInParentEdt(MemrefInfo &memrefInfo) {
  assert(memrefInfo.dbAllocOp && "No DbAllocOp found");

  auto dbAlloc = memrefInfo.dbAllocOp;
  Type elementMemRefType = dbAlloc.getAllocatedElementType();

  /// Collect users in the parent edt
  SmallVector<Operation *, 8> users;
  for (auto &use : memrefInfo.alloc->getUses()) {
    Operation *user = use.getOwner();
    if (user->getParentOfType<EdtOp>() == memrefInfo.parentEdt)
      users.push_back(user);
  }

  ARTS_DEBUG(" - Rewriting " << users.size() << " operations in parent EDT");

  /// Coarse-grained: db_ref[0] + load/store[indices]
  /// Fine-grained: db_ref[indices] + load/store[0]
  unsigned outerRank =
      memrefInfo.usedFineGrained ? dbAlloc.getSizes().size() : 0;
  unsigned innerRank = elementMemRefType.cast<MemRefType>().getRank();
  Value elemOffset = AC->createIndexConstant(0, dbAlloc.getLoc());
  Value elemSize = AC->createIndexConstant(1, dbAlloc.getLoc());
  DbElementWiseRewriter rewriter(elemOffset, elemSize, outerRank, innerRank,
                                 {});
  for (Operation *user : users)
    rewriter.rewriteAccessWithDbPattern(user, dbAlloc.getPtr(),
                                        elementMemRefType, *AC, opsToRemove);
}

/// Rewrite all uses of a coarse-grained allocation in the parent region.
/// This is used when the allocation is not inside an EDT, but is still shared
/// with EDTs and the host needs to see the updated data.
/// For coarse-grained: all indices go to inner load/store, db_ref gets [0].
void CreateDbsPass::rewriteUsesEverywhereCoarse(Operation *alloc,
                                                DbAllocOp dbAlloc) {
  Type elementMemRefType = dbAlloc.getAllocatedElementType();

  SmallVector<Operation *> users;
  for (auto &use : alloc->getUses()) {
    Operation *user = use.getOwner();
    /// Skip the DbAlloc itself
    if (user == dbAlloc.getOperation())
      continue;
    /// Skip uses inside EDTs; those are rewritten via acquires separately.
    if (user->getParentOfType<EdtOp>())
      continue;
    users.push_back(user);
  }

  if (users.empty())
    return;

  /// Coarse-grained: outerCount=0, all indices go to inner load/store
  Value elemOffset = AC->createIndexConstant(0, dbAlloc.getLoc());
  Value elemSize = AC->createIndexConstant(1, dbAlloc.getLoc());
  DbElementWiseRewriter rewriter(elemOffset, elemSize, 0,
                                 elementMemRefType.cast<MemRefType>().getRank(),
                                 {});
  for (Operation *user : users)
    rewriter.rewriteAccessWithDbPattern(user, dbAlloc.getPtr(),
                                        elementMemRefType, *AC, opsToRemove);
}

///===----------------------------------------------------------------------===///
/// Rewrite Operations to Use DbAcquire
/// Rewrite loads/stores in an EDT to use acquired datablocks
/// Uses the stored DbRewritePlan to avoid recomputing allocation decisions
///===----------------------------------------------------------------------===///
void CreateDbsPass::rewriteOpsToUseDbAcquire(
    EdtOp edt, SmallVector<Operation *> &operations, Operation *dbOp,
    SmallVector<Value> &acquireIndices, SmallVector<Value> &acquireOffsets,
    BlockArgument dbAcquireArg, const DbRewritePlan &plan) {
  ARTS_DEBUG(" - Rewriting " << operations.size() << " operations in EDT");

  /// Get the element type from the acquired datablock
  /// For fine-grained: memref<?xmemref<?xT>> -> extract memref<?xT>
  Type elementMemRefType = dbAcquireArg.getType();
  if (auto outerMemrefType = elementMemRefType.dyn_cast<MemRefType>()) {
    if (auto innerMemrefType =
            outerMemrefType.getElementType().dyn_cast<MemRefType>()) {
      elementMemRefType = innerMemrefType;
    }
  }
  ARTS_DEBUG(" - Element memref type: " << elementMemRefType);

  /// Use stored ranks from plan, with fallback for innerRank
  unsigned innerRank = plan.innerRank;
  if (innerRank == 0)
    if (auto memrefType = elementMemRefType.dyn_cast<MemRefType>())
      innerRank = memrefType.getRank();

  /// Build value mapping from parent EDT block args/acquires to this EDT's args
  IRMapping scopeMapping;
  Block &edtBlock = edt.getBody().front();
  auto deps = edt.getDependenciesAsVector();

  for (auto [idx, blockArg] : llvm::enumerate(edtBlock.getArguments())) {
    if (idx < deps.size()) {
      Value externalDep = deps[idx];
      if (auto parentArg = externalDep.dyn_cast<BlockArgument>()) {
        scopeMapping.map(parentArg, blockArg);
        ARTS_DEBUG("   - Mapping parent block arg to local");
      }
      if (auto acquireOp = externalDep.getDefiningOp<DbAcquireOp>()) {
        scopeMapping.map(acquireOp.getPtr(), blockArg);
        if (Value srcPtr = acquireOp.getSourcePtr())
          scopeMapping.map(srcPtr, blockArg);
        ARTS_DEBUG("   - Mapping acquire result to local block arg");
      }
    }
  }

  /// Rewrite each tracked operation with DbRefOp pattern
  for (Operation *op : operations) {
    ARTS_DEBUG(" - Rewriting operation: " << *op);
    OpBuilder::InsertionGuard ig(AC->getBuilder());
    AC->setInsertionPoint(op);

    /// Apply scope mapping to operands before rewriting
    for (OpOperand &operand : op->getOpOperands()) {
      Value mappedVal = scopeMapping.lookupOrDefault(operand.get());
      if (mappedVal != operand.get()) {
        operand.set(mappedVal);
        ARTS_DEBUG("   - Remapped operand from outer scope to inner scope");
      }
    }

    if (plan.mode == RewriterMode::Chunked) {
      /// Chunked mode: Use DbChunkedRewriter with stored chunk size
      ARTS_DEBUG(" - Using DbChunkedRewriter with stored plan");
      Location loc = op->getLoc();
      Value startOffset = acquireOffsets.empty()
                              ? AC->createIndexConstant(0, loc)
                              : acquireOffsets[0];

      /// Compute startChunk using stored chunkSize from plan
      Value startChunk =
          AC->create<arith::DivUIOp>(loc, startOffset, plan.chunkSize);
      Value elemOffset = AC->createIndexConstant(0, loc);

      unsigned chunkOuterRank = plan.outerRank;
      if (chunkOuterRank == 0)
        if (auto acquireOp = dyn_cast<DbAcquireOp>(dbOp))
          chunkOuterRank = acquireOp.getSizes().size();

      DbChunkedRewriter rewriter(plan.chunkSize, startChunk, elemOffset,
                                 chunkOuterRank, innerRank);
      llvm::SetVector<Operation *> localOpsToRemove;
      rewriter.rebaseOps({op}, dbAcquireArg, elementMemRefType, *AC,
                         localOpsToRemove);
      for (Operation *toRemove : localOpsToRemove)
        opsToRemove.insert(toRemove);
      continue;
    }

    /// ElementWise mode
    Value elemOffset = AC->createIndexConstant(0, op->getLoc());
    Value elemSize = AC->createIndexConstant(1, op->getLoc());
    DbElementWiseRewriter rewriter(elemOffset, elemSize, plan.outerRank,
                                   innerRank, {});

    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      auto localized = rewriter.localizeForFineGrained(
          load.getIndices(), acquireIndices, acquireOffsets, AC->getBuilder(),
          op->getLoc());
      auto dbRef = AC->create<DbRefOp>(op->getLoc(), elementMemRefType,
                                       dbAcquireArg, localized.dbRefIndices);
      auto newLoad = AC->create<memref::LoadOp>(op->getLoc(), dbRef,
                                                localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(op);
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      auto localized = rewriter.localizeForFineGrained(
          store.getIndices(), acquireIndices, acquireOffsets, AC->getBuilder(),
          op->getLoc());
      auto dbRef = AC->create<DbRefOp>(op->getLoc(), elementMemRefType,
                                       dbAcquireArg, localized.dbRefIndices);
      AC->create<memref::StoreOp>(op->getLoc(), store.getValue(), dbRef,
                                  localized.memrefIndices);
      opsToRemove.insert(op);
    } else {
      llvm::SetVector<Operation *> localOpsToRemove;
      rewriter.rebaseOps({op}, dbAcquireArg, elementMemRefType, *AC,
                         localOpsToRemove);
      for (Operation *toRemove : localOpsToRemove)
        opsToRemove.insert(toRemove);
    }
  }
}

///===----------------------------------------------------------------------===///
// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateDbsPass(ArtsAnalysisManager *AM) {
  return std::make_unique<CreateDbsPass>(AM);
}
} // namespace arts
} // namespace mlir
