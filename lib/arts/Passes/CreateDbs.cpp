///==========================================================================
// File: CreateDbs.cpp
//
// This pass  focuses on handling memory dependencies for EDTs, where each EDT
// might run in a separate memory environment. To ensure data availability,
// every external memref (not allocated within the EDT) used in loads/stores
// is wrapped in ARTS Dbs operations:
// 1. Identify all unique external allocations across all EDTs via load/store
// analysis.
// 2. Create arts.db_alloc for each such allocation immediately after its
// original alloc op.
// 3. For each EDT, collect external bases used in its region (including
// nested).
// 4. For each base, determine the source (db_alloc or parent's acquired block
// arg if nested).
// 5. Compute dependency info (mode, pinned indices, offsets, sizes) based on
// access patterns:
//    - Mode: 'in', 'out', or 'inout' based on reads/writes.
//    - Pinned: Leading constant indices common to all accesses.
//    - Offsets/Sizes: Bounding box for variable or range indices; full dim if
//    no access info.
// 6. Insert arts.db_acquire before the EDT with computed slice, add as
// operand and block arg.
// 7. Adjust all loads/stores in the EDT region to use the acquired view,
// updating indices for offsets.
// 8. Insert arts.db_release before the yield terminator.
// This ensures data is fetched/released properly for remote execution, with
// nested EDTs inheriting parent's acquires when possible. db_control ops from
// parent's acquires when possible. db_control ops from / prior passes are
// ignored and erased, as dependencies are recomputed from / actual accesses.

// Discussion on DB Acquisition Sizes and Efficiency:
// In this pass, we compute acquisition regions based on actual accesses
// within / the EDT and its nested tasks. For efficiency, we merge
// finer-grained access / patterns into coarser ones where possible (e.g.,
// using min/max for constant / indices). This coarsening reduces the number of
// dependencies per EDT, / lowering runtime overhead in task-based systems.
// However, coarser DBs may / over-approximate dependencies, potentially
// reducing parallelism by making / tasks wait on larger data blocks. The
// trade-off is between overhead reduction and preserving fine-grained
// concurrency. We create coarser DBs here, with potential for further
// optimization in subsequent passes.
//==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/StringAnalysis.h"
#include "arts/ArtsDialect.h"

#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(create_dbs);

using namespace mlir;
using namespace mlir::arts;

/// Access pattern info for a memref base
struct AccessInfo {
  bool hasRead = false;
  bool hasWrite = false;
  SmallVector<SetVector<int64_t>> dimConstants;
  SmallVector<bool> hasNonConstant;

  AccessInfo(int rank) : dimConstants(rank), hasNonConstant(rank, false) {}
};

/// Helper struct for dependency info
struct DepInfo {
  ArtsMode mode;
  SmallVector<Value> pinned;
  SmallVector<Value> offsets;
  SmallVector<Value> sizes;
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

  /// Core helper functions
  Value getOriginalAllocation(Value v);
  void cleanupExistingDbOps();
  SetVector<Value> collectUsedAllocations();
  void createDbAllocOps(const SetVector<Value> &allocs, OpBuilder &builder);
  void processEdtDependencies(EdtOp edt, OpBuilder &builder,
                              const StringAnalysis &analysis);

  /// Dependency analysis
  AccessInfo analyzeAccesses(EdtOp edt, Value originalBase);
  DepInfo computeDepInfo(OpBuilder &builder, EdtOp edt, Value originalBase,
                         Value source, const StringAnalysis &analysis);
  Value findParentSource(EdtOp edt, Value base);

  /// Access adjustment
  void adjustAccesses(OpBuilder &builder, Region &region, Value originalBase,
                      Value view, SmallVector<Value> pinned,
                      SmallVector<Value> offsets);
};
} // namespace

