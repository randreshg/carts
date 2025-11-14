///==========================================================================///
/// File: NestedAllocTransformer.cpp
/// Implements the NestedAllocTransformer for canonicalizing nested
/// array-of-arrays allocations.
///==========================================================================///

#include "arts/Transforms/NestedAllocTransformer.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Dominance.h"

ARTS_DEBUG_SETUP(nested_alloc_transformer);

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Helper Methods
///===----------------------------------------------------------------------===///

SmallVector<Value>
NestedAllocTransformer::Pattern::getAllSizes() const {
  SmallVector<Value> s = {dimSize};
  if (inner)
    s.append(inner->getAllSizes());
  else
    s.push_back(innerDimSize);
  return s;
}

SmallVector<scf::ForOp>
NestedAllocTransformer::Pattern::getAllInitLoops() const {
  SmallVector<scf::ForOp> l = {initLoop};
  if (inner)
    l.append(inner->getAllInitLoops());
  return l;
}

SmallVector<Value>
NestedAllocTransformer::Pattern::getAllNestedAllocs() const {
  SmallVector<Value> a = {outerAlloc};
  if (inner)
    a.append(inner->getAllNestedAllocs());
  else if (innerAllocValue)
    a.push_back(innerAllocValue);
  return a;
}

void NestedAllocTransformer::Statistics::print(llvm::raw_ostream &os) const {
  os << "NestedAllocTransformer Statistics:\n"
     << "  Patterns detected: " << patternsDetected << "\n"
     << "  Patterns transformed: " << patternsTransformed << "\n"
     << "  Accesses rewritten: " << accessesRewritten << "\n"
     << "  Allocations removed: " << allocationsRemoved << "\n";
}

///===----------------------------------------------------------------------===///
/// Constructor
///===----------------------------------------------------------------------===///

NestedAllocTransformer::NestedAllocTransformer(MLIRContext *context,
                                               Config config)
    : ctx_(context), config_(config) {}

///===----------------------------------------------------------------------===///
/// Main Entry Point
///===----------------------------------------------------------------------===///

LogicalResult
NestedAllocTransformer::transform(ModuleOp module, OpBuilder &builder,
                                  OpRemovalManager &removalMgr) {
  module_ = module;
  stats_ = Statistics();

  ARTS_DEBUG("=== NestedAllocTransformer::transform ===");

  /// Collect all function operations
  SmallVector<func::FuncOp> functions;
  module.walk([&](func::FuncOp func) { functions.push_back(func); });

  /// For each function, detect and transform nested allocation patterns
  for (auto func : functions) {
    /// Collect all allocations in the function
    SmallVector<Value> allocations;
    func.walk([&](Operation *op) {
      if (auto allocOp = dyn_cast<memref::AllocOp>(op))
        allocations.push_back(allocOp.getResult());
      else if (auto allocaOp = dyn_cast<memref::AllocaOp>(op))
        allocations.push_back(allocaOp.getResult());
    });

    /// Try to detect patterns for each allocation
    DenseMap<scf::ForOp, SmallVector<Pattern>> patternsByLoop;
    for (Value alloc : allocations) {
      auto pattern = detectPattern(alloc);
      if (pattern) {
        stats_.patternsDetected++;
        auto initLoop = pattern->getAllInitLoops()[0];
        patternsByLoop[initLoop].push_back(std::move(*pattern));
      }
    }

    if (patternsByLoop.empty()) {
      ARTS_DEBUG("  No nested allocation patterns found in function "
                 << func.getName());
      continue;
    }

    ARTS_DEBUG("  Found " << stats_.patternsDetected
                          << " nested allocation patterns");

    /// Transform each pattern group
    for (auto &[initLoop, patterns] : patternsByLoop) {
      for (auto &pattern : patterns) {
        /// Create canonical N-dimensional allocation
        Location loc = initLoop.getLoc();
        Value ndAlloc = transformToCanonical(pattern, loc, builder);
        if (!ndAlloc) {
          module.emitError("Failed to create canonical allocation");
          return failure();
        }

        /// Transform all accesses to use canonical allocation
        if (failed(transformAccesses(pattern, ndAlloc))) {
          module.emitError("Failed to transform accesses for nested allocation");
          return failure();
        }

        /// Clean up legacy operations
        cleanupPattern(pattern, removalMgr);

        stats_.patternsTransformed++;
      }
    }
  }

  ARTS_DEBUG("  Transformation complete: " << stats_.patternsTransformed
                                            << " patterns transformed");

  return success();
}

