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
#include "arts/Codegen/ArtsIR.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/RegionUtils.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SmallVector.h"

#define DEBUG_TYPE "create-dbs"
#define LINE "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

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

  Value getOriginalAllocation(Value v);
  DepInfo computeDepInfo(OpBuilder &builder, EdtOp edt, Value originalBase,
                         Value source, const StringAnalysis &analysis);
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

DepInfo CreateDbsPass::computeDepInfo(OpBuilder &builder, EdtOp edt,
                                      Value originalBase, Value source,
                                      const StringAnalysis &analysis) {
  MemRefType sourceType = source.getType().dyn_cast<MemRefType>();
  assert(sourceType && "Source must be memref");
  int rank = sourceType.getRank();
  SmallVector<SetVector<int64_t>> dimConstants(rank);
  SmallVector<bool> hasNonConstant(rank, false);
  bool hasRead = false;
  bool hasWrite = false;

  edt.walk([&](Operation *op) {
    Value mem;
    SmallVector<Value> indices;
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      mem = load.getMemref();
      indices = load.getIndices();
      hasRead = true;
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      mem = store.getMemref();
      indices = store.getIndices();
      hasWrite = true;
    } else {
      return;
    }
    if (getOriginalAllocation(mem) != originalBase)
      return;
    assert((int)indices.size() == rank);
    for (int d = 0; d < rank; ++d) {
      Value idx = indices[d];
      if (auto constOp = idx.getDefiningOp<arith::ConstantIndexOp>()) {
        dimConstants[d].insert(constOp.getValue().cast<IntegerAttr>().getInt());
      } else {
        hasNonConstant[d] = true;
      }
    }
  });

  ArtsMode mode;
  if (hasWrite) {
    mode = hasRead ? ArtsMode::inout : ArtsMode::out;
  } else if (hasRead) {
    mode = ArtsMode::in;
  } else {
    mode = ArtsMode::inout;
  }

  /// String memrefs: acquire the whole memory
  bool isString = analysis.isStringMemRef(originalBase);
  if (isString) {
    SmallVector<Value> fullOffsets;
    SmallVector<Value> fullSizes;
    for (int d = 0; d < rank; ++d) {
      Value offVal = builder.create<arith::ConstantIndexOp>(edt.getLoc(), 0);
      Value sizeVal;
      if (sourceType.isDynamicDim(d)) {
        sizeVal = builder.create<memref::DimOp>(edt.getLoc(), source, d);
      } else {
        sizeVal = builder.create<arith::ConstantIndexOp>(
            edt.getLoc(), sourceType.getDimSize(d));
      }
      fullOffsets.push_back(offVal);
      fullSizes.push_back(sizeVal);
    }
    MemRefType fullResultType =
        MemRefType::get(sourceType.getShape(), sourceType.getElementType());
    return {mode, SmallVector<Value>{}, fullOffsets, fullSizes, fullResultType};
  }

  SmallVector<Value> pinned;
  int prefix = 0;
  for (int d = 0; d < rank; ++d) {
    if (dimConstants[d].size() == 1 && !hasNonConstant[d]) {
      int64_t val = *dimConstants[d].begin();
      pinned.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), val));
      prefix++;
    } else {
      break;
    }
  }

  SmallVector<Value> offsets, sizes;
  bool isFull = true;
  for (int d = prefix; d < rank; ++d) {
    Value offVal = builder.create<arith::ConstantIndexOp>(edt.getLoc(), 0);
    Value sizeVal;
    if (sourceType.isDynamicDim(d)) {
      sizeVal = builder.create<memref::DimOp>(edt.getLoc(), source, d);
    } else {
      sizeVal = builder.create<arith::ConstantIndexOp>(
          edt.getLoc(), sourceType.getDimSize(d));
    }
    if (hasNonConstant[d]) {
      offsets.push_back(offVal);
      sizes.push_back(sizeVal);
      isFull = false;
    } else if (!dimConstants[d].empty()) {
      int64_t minVal = LLONG_MAX;
      int64_t maxVal = LLONG_MIN;
      for (int64_t v : dimConstants[d]) {
        minVal = std::min(minVal, v);
        maxVal = std::max(maxVal, v);
      }
      offVal = builder.create<arith::ConstantIndexOp>(edt.getLoc(), minVal);
      int64_t sizeNum = maxVal - minVal + 1;
      sizeVal = builder.create<arith::ConstantIndexOp>(edt.getLoc(), sizeNum);
      offsets.push_back(offVal);
      sizes.push_back(sizeVal);
      isFull = false;
    } else {
      offsets.push_back(offVal);
      sizes.push_back(sizeVal);
    }
  }
  if (isFull && prefix == 0) {
    offsets.clear();
    sizes.clear();
  }

  SmallVector<int64_t> newShape(rank - prefix, ShapedType::kDynamic);
  MemRefType resultType =
      MemRefType::get(newShape, sourceType.getElementType());

  return {mode, pinned, offsets, sizes, resultType};
}