Value CreateDbsPass::getOriginalAllocation(Value v) {
  if (v.isa<BlockArgument>()) {
    Block *block = v.getParentBlock();
    Operation *owner = block->getParentOp();
    if (auto edt = dyn_cast<EdtOp>(owner)) {
      unsigned argIndex = v.cast<BlockArgument>().getArgNumber();
      Value operand = edt.getOperand(argIndex);
      return getOriginalAllocation(operand);
    } else {
      /// Function argument
      return v;
    }
  }
  if (auto op = v.getDefiningOp()) {
    if (auto dbAlloc = dyn_cast<DbAllocOp>(op)) {
      return getOriginalAllocation(dbAlloc.getAddress());
    } else if (auto dbAcquire = dyn_cast<DbAcquireOp>(op)) {
      return getOriginalAllocation(dbAcquire.getSource());
    } else if (isa<memref::AllocOp, memref::AllocaOp, memref::GetGlobalOp>(
                   op)) {
      return v;
    } else if (auto subview = dyn_cast<memref::SubViewOp>(op)) {
      return getOriginalAllocation(subview.getSource());
    } else if (auto cast = dyn_cast<memref::CastOp>(op)) {
      return getOriginalAllocation(cast.getSource());
    }
  }
  return nullptr;
}

void CreateDbsPass::cleanupExistingDbOps() {
  /// Erase all db_control ops, replacing with their pointer
  module.walk([&](DbControlOp dbControl) {
    dbControl.getSubview().replaceAllUsesWith(dbControl.getPtr());
    dbControl.erase();
  });

  /// Clear all existing EDT dependencies and block args
  module.walk([&](EdtOp edt) {
    edt.getBody().front().eraseArguments([](BlockArgument) { return true; });
    edt->setOperands({});
  });
}

SetVector<Value> CreateDbsPass::collectUsedAllocations() {
  SetVector<Value> allUsedAllocs;
  module.walk([&](EdtOp edt) {
    edt.walk([&](Operation *op) {
      Value mem;
      if (auto load = dyn_cast<memref::LoadOp>(op)) {
        mem = load.getMemref();
      } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
        mem = store.getMemref();
      } else {
        return;
      }
      Value base = getOriginalAllocation(mem);
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
    MemRefType type = alloc.getType().cast<MemRefType>();

    SmallVector<Value> sizes;
    for (int i = 0; i < type.getRank(); ++i) {
      if (type.isDynamicDim(i)) {
        sizes.push_back(builder.create<memref::DimOp>(loc, alloc, i));
      } else {
        sizes.push_back(
            builder.create<arith::ConstantIndexOp>(loc, type.getDimSize(i)));
      }
    }

    /// Create DbAllocOp with default values for route, allocType, and dbMode
    Value route = builder.create<arith::ConstantIntOp>(loc, 0, 32);
    DbAllocType allocType = DbAllocType::heap;
    DbMode dbMode = DbMode::pin;

    auto dbAllocOp = builder.create<DbAllocOp>(loc, ArtsMode::inout, sizes,
                                               route, allocType, dbMode, alloc);

    /// Store the mapping from original allocation to DbAllocOp
    allocToDbAlloc[alloc] = dbAllocOp;
  }
}

AccessInfo CreateDbsPass::analyzeAccesses(EdtOp edt, Value originalBase) {
  Value anyMem;
  edt.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (getOriginalAllocation(load.getMemref()) == originalBase) {
        anyMem = load.getMemref();
        return;
      }
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (getOriginalAllocation(store.getMemref()) == originalBase) {
        anyMem = store.getMemref();
        return;
      }
    }
  });

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
    if (getOriginalAllocation(mem) != originalBase)
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

  AccessInfo accInfo = analyzeAccesses(edt, originalBase);

  ArtsMode mode;
  if (accInfo.hasWrite) {
    mode = accInfo.hasRead ? ArtsMode::inout : ArtsMode::out;
  } else if (accInfo.hasRead) {
    mode = ArtsMode::in;
  } else {
    mode = ArtsMode::inout;
  }

  /// String memrefs: acquire the whole memory
  if (analysis.isStringMemRef(originalBase)) {
    SmallVector<Value> fullOffsets, fullSizes;
    for (int d = 0; d < rank; ++d) {
      fullOffsets.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), 0));
      Value sizeVal;
      if (sourceType.isDynamicDim(d)) {
        sizeVal = builder.create<memref::DimOp>(edt.getLoc(), source, d);
      } else {
        sizeVal = builder.create<arith::ConstantIndexOp>(
            edt.getLoc(), sourceType.getDimSize(d));
      }
      fullSizes.push_back(sizeVal);
    }
    MemRefType fullResultType =
        MemRefType::get(sourceType.getShape(), sourceType.getElementType());
    return {mode, SmallVector<Value>{}, fullOffsets, fullSizes, fullResultType};
  }

  /// Compute pinned indices (leading constant dimensions)
  SmallVector<Value> pinned;
  int prefix = 0;
  for (int d = 0; d < rank; ++d) {
    if (accInfo.dimConstants[d].size() == 1 && !accInfo.hasNonConstant[d]) {
      int64_t val = *accInfo.dimConstants[d].begin();
      pinned.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), val));
      prefix++;
    } else {
      break;
    }
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
        sizeVal = builder.create<memref::DimOp>(edt.getLoc(), source, d);
      } else {
        sizeVal = builder.create<arith::ConstantIndexOp>(
            edt.getLoc(), sourceType.getDimSize(d));
      }
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