///===----------------------------------------------------------------------===///
/// Pattern Detection
///===----------------------------------------------------------------------===///

std::optional<NestedAllocTransformer::Pattern>
NestedAllocTransformer::detectPattern(Value alloc) {
  auto currentType = alloc.getType().dyn_cast<MemRefType>();
  if (!currentType)
    return std::nullopt;

  auto elementType = currentType.getElementType();
  auto innerMemRefType = elementType.dyn_cast<MemRefType>();
  if (!innerMemRefType)
    return std::nullopt;

  /// Get this dimension size
  Value thisSize;
  Operation *allocOpRaw = alloc.getDefiningOp();
  if (!allocOpRaw)
    return std::nullopt;

  Location loc = allocOpRaw->getLoc();

  /// Handle both AllocOp and AllocaOp
  SmallVector<Value> dynamicSizes = getAllocDynamicSizes(allocOpRaw);

  /// Handle rank-0 memrefs (scalar containers for nested memrefs)
  OpBuilder builder(ctx_);
  if (currentType.getRank() == 0) {
    builder.setInsertionPointAfter(allocOpRaw);
    thisSize = builder.create<arith::ConstantIndexOp>(loc, 1);
  } else if (currentType.isDynamicDim(0) && !dynamicSizes.empty()) {
    thisSize = dynamicSizes[0];
  } else if (!currentType.isDynamicDim(0)) {
    builder.setInsertionPointAfter(allocOpRaw);
    thisSize =
        builder.create<arith::ConstantIndexOp>(loc, currentType.getDimSize(0));
  } else {
    return std::nullopt;
  }

  /// Find the initialization loop and inner allocations
  scf::ForOp initLoop = nullptr;
  Value storedValue;

  for (Operation *user : alloc.getUsers()) {
    if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      if (storeOp.getMemref() != alloc)
        continue;

      storedValue = storeOp.getValue();

      /// Check for both AllocOp and AllocaOp
      Operation *innerAllocOp = storedValue.getDefiningOp();
      if (!innerAllocOp || (!isa<memref::AllocOp>(innerAllocOp) &&
                            !isa<memref::AllocaOp>(innerAllocOp)))
        continue;

      auto forOp = storeOp->getParentOfType<scf::ForOp>();
      if (!forOp)
        continue;

      initLoop = forOp;
      auto innerAllocType = storedValue.getType().cast<MemRefType>();

      /// Get inner dimension size
      Value innerSize;
      SmallVector<Value> innerDynamicSizes = getAllocDynamicSizes(innerAllocOp);

      if (innerAllocType.isDynamicDim(0) && !innerDynamicSizes.empty()) {
        innerSize = innerDynamicSizes[0];
      } else if (!innerAllocType.isDynamicDim(0)) {
        OpBuilder innerBuilder(ctx_);
        innerBuilder.setInsertionPoint(innerAllocOp);
        innerSize = innerBuilder.create<arith::ConstantIndexOp>(
            innerAllocOp->getLoc(), innerAllocType.getDimSize(0));
      }

      if (!innerSize)
        continue;

      /// Recursively detect inner pattern
      auto innerPattern = detectPattern(storedValue);

      Pattern pattern;
      pattern.outerAlloc = alloc;
      pattern.dimSize = thisSize;
      pattern.initLoop = initLoop;
      if (innerPattern) {
        pattern.inner = std::make_unique<Pattern>(std::move(*innerPattern));
        pattern.elementType = innerPattern->getFinalElementType();
      } else {
        pattern.elementType = innerAllocType.getElementType();
        pattern.innerDimSize = innerSize;
        pattern.innerAllocValue = storedValue;
      }

      /// Check if outerAlloc is stored to a rank-0 alloca (wrapper pattern)
      for (Operation *allocUser : alloc.getUsers()) {
        if (auto allocStoreOp = dyn_cast<memref::StoreOp>(allocUser)) {
          if (allocStoreOp.getValue() == alloc) {
            Value destMemref = allocStoreOp.getMemref();
            if (auto allocaOp = destMemref.getDefiningOp<memref::AllocaOp>()) {
              auto allocaType = allocaOp.getType().cast<MemRefType>();
              if (allocaType.getRank() == 0) {
                pattern.wrapperAlloca = destMemref;
                ARTS_DEBUG("   - Found wrapper alloca for outer allocation");
                break;
              }
            }
          }
        }
      }

      ARTS_DEBUG(" - Detected nested allocation pattern: depth="
                 << pattern.getDepth()
                 << ", element_type=" << pattern.elementType);

      return pattern;
    }
  }

  return std::nullopt;
}