void CreateDbsPass::adjustAccesses(OpBuilder &builder, Region &region,
                                   Value originalBase, Value view,
                                   SmallVector<Value> pinned,
                                   SmallVector<Value> offsets) {
  int prefix = pinned.size();
  region.walk([&](Operation *op) {
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
      return;
    }
    if (getOriginalAllocation(mem) != originalBase)
      return;

    /// Verify leading pinned indices
    for (int i = 0; i < prefix; ++i) {
      Value idx = oldIndices[i];
      auto constOp = idx.getDefiningOp<arith::ConstantIndexOp>();
      assert(constOp &&
             constOp.getValue() ==
                 pinned[i].cast<arith::ConstantIndexOp>().getValue());
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
  });
}

void CreateDbsPass::runOnOperation() {
  module = getOperation();
  LLVM_DEBUG({
    dbgs() << "\n" << LINE << "CreateDbsPass STARTED\n" << LINE;
    module->dump();
  });

  OpBuilder builder(module.getContext());

  /// Run StringAnalysis to identify string memrefs
  StringAnalysis stringAnalysis(module);

  /// Erase all db_control ops, ignoring them but marking for removal
  /// effectively
  module.walk([&](DbControlOp dbControl) {
    Value subview = dbControl.getSubview();
    Value ptr = dbControl.getPtr();
    subview.replaceAllUsesWith(ptr);
    dbControl.erase();
  });

  /// Clear all existing EDT dependencies and block args
  module.walk([&](EdtOp edt) {
    edt.getBody().front().eraseArguments([](BlockArgument) { return true; });
    edt->setOperands({});
  });

  /// Collect all unique allocations used in any EDT via load/store
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

  /// Create db_alloc for each unique allocation used in EDTs
  for (Value alloc : allUsedAllocs) {
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
    DbAllocOp dbAlloc =
        builder.create<DbAllocOp>(loc, type, ArtsMode::inout, alloc, sizes);
    allocToDbAlloc[alloc] = dbAlloc;
  }

  /// Process each EDT
  module.walk([&](EdtOp edt) {
    SetVector<Value> externalBases;
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
      if (base && !edt.getRegion().isAncestor(
                      base.getDefiningOp()->getParentRegion())) {
        externalBases.insert(base);
      }
    });

    for (Value base : externalBases) {
      DbAllocOp dbAlloc = allocToDbAlloc[base];
      Value source =
          dbAlloc ? dbAlloc.getPtr() : base; /// Fallback if no dbAlloc

      /// Check if parent EDT has a block arg for this base
      if (auto parentEdt = edt->getParentOfType<EdtOp>()) {
        for (BlockArgument arg : parentEdt.getBody().getArguments()) {
          if (getOriginalAllocation(arg) == base) {
            source = arg;
            break;
          }
        }
      }

      builder.setInsertionPoint(edt);
      DepInfo info = computeDepInfo(builder, edt, base, source, stringAnalysis);
      ArtsModeAttr modeAttr =
          ArtsModeAttr::get(builder.getContext(), info.mode);

      DbAcquireOp acquire = builder.create<DbAcquireOp>(
          edt.getLoc(), info.resultType, modeAttr, source, info.pinned,
          info.offsets, info.sizes);

      /// Add the new dependency to the EDT
      SmallVector<Value> newDeps(edt.getDependencies());
      newDeps.push_back(acquire.getResult());
      edt->setOperands(newDeps);
      BlockArgument arg =
          edt.getBody().front().addArgument(info.resultType, edt.getLoc());

      adjustAccesses(builder, edt.getRegion(), base, arg, info.pinned,
                     info.offsets);

      builder.setInsertionPoint(edt.getBody().front().getTerminator());
      builder.create<DbReleaseOp>(edt.getLoc(), arg);
    }
  });

  LLVM_DEBUG({
    dbgs() << LINE << "CreateDbsPass FINISHED\n" << LINE;
    module->dump();
  });
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