Value CreateDbsPass::findParentSource(EdtOp edt, Value base) {
  auto parentEdt = edt->getParentOfType<EdtOp>();
  if (!parentEdt)
    return nullptr;

  /// Try to find base in parent's block arguments mapping
  auto parentIt = edtBaseToArg.find(parentEdt.getOperation());
  if (parentIt != edtBaseToArg.end()) {
    auto it = parentIt->second.find(base);
    if (it != parentIt->second.end()) {
      return it->second;
    }
  }

  /// Fallback: scan parent block arguments
  for (BlockArgument arg : parentEdt.getBody().getArguments()) {
    if (getOriginalAllocation(arg) == base) {
      return arg;
    }
  }
  return nullptr;
}

void CreateDbsPass::processEdtDependencies(EdtOp edt, OpBuilder &builder,
                                           const StringAnalysis &analysis) {
  SetVector<Value> externalBases;

  /// Collect external bases used by this EDT (excluding nested EDTs)
  edt.walk([&](Operation *op) -> WalkResult {
    if (isa<EdtOp>(op) && op != edt.getOperation())
      return WalkResult::skip();

    Value mem;
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      mem = load.getMemref();
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      mem = store.getMemref();
    } else {
      return WalkResult::advance();
    }

    Value base = getOriginalAllocation(mem);
    if (base &&
        !edt.getRegion().isAncestor(base.getDefiningOp()->getParentRegion())) {
      externalBases.insert(base);
    }
    return WalkResult::advance();
  });

  /// Process each external base
  for (Value base : externalBases) {
    /// Find source (db_alloc, parent arg, or original base)
    Value source =
        allocToDbAlloc[base] ? allocToDbAlloc[base].getPtr() : base;
    if (Value parentSource = findParentSource(edt, base))
      source = parentSource;

    builder.setInsertionPoint(edt);
    DepInfo info = computeDepInfo(builder, edt, base, source, analysis);

    /// Create acquire operation
    ArtsModeAttr modeAttr = ArtsModeAttr::get(builder.getContext(), info.mode);
    DbAcquireOp acquire = builder.create<DbAcquireOp>(
        edt.getLoc(), info.resultType, modeAttr, source, info.pinned,
        info.offsets, info.sizes);

    /// Add dependency to EDT
    SmallVector<Value> newDeps(edt.getDependencies());
    newDeps.push_back(acquire.getPtr());
    edt->setOperands(newDeps);
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
    if (getOriginalAllocation(mem) != originalBase)
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

void CreateDbsPass::runOnOperation() {
  module = getOperation();
  ARTS_DEBUG_HEADER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););

  OpBuilder builder(module.getContext());
  StringAnalysis stringAnalysis(module);

  /// Phase 1: Cleanup existing DB operations
  cleanupExistingDbOps();

  /// Phase 2: Collect and create db_alloc operations
  SetVector<Value> allUsedAllocs = collectUsedAllocations();
  createDbAllocOps(allUsedAllocs, builder);

  /// Phase 3: Process EDTs in reverse order (parents before children)
  SmallVector<EdtOp> allEdts;
  module.walk([&](EdtOp edt) { allEdts.push_back(edt); });

  for (auto it = allEdts.rbegin(); it != allEdts.rend(); ++it)
    processEdtDependencies(*it, builder, stringAnalysis);

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