///===----------------------------------------------------------------------===///
/// Transformation to Canonical Form
///===----------------------------------------------------------------------===///

Value NestedAllocTransformer::transformToCanonical(Pattern &pattern,
                                                    Location loc,
                                                    OpBuilder &builder) {
  builder.setInsertionPoint(pattern.initLoop);

  /// Extract all dimension sizes
  SmallVector<Value> allSizes = pattern.getAllSizes();

  /// Create N-D allocation
  Value ndAlloc = createCanonicalAllocation(builder, loc,
                                            pattern.getFinalElementType(),
                                            allSizes);
  if (!ndAlloc) {
    ARTS_DEBUG("  Failed to create canonical allocation");
    return nullptr;
  }

  /// Transfer metadata from old to new allocation
  auto allocOp = pattern.outerAlloc.getDefiningOp();
  if (allocOp)
    transferMetadata(allocOp, ndAlloc.getDefiningOp());

  /// Duplicate deallocation operations
  duplicateDeallocations(pattern.outerAlloc, ndAlloc, builder);

  ARTS_DEBUG("  Created canonical " << allSizes.size() << "-D allocation");

  return ndAlloc;
}

///===----------------------------------------------------------------------===///
/// Access Transformation
///===----------------------------------------------------------------------===///

LogicalResult
NestedAllocTransformer::transformAccesses(Pattern &pattern, Value ndAlloc) {
  /// Structure to hold information about a load chain
  struct ChainInfo {
    SmallVector<Value> indices;
    SmallVector<Operation *> loads;
  };

  Value outerAlloc = pattern.outerAlloc;

  /// Recursive function to collect index chain
  std::function<std::optional<ChainInfo>(Value)> collectChain;
  collectChain = [&](Value mem) -> std::optional<ChainInfo> {
    Value base = arts::getUnderlyingValue(mem);
    if (base == outerAlloc)
      return ChainInfo{{}, {}};

    // Check if this is a load from the wrapper alloca
    if (pattern.wrapperAlloca) {
      if (auto loadOp = mem.getDefiningOp<memref::LoadOp>()) {
        if (loadOp.getMemref() == pattern.wrapperAlloca &&
            loadOp.getIndices().empty()) {
          // This is loading the outer allocation from the wrapper alloca
          return ChainInfo{{}, {}};
        }
      }
    }

    // Check for memref.load
    if (auto defOp = mem.getDefiningOp<memref::LoadOp>()) {
      auto indicesRange = defOp.getIndices();
      SmallVector<Value> thisIndices(indicesRange.begin(), indicesRange.end());
      if (thisIndices.size() != 1)
        return std::nullopt;

      auto prevOpt = collectChain(defOp.getMemref());
      if (!prevOpt)
        return std::nullopt;

      ChainInfo prev = *prevOpt;
      prev.indices.push_back(thisIndices[0]);
      prev.loads.push_back(defOp);
      return prev;
    }

    return std::nullopt;
  };

  /// Collect operations to transform (collect first, transform later)
  struct AccessToRewrite {
    Operation *op;
    SmallVector<Value> allIndices;
    bool isLoad;
  };
  SmallVector<AccessToRewrite> toRewrite;

  module_.walk([&](Operation *op) -> WalkResult {
    Value mem;

    /// Check for memref.load operations
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      mem = loadOp.getMemref();
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      mem = storeOp.getMemref();
    } else {
      return WalkResult::advance();
    }

    /// Try to match this as part of a cascaded access chain
    auto chainOpt = collectChain(mem);
    if (!chainOpt)
      return WalkResult::advance();

    ChainInfo chain = *chainOpt;

    /// Get the final index from this operation
    SmallVector<Value> finalIndices;
    bool isLoad = false;
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      finalIndices.append(loadOp.getIndices().begin(),
                          loadOp.getIndices().end());
      isLoad = true;
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      finalIndices.append(storeOp.getIndices().begin(),
                          storeOp.getIndices().end());
      isLoad = false;
    }

    if (finalIndices.size() != 1)
      return WalkResult::advance();

    /// Build complete index list
    SmallVector<Value> allIndices = chain.indices;
    allIndices.push_back(finalIndices[0]);

    /// Collect this access for rewriting
    toRewrite.push_back({op, allIndices, isLoad});

    return WalkResult::advance();
  });

  /// Now perform the rewrites
  OpBuilder builder(ctx_);
  int64_t rewritten = 0;
  auto ndAllocType = ndAlloc.getType().cast<MemRefType>();
  unsigned expectedRank = ndAllocType.getRank();

  for (auto &access : toRewrite) {
    /// Verify index count matches allocation rank
    if (access.allIndices.size() != expectedRank) {
      ARTS_DEBUG("  Warning: Index count mismatch (have "
                 << access.allIndices.size() << ", expected " << expectedRank
                 << "), skipping access");
      continue;
    }

    builder.setInsertionPoint(access.op);

    if (access.isLoad) {
      auto loadOp = cast<memref::LoadOp>(access.op);
      auto newLoad = builder.create<memref::LoadOp>(loadOp.getLoc(), ndAlloc,
                                                     access.allIndices);
      loadOp.replaceAllUsesWith(newLoad.getResult());
      access.op->erase();
      rewritten++;
    } else {
      auto storeOp = cast<memref::StoreOp>(access.op);
      builder.create<memref::StoreOp>(storeOp.getLoc(), storeOp.getValue(),
                                      ndAlloc, access.allIndices);
      access.op->erase();
      rewritten++;
    }
  }

  if (rewritten == 0) {
    ARTS_DEBUG("  Warning: No accesses were transformed");
    return failure();
  }

  stats_.accessesRewritten += rewritten;
  ARTS_DEBUG("  Transformed " << rewritten << " cascaded accesses");

  return success();
}

///===----------------------------------------------------------------------===///
/// Cleanup
///===----------------------------------------------------------------------===///

void NestedAllocTransformer::cleanupPattern(Pattern &pattern,
                                            OpRemovalManager &removalMgr) {
  /// Clean up wrapper alloca if present
  if (pattern.wrapperAlloca) {
    /// Remove loads from the wrapper alloca
    for (Operation *user : pattern.wrapperAlloca.getUsers()) {
      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        if (loadOp.getMemref() == pattern.wrapperAlloca)
          removalMgr.markForRemoval(loadOp);
      }
    }
    /// Remove stores to the wrapper alloca
    for (Operation *user : pattern.wrapperAlloca.getUsers()) {
      if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        if (storeOp.getMemref() == pattern.wrapperAlloca)
          removalMgr.markForRemoval(storeOp);
      }
    }
    /// Remove the wrapper alloca itself
    if (Operation *allocaOp = pattern.wrapperAlloca.getDefiningOp())
      removalMgr.markForRemoval(allocaOp);
  }

  /// Schedule all legacy allocations for removal
  auto legacyAllocs = pattern.getAllNestedAllocs();
  for (Value legacy : legacyAllocs) {
    if (!legacy)
      continue;
    if (Operation *def = legacy.getDefiningOp()) {
      removalMgr.markForRemoval(def);
      stats_.allocationsRemoved++;
    }

    /// Remove stores that write this allocation into the outer array
    for (Operation *user : legacy.getUsers()) {
      if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        if (storeOp.getValue() == legacy)
          removalMgr.markForRemoval(storeOp);
      }
    }
  }

  /// Schedule initialization loops for removal
  auto initLoops = pattern.getAllInitLoops();
  for (auto loop : initLoops)
    removalMgr.markForRemoval(loop);

  /// Find and remove deallocation loops that dealloc inner arrays
  for (Operation *user : pattern.outerAlloc.getUsers()) {
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      /// Check if this load's result is used by memref.dealloc
      for (Operation *loadUser : loadOp->getUsers()) {
        if (isa<memref::DeallocOp>(loadUser)) {
          removalMgr.markForRemoval(loadOp);
          removalMgr.markForRemoval(loadUser);
          /// Also mark the enclosing loop for removal if it only contains
          /// deallocs
          if (auto forOp = loadOp->getParentOfType<scf::ForOp>())
            removalMgr.markForRemoval(forOp);
          break;
        }
      }
    } else if (auto deallocOp = dyn_cast<memref::DeallocOp>(user)) {
      /// Direct dealloc of outer array
      removalMgr.markForRemoval(deallocOp);
    }
  }

  /// Mark outer allocation for removal
  auto allocOp = pattern.outerAlloc.getDefiningOp<memref::AllocOp>();
  if (allocOp)
    removalMgr.markForRemoval(allocOp);

  /// Verify that all remaining users are marked for removal
  if (failed(removalMgr.verifyAllUsersMarked(pattern.outerAlloc,
                                              "Nested allocation (outer array)"))) {
    ARTS_DEBUG("  Warning: Not all users of outer allocation are marked");
  }
}

///===----------------------------------------------------------------------===///
/// Helper Methods
///===----------------------------------------------------------------------===///

SmallVector<Value>
NestedAllocTransformer::getAllocDynamicSizes(Operation *allocOp) {
  SmallVector<Value> sizes;
  if (auto alloc = dyn_cast<memref::AllocOp>(allocOp)) {
    sizes.append(alloc.getDynamicSizes().begin(),
                 alloc.getDynamicSizes().end());
  } else if (auto alloca = dyn_cast<memref::AllocaOp>(allocOp)) {
    sizes.append(alloca.getDynamicSizes().begin(),
                 alloca.getDynamicSizes().end());
  }
  return sizes;
}

Value NestedAllocTransformer::createCanonicalAllocation(
    OpBuilder &builder, Location loc, Type elementType,
    ArrayRef<Value> dimSizes) {
  /// Create a memref type with the specified dimensions
  SmallVector<int64_t> shape(dimSizes.size(), ShapedType::kDynamic);
  auto memrefType = MemRefType::get(shape, elementType);

  /// Create the allocation
  auto allocOp =
      builder.create<memref::AllocOp>(loc, memrefType, dimSizes);
  return allocOp.getResult();
}

void NestedAllocTransformer::transferMetadata(Operation *oldAlloc,
                                               Operation *newAlloc) {
  if (!oldAlloc || !newAlloc)
    return;

  /// Transfer attributes from old to new allocation
  /// This preserves analysis metadata attached to the allocation
  for (auto namedAttr : oldAlloc->getAttrs()) {
    if (namedAttr.getName().strref().starts_with("arts.")) {
      newAlloc->setAttr(namedAttr.getName(), namedAttr.getValue());
    }
  }
}

void NestedAllocTransformer::duplicateDeallocations(Value oldAlloc,
                                                     Value newAlloc,
                                                     OpBuilder &builder) {
  /// Find all deallocation operations for the old allocation
  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : oldAlloc.getUsers()) {
    if (auto deallocOp = dyn_cast<memref::DeallocOp>(user))
      deallocs.push_back(deallocOp);
  }

  /// Create matching deallocations for the new allocation
  for (auto deallocOp : deallocs) {
    builder.setInsertionPoint(deallocOp);
    builder.create<memref::DeallocOp>(deallocOp.getLoc(), newAlloc);
  }
}

} // namespace arts
} // namespace mlir
