///===----------------------------------------------------------------------===///
/// File: CanonicalizeMemrefs.cpp
///
/// This pass canonicalizes memref allocations and OpenMP task dependencies.
/// It uses a two-phase approach: complete analysis first, then transformation.
///
/// Key features:
/// 1. Detects nested allocation patterns (double **A -> memref<?x?xf64>)
/// 2. Collects ALL element accesses before any transformation
/// 3. Collects ALL OMP dependencies before any transformation
/// 4. Transforms in a single pass: element accesses + OMP deps
/// 5. Creates arts.omp_dep with proper indices and sizes
///===----------------------------------------------------------------------===///

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/OpRemovalManager.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/Pass/Pass.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>
#include <string>

ARTS_DEBUG_SETUP(canonicalize_memrefs);

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::omp;

namespace {

///===----------------------------------------------------------------------===///
/// Data Structures
///===----------------------------------------------------------------------===///

/// Info about an element load/store access
struct AccessInfo {
  enum class Kind { ElementLoad, ElementStore };

  Kind kind;
  Operation *terminalOp;
  SmallVector<Value> indices;
  SmallVector<Operation *> chainOps;
};

/// Result of tracing a value back to a pattern
struct TraceResult {
  Value canonicalSource;
  SmallVector<Value> indices;
  SmallVector<Operation *> chainOps;
};

/// Info about an OMP dependency
struct DepInfo {
  Value source;
  SmallVector<Value> indices, sizes;
  ArtsMode mode;
  omp::TaskOp taskOp;
  unsigned depVarIndex;
  SmallVector<Operation *> opsToRemove;
};

/// Pattern for a nested allocation structure
struct AllocPattern {
  Value rootAlloc;
  Value wrapperAlloca;
  SmallVector<Value> dimensions;
  SmallVector<Value> hoistedDimensions;
  Type finalElementType;
  scf::ForOp initLoop;
  SmallVector<Operation *> nestedAllocs;
  Operation *storeToWrapper;

  /// Collected during analysis:
  SmallVector<AccessInfo> accesses;
  SmallVector<DepInfo> dependencies;
};

///===----------------------------------------------------------------------===///
/// Static Helper Functions
///===----------------------------------------------------------------------===///

/// Check if alloca is a scalar token container (rank-0 or rank-1 with size ≤ 1)
static bool isScalarTokenContainer(memref::AllocaOp allocaOp) {
  auto type = allocaOp.getType().dyn_cast<MemRefType>();
  if (!type)
    return false;
  return type.getRank() == 0 || (type.getRank() == 1 && type.hasStaticShape() &&
                                 type.getNumElements() <= 1);
}

/// Find the value stored to a token container alloca
static std::optional<Value> getStoredValueToToken(memref::AllocaOp allocaOp) {
  for (Operation *user : allocaOp->getUsers()) {
    if (auto store = dyn_cast<memref::StoreOp>(user)) {
      if (store.getMemref() == allocaOp.getResult())
        return store.getValue();
    }
  }
  return std::nullopt;
}

/// Materialize OpFoldResult to Value
static Value materializeOpFoldResult(OpFoldResult ofr, OpBuilder &builder,
                                     Location loc) {
  if (auto val = ofr.dyn_cast<Value>())
    return val;
  if (auto attr = ofr.dyn_cast<Attribute>())
    return builder.create<arith::ConstantIndexOp>(
        loc, attr.cast<IntegerAttr>().getInt());
  return nullptr;
}

///===----------------------------------------------------------------------===///
/// Pass Implementation
///===----------------------------------------------------------------------===///

struct CanonicalizeMemrefsPass
    : public arts::CanonicalizeMemrefsBase<CanonicalizeMemrefsPass> {

  void runOnOperation() override;

private:
  OpRemovalManager removalMgr;
  DominanceInfo *domInfo = nullptr;
  OpBuilder *globalBuilder = nullptr;

  /// Phase 1: Detect pattern from an allocation
  std::optional<AllocPattern> detectPattern(Value alloc);

  /// Phase 2a: Collect all element accesses
  void collectAllAccesses(AllocPattern &pattern);
  void collectAccessesRecursively(Value current, SmallVector<Value> &indices,
                                  SmallVector<Operation *> &chain,
                                  SmallVector<AccessInfo> &accesses);

  /// Phase 2b: Collect all OMP dependencies
  void collectOmpDependencies(AllocPattern &pattern);
  std::optional<DepInfo> analyzeDepVar(Value depVar, AllocPattern &pattern,
                                       omp::TaskOp taskOp, unsigned depIdx,
                                       omp::ClauseTaskDepend depMode);

  /// Helper: Trace a value back to the pattern
  std::optional<TraceResult> traceToPattern(Value val, AllocPattern &pattern);
  std::optional<TraceResult> traceValueToPattern(Value val,
                                                 AllocPattern &pattern);

  /// Phase 3: Transform the pattern
  LogicalResult transformPattern(AllocPattern &pattern, OpBuilder &builder);
  LogicalResult transformSimpleWrapper(AllocPattern &pattern,
                                       OpBuilder &builder);

  /// Utilities
  Value createCanonicalAllocation(OpBuilder &builder, Location loc,
                                  Type elementType, ArrayRef<Value> dimSizes);
  void transferMetadata(Operation *oldAlloc, Operation *newAlloc);
  void handleDeallocations(AllocPattern &pattern, Value ndAlloc,
                           OpBuilder &builder,
                           llvm::DenseSet<Operation *> &toRemove);
  Value createConstantIndex(OpBuilder &builder, Location loc, int64_t value);
  ArtsMode convertOmpMode(omp::ClauseTaskDepend mode);

  /// Dimension hoisting helpers
  Value hoistValue(Value val, Operation *insertPoint, OpBuilder &builder,
                   DenseMap<Value, Value> &cache);
  bool isPureClonable(Operation *op);

  /// General dependency handling (handles all patterns including static arrays)
  void handleDependencies(ModuleOp module, OpBuilder &builder);
  std::optional<DepInfo> extractDepInfo(Value depVar, omp::TaskOp taskOp,
                                        unsigned depIdx,
                                        omp::ClauseTaskDepend depMode,
                                        OpBuilder &builder);
};

///===----------------------------------------------------------------------===///
/// Main Entry Point
///===----------------------------------------------------------------------===///

void CanonicalizeMemrefsPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *ctx = module.getContext();
  domInfo = &getAnalysis<DominanceInfo>();

  ARTS_INFO_HEADER(CanonicalizeMemrefsPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Count initial omp.task operations for validation
  unsigned initialTaskCount = 0;
  module.walk([&](omp::TaskOp) { ++initialTaskCount; });
  ARTS_DEBUG("Initial omp.task count: " << initialTaskCount);

  OpBuilder builder(ctx);
  globalBuilder = &builder;
  removalMgr.clear();

  /// Collect all allocations (potential roots)
  SmallVector<Value> allocations;
  module.walk([&](Operation *op) {
    if (auto allocOp = dyn_cast<memref::AllocOp>(op))
      allocations.push_back(allocOp.getResult());
    else if (auto allocaOp = dyn_cast<memref::AllocaOp>(op))
      allocations.push_back(allocaOp.getResult());
  });

  /// Process each allocation
  for (Value alloc : allocations) {
    if (removalMgr.isMarkedForRemoval(alloc.getDefiningOp()))
      continue;

    /// Phase 1: Detect pattern
    auto patternOpt = detectPattern(alloc);
    if (!patternOpt)
      continue;

    /// CASE 1: Simple Wrapper Pattern (1D arrays)
    /// No init loop means this is a simple wrapper, not nested allocation.
    /// We do SROA: replace all wrapper loads with actual allocation.
    if (!patternOpt->initLoop && patternOpt->wrapperAlloca) {
      ARTS_DEBUG("Processing simple wrapper pattern: " << alloc);

      /// Phase 2a: Collect all element accesses (reuse existing function)
      collectAllAccesses(*patternOpt);
      ARTS_DEBUG("  Collected " << patternOpt->accesses.size()
                                << " element accesses");

      /// Phase 2b: Collect all OMP dependencies (reuse existing function)
      collectOmpDependencies(*patternOpt);
      ARTS_DEBUG("  Collected " << patternOpt->dependencies.size()
                                << " OMP dependencies");

      /// Phase 3: Transform - SROA for simple wrapper
      if (failed(transformSimpleWrapper(*patternOpt, builder))) {
        ARTS_DEBUG("  Failed to transform simple wrapper pattern");
      }
      continue;
    }

    /// CASE 2: Nested Allocation Pattern (2D arrays)
    ARTS_DEBUG("Found nested allocation pattern: " << alloc);

    /// Phase 2a: Collect all element accesses
    collectAllAccesses(*patternOpt);
    ARTS_DEBUG("  Collected " << patternOpt->accesses.size()
                              << " element accesses");

    /// Phase 2b: Collect all OMP dependencies
    collectOmpDependencies(*patternOpt);
    ARTS_DEBUG("  Collected " << patternOpt->dependencies.size()
                              << " OMP dependencies");

    /// Phase 3: Transform
    if (failed(transformPattern(*patternOpt, builder))) {
      ARTS_DEBUG("  Failed to transform pattern");
    }
  }

  /// Handle remaining OMP dependencies (static arrays, token containers, etc.)
  handleDependencies(module, builder);

  /// Cleanup
  removalMgr.removeAllMarked(module, /*recursive=*/true);

  /// Validate: omp.task count must be preserved
  unsigned finalTaskCount = 0;
  module.walk([&](omp::TaskOp) { ++finalTaskCount; });
  ARTS_DEBUG("Final omp.task count: " << finalTaskCount);

  if (finalTaskCount != initialTaskCount) {
    module.emitError() << "CanonicalizeMemrefs pass corrupted omp.task "
                          "operations: started with "
                       << initialTaskCount << " tasks, ended with "
                       << finalTaskCount << " tasks";
    signalPassFailure();
    return;
  }

  ARTS_INFO_FOOTER(CanonicalizeMemrefsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Phase 1: Pattern Detection
///===----------------------------------------------------------------------===///

std::optional<AllocPattern>
CanonicalizeMemrefsPass::detectPattern(Value alloc) {
  AllocPattern pattern;
  pattern.rootAlloc = alloc;
  pattern.storeToWrapper = nullptr;

  auto allocType = alloc.getType().dyn_cast<MemRefType>();
  if (!allocType)
    return std::nullopt;

  /// Check for Wrapper Alloca (rank-0 alloca storing the pointer)
  if (allocType.getRank() == 0) {
    auto elemType = allocType.getElementType().dyn_cast<MemRefType>();
    if (!elemType)
      return std::nullopt;

    /// This is a wrapper. Look for stores to it.
    for (Operation *user : alloc.getUsers()) {
      if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        if (storeOp.getMemref() == alloc) {
          Value storedVal = storeOp.getValue();

          /// Use getUnderlyingValue to trace through casts to find allocation.
          /// This handles patterns like:
          ///   %alloc = memref.alloc() : memref<10xf64>
          ///   %cast = memref.cast %alloc : memref<10xf64> to memref<?xf64>
          ///   memref.store %cast, %wrapper[]
          Value underlying = arts::getUnderlyingValue(storedVal);
          if (underlying && underlying.getDefiningOp<memref::AllocOp>()) {
            pattern.wrapperAlloca = alloc;
            pattern.rootAlloc = storedVal; // Keep cast result for SROA
            pattern.storeToWrapper = storeOp;
            allocType = storedVal.getType().cast<MemRefType>();
            break;
          }
        }
      }
    }
    if (!pattern.wrapperAlloca)
      return std::nullopt;
  }

  /// At this point we have:
  /// - pattern.wrapperAlloca set (or null if direct alloc)
  /// - pattern.rootAlloc pointing to the actual allocation
  /// - allocType is the type of rootAlloc

  auto elemType = allocType.getElementType();

  /// =======================================================================
  /// CASE 1: Simple Wrapper Pattern (1D arrays like int *A)
  /// =======================================================================
  /// Pattern: memref<memref<?xT>> wrapper storing memref<?xT> allocation
  ///          where T is a SCALAR type (i32, f64, etc.), not a memref
  ///
  /// Example C code:
  ///   int *A = (int *)malloc(N * sizeof(int));
  ///
  /// MLIR before canonicalization:
  ///   %wrapper = memref.alloca() : memref<memref<?xi32>>
  ///   %alloc = memref.alloc(%N) : memref<?xi32>
  ///   memref.store %alloc, %wrapper[]
  ///   %loaded = memref.load %wrapper[]   // Creates new SSA value!
  ///   // All accesses use %loaded[i], not %alloc[i]
  ///
  /// After canonicalization (SROA):
  ///   %alloc = memref.alloc(%N) : memref<?xi32>
  ///   // All uses of %loaded replaced with %alloc
  ///   // Wrapper alloca removed
  ///
  if (pattern.wrapperAlloca && !elemType.isa<MemRefType>()) {
    ARTS_DEBUG("Detected simple wrapper pattern (1D array)");

    /// Get array size from allocation
    OpBuilder b(pattern.rootAlloc.getContext());
    b.setInsertionPointAfter(pattern.rootAlloc.getDefiningOp());
    Location loc = pattern.rootAlloc.getLoc();

    if (allocType.getRank() >= 1) {
      /// Get underlying allocation (traces through casts)
      Value underlying = arts::getUnderlyingValue(pattern.rootAlloc);
      auto underlyingAlloc =
          underlying ? underlying.getDefiningOp<memref::AllocOp>() : nullptr;
      auto underlyingType =
          underlying ? underlying.getType().cast<MemRefType>() : allocType;

      if (allocType.isDynamicDim(0)) {
        /// Case 1: underlying alloc has dynamic sizes
        if (underlyingAlloc && !underlyingAlloc.getDynamicSizes().empty()) {
          pattern.dimensions.push_back(underlyingAlloc.getDynamicSizes()[0]);
        }
        /// Case 2: rootAlloc is cast from static alloc (e.g., memref<10xf64>)
        else if (underlyingType.hasStaticShape()) {
          pattern.dimensions.push_back(b.create<arith::ConstantIndexOp>(
              loc, underlyingType.getDimSize(0)));
        } else {
          ARTS_DEBUG("  Cannot get dimension from allocation");
          return std::nullopt;
        }
      } else {
        /// Static dimension in allocType
        pattern.dimensions.push_back(
            b.create<arith::ConstantIndexOp>(loc, allocType.getDimSize(0)));
      }
    }

    pattern.finalElementType = elemType;
    pattern.initLoop = nullptr; /// No init loop for simple wrappers

    return pattern;
  }

  /// =======================================================================
  /// CASE 2: Nested Allocation Pattern (2D arrays like double **A)
  /// =======================================================================
  /// Pattern: memref<?xmemref<?xT>> with initialization loop
  /// rootAlloc must be an array of pointers (memref<?xmemref<...>>)
  if (!elemType.isa<MemRefType>())
    return std::nullopt;

  /// Get outer dimension size
  OpBuilder b(pattern.rootAlloc.getContext());
  b.setInsertionPointAfter(pattern.rootAlloc.getDefiningOp());

  Value outerSize;
  if (allocType.isDynamicDim(0)) {
    if (auto allocOp = pattern.rootAlloc.getDefiningOp<memref::AllocOp>()) {
      if (allocOp.getDynamicSizes().empty())
        return std::nullopt;
      outerSize = allocOp.getDynamicSizes()[0];
    } else {
      return std::nullopt;
    }
  } else {
    outerSize = b.create<arith::ConstantIndexOp>(pattern.rootAlloc.getLoc(),
                                                 allocType.getDimSize(0));
  }
  pattern.dimensions.push_back(outerSize);

  /// Find initialization loop with nested allocations
  /// Two patterns:
  /// 1. Direct: store %inner_alloc, %rootAlloc[%i]
  /// 2. Indirect: %loaded = load %wrapper[] ; store %inner_alloc, %loaded[%i]
  bool foundLoop = false;

  /// Helper lambda to check a store and extract loop/inner alloc
  auto checkStoreForPattern = [&](memref::StoreOp storeOp) -> bool {
    if (auto forOp = storeOp->getParentOfType<scf::ForOp>()) {
      Value storedVal = storeOp.getValue();
      if (auto innerAlloc = storedVal.getDefiningOp<memref::AllocOp>()) {
        pattern.initLoop = forOp;
        pattern.nestedAllocs.push_back(innerAlloc);

        auto innerType = innerAlloc.getType().cast<MemRefType>();
        Value innerSize;
        if (innerType.isDynamicDim(0)) {
          if (innerAlloc.getDynamicSizes().empty())
            return false;
          innerSize = innerAlloc.getDynamicSizes()[0];
        } else {
          OpBuilder innerB(forOp);
          innerSize = innerB.create<arith::ConstantIndexOp>(
              innerAlloc.getLoc(), innerType.getDimSize(0));
        }
        pattern.dimensions.push_back(innerSize);
        pattern.finalElementType = innerType.getElementType();
        return true;
      }
    }
    return false;
  };

  /// Pattern 1: Direct store to rootAlloc
  for (Operation *user : pattern.rootAlloc.getUsers()) {
    if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      if (storeOp.getMemref() == pattern.rootAlloc) {
        if (checkStoreForPattern(storeOp)) {
          foundLoop = true;
          break;
        }
      }
    }
  }

  /// Pattern 2: Store to a value loaded from wrapper
  /// The pattern is: load wrapper[] → rootAlloc; then store inner_alloc to
  /// loaded_val[i]
  if (!foundLoop && pattern.wrapperAlloca) {
    /// Find loads from the wrapper
    for (Operation *wrapperUser : pattern.wrapperAlloca.getUsers()) {
      if (auto loadOp = dyn_cast<memref::LoadOp>(wrapperUser)) {
        if (loadOp.getMemref() == pattern.wrapperAlloca) {
          /// This load gives us the rootAlloc value
          /// Now find stores to this loaded value
          for (Operation *loadUser : loadOp.getResult().getUsers()) {
            if (auto storeOp = dyn_cast<memref::StoreOp>(loadUser)) {
              if (storeOp.getMemref() == loadOp.getResult()) {
                if (checkStoreForPattern(storeOp)) {
                  foundLoop = true;
                  break;
                }
              }
            }
          }
          if (foundLoop)
            break;
        }
      }
      /// Also check affine.load (cholesky uses affine.load before lowering)
      if (auto loadOp = dyn_cast<affine::AffineLoadOp>(wrapperUser)) {
        if (loadOp.getMemref() == pattern.wrapperAlloca) {
          for (Operation *loadUser : loadOp.getResult().getUsers()) {
            if (auto storeOp = dyn_cast<memref::StoreOp>(loadUser)) {
              if (storeOp.getMemref() == loadOp.getResult()) {
                if (checkStoreForPattern(storeOp)) {
                  foundLoop = true;
                  break;
                }
              }
            }
          }
          if (foundLoop)
            break;
        }
      }
    }
  }

  if (!foundLoop)
    return std::nullopt;

  /// Check if the init loop has results (produces values)
  /// If so, we cannot simply remove it - the results are used by surrounding
  /// code
  if (pattern.initLoop.getNumResults() > 0) {
    ARTS_DEBUG("  Skipping pattern: init loop has "
               << pattern.initLoop.getNumResults()
               << " results that would break control flow");
    return std::nullopt;
  }

  return pattern;
}

///===----------------------------------------------------------------------===///
/// Phase 2a: Collect Element Accesses
///===----------------------------------------------------------------------===///

void CanonicalizeMemrefsPass::collectAllAccesses(AllocPattern &pattern) {
  SmallVector<Value> roots;
  if (pattern.wrapperAlloca)
    roots.push_back(pattern.wrapperAlloca);
  roots.push_back(pattern.rootAlloc);

  for (Value root : roots) {
    if (!root)
      continue;
    SmallVector<Value> indices;
    SmallVector<Operation *> chain;
    collectAccessesRecursively(root, indices, chain, pattern.accesses);
  }
}

void CanonicalizeMemrefsPass::collectAccessesRecursively(
    Value current, SmallVector<Value> &indices, SmallVector<Operation *> &chain,
    SmallVector<AccessInfo> &accesses) {

  auto memType = current.getType().dyn_cast<MemRefType>();
  if (!memType)
    return;

  for (Operation *user : current.getUsers()) {
    /// Skip ops already marked for removal
    if (removalMgr.isMarkedForRemoval(user))
      continue;

    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      if (loadOp.getMemref() != current)
        continue;

      SmallVector<Value> newIndices = indices;
      SmallVector<Operation *> newChain = chain;

      /// Add indices from this load (if not rank-0)
      if (memType.getRank() > 0) {
        newIndices.append(loadOp.getIndices().begin(),
                          loadOp.getIndices().end());
      }
      newChain.push_back(loadOp);

      if (loadOp.getType().isa<MemRefType>()) {
        /// Intermediate load (returns row pointer), recurse
        collectAccessesRecursively(loadOp.getResult(), newIndices, newChain,
                                   accesses);
      } else {
        /// Terminal element load
        AccessInfo info;
        info.kind = AccessInfo::Kind::ElementLoad;
        info.terminalOp = loadOp;
        info.indices = newIndices;
        info.chainOps = newChain;
        accesses.push_back(std::move(info));
      }
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      if (storeOp.getMemref() != current)
        continue;

      /// Skip stores of memref values (these are init loop stores)
      if (storeOp.getValue().getType().isa<MemRefType>())
        continue;

      SmallVector<Value> newIndices = indices;
      if (memType.getRank() > 0) {
        newIndices.append(storeOp.getIndices().begin(),
                          storeOp.getIndices().end());
      }

      AccessInfo info;
      info.kind = AccessInfo::Kind::ElementStore;
      info.terminalOp = storeOp;
      info.indices = newIndices;
      info.chainOps = chain;
      accesses.push_back(std::move(info));
    }
  }
}

///===----------------------------------------------------------------------===///
/// Phase 2b: Collect OMP Dependencies
///===----------------------------------------------------------------------===///

void CanonicalizeMemrefsPass::collectOmpDependencies(AllocPattern &pattern) {
  getOperation().walk([&](omp::TaskOp taskOp) {
    auto dependVars = taskOp.getDependVars();
    auto dependAttrs = taskOp.getDependsAttr();

    if (dependVars.empty() || !dependAttrs)
      return;

    for (unsigned i = 0; i < dependVars.size(); ++i) {
      Value depVar = dependVars[i];
      auto depAttr = dependAttrs[i].cast<omp::ClauseTaskDependAttr>();
      auto depMode = depAttr.getValue();

      auto depInfo = analyzeDepVar(depVar, pattern, taskOp, i, depMode);
      if (depInfo) {
        pattern.dependencies.push_back(std::move(*depInfo));
      }
    }
  });
}

std::optional<DepInfo>
CanonicalizeMemrefsPass::analyzeDepVar(Value depVar, AllocPattern &pattern,
                                       omp::TaskOp taskOp, unsigned depIdx,
                                       omp::ClauseTaskDepend depMode) {
  DepInfo info;
  info.taskOp = taskOp;
  info.depVarIndex = depIdx;
  info.mode = convertOmpMode(depMode);

  Operation *defOp = depVar.getDefiningOp();
  /// Block arg - skip
  if (!defOp)
    return std::nullopt;

  OpBuilder builder(defOp->getContext());
  builder.setInsertionPoint(taskOp);
  Location loc = taskOp.getLoc();

  /// Case 1: SubView - chunk dependency with offsets/sizes
  if (auto subview = dyn_cast<memref::SubViewOp>(defOp)) {
    Value source = subview.getSource();
    auto traceResult = traceToPattern(source, pattern);
    if (!traceResult)
      return std::nullopt;

    info.source = traceResult->canonicalSource;
    info.indices = traceResult->indices;

    /// Add subview offsets
    for (auto offset : subview.getMixedOffsets()) {
      if (auto val = offset.dyn_cast<Value>())
        info.indices.push_back(val);
      else {
        auto attr = offset.get<Attribute>().cast<IntegerAttr>();
        info.indices.push_back(
            createConstantIndex(builder, loc, attr.getInt()));
      }
    }

    /// Get sizes from subview
    for (auto size : subview.getMixedSizes()) {
      if (auto val = size.dyn_cast<Value>())
        info.sizes.push_back(val);
      else {
        auto attr = size.get<Attribute>().cast<IntegerAttr>();
        info.sizes.push_back(createConstantIndex(builder, loc, attr.getInt()));
      }
    }

    info.opsToRemove = traceResult->chainOps;
    info.opsToRemove.push_back(subview);
    return info;
  }

  /// Case 2: Alloca (token container) - follow store→value chain
  if (auto allocaOp = dyn_cast<memref::AllocaOp>(defOp)) {
    auto allocaType = allocaOp.getType();
    /// Check if it's a token container (rank-0 or small rank-1)
    bool isTokenContainer =
        allocaType.getRank() == 0 ||
        (allocaType.getRank() == 1 && allocaType.hasStaticShape() &&
         allocaType.getDimSize(0) <= 1);

    if (isTokenContainer) {
      /// Find the store to this alloca and trace the stored value
      for (Operation *user : allocaOp.getResult().getUsers()) {
        if (auto store = dyn_cast<memref::StoreOp>(user)) {
          if (store.getMemref() == allocaOp.getResult()) {
            Value storedVal = store.getValue();
            auto traceResult = traceValueToPattern(storedVal, pattern);
            if (traceResult) {
              info.source = traceResult->canonicalSource;
              info.indices = traceResult->indices;

              /// Element-level dependency, sizes = [1, 1, ...]
              for (size_t d = 0; d < info.indices.size(); ++d)
                info.sizes.push_back(createConstantIndex(builder, loc, 1));

              info.opsToRemove = traceResult->chainOps;
              info.opsToRemove.push_back(store);
              info.opsToRemove.push_back(allocaOp);
              return info;
            }
          }
        }
      }
    }
  }

  /// Case 3: Load - element or row dependency
  if (auto loadOp = dyn_cast<memref::LoadOp>(defOp)) {
    auto traceResult = traceToPattern(loadOp.getMemref(), pattern);
    if (!traceResult)
      return std::nullopt;

    info.source = traceResult->canonicalSource;
    info.indices = traceResult->indices;

    /// Add load indices
    info.indices.append(loadOp.getIndices().begin(), loadOp.getIndices().end());

    /// Element-level sizes
    for (size_t d = 0; d < info.indices.size(); ++d)
      info.sizes.push_back(createConstantIndex(builder, loc, 1));

    info.opsToRemove = traceResult->chainOps;
    info.opsToRemove.push_back(loadOp);
    return info;
  }

  /// Case 4: Direct allocation - whole array dependency
  if (defOp == pattern.rootAlloc.getDefiningOp() ||
      (pattern.wrapperAlloca &&
       defOp == pattern.wrapperAlloca.getDefiningOp())) {
    info.source = pattern.rootAlloc;

    /// Whole array: indices = [0, 0], sizes = [dim0, dim1]
    for (Value dim : pattern.dimensions) {
      info.indices.push_back(createConstantIndex(builder, loc, 0));
      info.sizes.push_back(dim);
    }
    return info;
  }

  return std::nullopt;
}

///===----------------------------------------------------------------------===///
/// Tracing Helpers
///===----------------------------------------------------------------------===///

std::optional<TraceResult>
CanonicalizeMemrefsPass::traceToPattern(Value val, AllocPattern &pattern) {
  TraceResult result;

  /// Direct match to root
  if (val == pattern.rootAlloc) {
    result.canonicalSource = pattern.rootAlloc;
    return result;
  }

  /// Direct match to wrapper
  if (val == pattern.wrapperAlloca) {
    result.canonicalSource = pattern.rootAlloc;
    return result;
  }

  /// Check if it's a load from root/wrapper
  if (auto loadOp = val.getDefiningOp<memref::LoadOp>()) {
    auto innerResult = traceToPattern(loadOp.getMemref(), pattern);
    if (innerResult) {
      result.canonicalSource = innerResult->canonicalSource;
      result.indices = innerResult->indices;
      result.chainOps = innerResult->chainOps;

      /// Add indices from this load
      auto memType = loadOp.getMemref().getType().cast<MemRefType>();
      if (memType.getRank() > 0) {
        result.indices.append(loadOp.getIndices().begin(),
                              loadOp.getIndices().end());
      }
      result.chainOps.push_back(loadOp);
      return result;
    }
  }

  return std::nullopt;
}

std::optional<TraceResult>
CanonicalizeMemrefsPass::traceValueToPattern(Value val, AllocPattern &pattern) {
  /// For element values (not memrefs), we look at the defining load
  if (auto loadOp = val.getDefiningOp<memref::LoadOp>()) {
    auto memResult = traceToPattern(loadOp.getMemref(), pattern);
    if (memResult) {
      TraceResult result;
      result.canonicalSource = memResult->canonicalSource;
      result.indices = memResult->indices;
      result.chainOps = memResult->chainOps;

      /// Add final load indices
      auto memType = loadOp.getMemref().getType().cast<MemRefType>();
      if (memType.getRank() > 0) {
        result.indices.append(loadOp.getIndices().begin(),
                              loadOp.getIndices().end());
      }
      result.chainOps.push_back(loadOp);
      return result;
    }
  }

  return std::nullopt;
}

///===----------------------------------------------------------------------===///
/// Phase 3: Transformation
///===----------------------------------------------------------------------===///

LogicalResult CanonicalizeMemrefsPass::transformPattern(AllocPattern &pattern,
                                                        OpBuilder &builder) {
  Operation *insertPoint = pattern.rootAlloc.getDefiningOp();

  /// 1. Hoist all dimensions
  DenseMap<Value, Value> hoistCache;
  pattern.hoistedDimensions.clear();

  for (Value dim : pattern.dimensions) {
    Value hoisted = hoistValue(dim, insertPoint, builder, hoistCache);
    if (!hoisted) {
      ARTS_DEBUG("  FAILED: Cannot hoist dimension " << dim);
      ARTS_DEBUG(
          "  Pattern rejected - dimension does not dominate insertion point");
      return failure();
    }
    pattern.hoistedDimensions.push_back(hoisted);
  }
  ARTS_DEBUG("  Successfully hoisted " << pattern.hoistedDimensions.size()
                                       << " dimensions");

  /// 2. Create N-D allocation before rootAlloc
  builder.setInsertionPoint(insertPoint);
  Value ndAlloc = createCanonicalAllocation(builder, pattern.rootAlloc.getLoc(),
                                            pattern.finalElementType,
                                            pattern.hoistedDimensions);

  transferMetadata(pattern.rootAlloc.getDefiningOp(), ndAlloc.getDefiningOp());

  ARTS_DEBUG("  Created N-D allocation: " << ndAlloc);

  /// Build unified removal set
  llvm::DenseSet<Operation *> toRemove;

  /// 3. Transform element accesses
  for (auto &access : pattern.accesses) {
    /// Add chain ops to removal set
    for (auto *op : access.chainOps)
      toRemove.insert(op);

    switch (access.kind) {
    case AccessInfo::Kind::ElementLoad: {
      auto loadOp = cast<memref::LoadOp>(access.terminalOp);
      builder.setInsertionPoint(loadOp);
      auto newLoad = builder.create<memref::LoadOp>(loadOp.getLoc(), ndAlloc,
                                                    access.indices);
      loadOp.replaceAllUsesWith(newLoad.getResult());
      toRemove.insert(loadOp);
      break;
    }
    case AccessInfo::Kind::ElementStore: {
      auto storeOp = cast<memref::StoreOp>(access.terminalOp);
      builder.setInsertionPoint(storeOp);
      builder.create<memref::StoreOp>(storeOp.getLoc(), storeOp.getValue(),
                                      ndAlloc, access.indices);
      toRemove.insert(storeOp);
      break;
    }
    }
  }
  ARTS_DEBUG("  Transformed " << pattern.accesses.size()
                              << " element accesses");

  /// 4. Transform OMP dependencies
  for (auto &dep : pattern.dependencies) {
    builder.setInsertionPoint(dep.taskOp);

    /// Create arts.omp_dep with proper indices and sizes
    auto ompDepOp = builder.create<arts::OmpDepOp>(
        dep.taskOp.getLoc(), ndAlloc.getType(), dep.mode, ndAlloc, dep.indices,
        dep.sizes);

    /// Update the task's depend_vars
    dep.taskOp.getDependVarsMutable()[dep.depVarIndex].set(
        ompDepOp.getResult());

    ARTS_DEBUG("  Created OmpDepOp for task at "
               << dep.taskOp.getLoc() << " with " << dep.indices.size()
               << " indices and " << dep.sizes.size() << " sizes");

    /// Mark associated ops for removal
    for (auto *op : dep.opsToRemove)
      toRemove.insert(op);
  }

  /// 5. Handle deallocations
  /// The deallocation can be:
  /// - Direct: dealloc %rootAlloc
  /// - Via wrapper load: %loaded = load %wrapper[] ; dealloc %loaded
  /// - Dealloc loop: for i { %row = load %rootAlloc[i]; dealloc %row } ;
  /// dealloc %rootAlloc
  handleDeallocations(pattern, ndAlloc, builder, toRemove);

  /// 6. Replace polygeist.memref2pointer (NULL CHECKS)
  /// Handle uses of rootAlloc in null check patterns:
  /// %ptr = polygeist.memref2pointer %alloc : memref<?x...> to !llvm.ptr
  /// %cmp = llvm.icmp "eq" %ptr, %null : !llvm.ptr
  for (Operation *user :
       llvm::make_early_inc_range(pattern.rootAlloc.getUsers())) {
    if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(user)) {
      builder.setInsertionPoint(m2p);
      auto newM2P = builder.create<polygeist::Memref2PointerOp>(
          m2p.getLoc(), m2p.getType(), ndAlloc);
      m2p.replaceAllUsesWith(newM2P.getResult());
      toRemove.insert(m2p);
      ARTS_DEBUG("  Replaced memref2pointer for null check");
    }
  }

  /// Also handle via wrapper if applicable
  if (pattern.wrapperAlloca) {
    for (Operation *user :
         llvm::make_early_inc_range(pattern.wrapperAlloca.getUsers())) {
      if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(user)) {
        builder.setInsertionPoint(m2p);
        auto newM2P = builder.create<polygeist::Memref2PointerOp>(
            m2p.getLoc(), m2p.getType(), ndAlloc);
        m2p.replaceAllUsesWith(newM2P.getResult());
        toRemove.insert(m2p);
        ARTS_DEBUG("  Replaced memref2pointer (wrapper) for null check");
      }
    }
  }

  /// 7. Mark old structures for removal
  toRemove.insert(pattern.initLoop);
  toRemove.insert(pattern.rootAlloc.getDefiningOp());
  for (auto *op : pattern.nestedAllocs)
    toRemove.insert(op);
  if (pattern.wrapperAlloca)
    toRemove.insert(pattern.wrapperAlloca.getDefiningOp());
  if (pattern.storeToWrapper)
    toRemove.insert(pattern.storeToWrapper);

  /// 8. Queue all ops for removal
  for (auto *op : toRemove)
    removalMgr.markForRemoval(op);

  ARTS_DEBUG("  Marked " << toRemove.size() << " operations for removal");
  return success();
}

///===----------------------------------------------------------------------===///
/// Transform Simple Wrapper Pattern (SROA)
///===----------------------------------------------------------------------===///

/// For 1D arrays with wrapper pattern, we perform Scalar Replacement of
/// Aggregates (SROA): replace all loads from wrapper with direct use of
/// the actual allocation.
///
/// Before:
///   %wrapper = memref.alloca() : memref<memref<?xi32>>
///   %alloc = memref.alloc(%N) : memref<?xi32>
///   memref.store %alloc, %wrapper[]
///   %loaded = memref.load %wrapper[]
///   memref.store %val, %loaded[%i]
///
/// After:
///   %alloc = memref.alloc(%N) : memref<?xi32>
///   memref.store %val, %alloc[%i]   // Direct use of %alloc
///
LogicalResult
CanonicalizeMemrefsPass::transformSimpleWrapper(AllocPattern &pattern,
                                                OpBuilder &builder) {

  Value actualAlloc = pattern.rootAlloc;
  Value wrapper = pattern.wrapperAlloca;
  Operation *insertPoint = actualAlloc.getDefiningOp();

  /// 1. Hoist dimensions (reuse existing infrastructure)
  DenseMap<Value, Value> hoistCache;
  pattern.hoistedDimensions.clear();

  for (Value dim : pattern.dimensions) {
    Value hoisted = hoistValue(dim, insertPoint, builder, hoistCache);
    if (!hoisted) {
      ARTS_DEBUG("  FAILED: Cannot hoist dimension " << dim);
      return failure();
    }
    pattern.hoistedDimensions.push_back(hoisted);
  }
  ARTS_DEBUG("  Hoisted " << pattern.hoistedDimensions.size() << " dimensions");

  /// 2. Collect all loads from wrapper to replace
  SmallVector<Operation *> wrapperLoads;
  for (Operation *user : wrapper.getUsers()) {
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      if (loadOp.getMemref() == wrapper && loadOp.getIndices().empty())
        wrapperLoads.push_back(loadOp);
    }
    if (auto loadOp = dyn_cast<affine::AffineLoadOp>(user)) {
      if (loadOp.getMemref() == wrapper)
        wrapperLoads.push_back(loadOp);
    }
  }

  /// 3. Replace all wrapper loads with actual allocation (SROA)
  for (Operation *loadOp : wrapperLoads) {
    loadOp->getResult(0).replaceAllUsesWith(actualAlloc);
    removalMgr.markForRemoval(loadOp);
  }
  ARTS_DEBUG("  Replaced " << wrapperLoads.size()
                           << " wrapper loads with actual alloc");

  /// 4. Transform OMP dependencies - create arts.omp_dep
  /// Now that wrapper loads are replaced, dependencies reference actualAlloc
  for (auto &dep : pattern.dependencies) {
    builder.setInsertionPoint(dep.taskOp);

    auto ompDepOp = builder.create<arts::OmpDepOp>(
        dep.taskOp.getLoc(), actualAlloc.getType(), dep.mode, actualAlloc,
        dep.indices, dep.sizes);

    dep.taskOp.getDependVarsMutable()[dep.depVarIndex].set(
        ompDepOp.getResult());

    ARTS_DEBUG("  Created OmpDepOp with " << dep.indices.size() << " indices");

    /// Mark ops for removal
    for (auto *op : dep.opsToRemove)
      removalMgr.markForRemoval(op);
  }

  /// 5. Mark wrapper and store-to-wrapper for removal
  removalMgr.markForRemoval(wrapper.getDefiningOp());
  if (pattern.storeToWrapper)
    removalMgr.markForRemoval(pattern.storeToWrapper);

  /// 6. Handle deallocations (wrapper may have associated deallocs)
  llvm::DenseSet<Operation *> toRemove;
  handleDeallocations(pattern, actualAlloc, builder, toRemove);
  for (auto *op : toRemove)
    removalMgr.markForRemoval(op);

  ARTS_DEBUG("  Simple wrapper transformation complete");
  return success();
}

///===----------------------------------------------------------------------===///
/// Utilities
///===----------------------------------------------------------------------===///

Value CanonicalizeMemrefsPass::createCanonicalAllocation(
    OpBuilder &builder, Location loc, Type elementType,
    ArrayRef<Value> dimSizes) {
  SmallVector<int64_t> shape(dimSizes.size(), ShapedType::kDynamic);
  auto memrefType = MemRefType::get(shape, elementType);
  auto allocOp = builder.create<memref::AllocOp>(loc, memrefType, dimSizes);
  return allocOp.getResult();
}

void CanonicalizeMemrefsPass::transferMetadata(Operation *oldAlloc,
                                               Operation *newAlloc) {
  if (!oldAlloc || !newAlloc)
    return;
  for (auto namedAttr : oldAlloc->getAttrs()) {
    if (namedAttr.getName().strref().starts_with("arts.")) {
      newAlloc->setAttr(namedAttr.getName(), namedAttr.getValue());
    }
  }
}

void CanonicalizeMemrefsPass::handleDeallocations(
    AllocPattern &pattern, Value ndAlloc, OpBuilder &builder,
    llvm::DenseSet<Operation *> &toRemove) {
  /// We need to find and remove the deallocation pattern which can be:
  /// 1. Direct dealloc of rootAlloc
  /// 2. Dealloc loop that frees inner arrays then outer array
  /// 3. Loads from wrapper used for deallocation

  /// Helper to collect all dealloc-related ops starting from a value
  std::function<void(Value)> collectDeallocOps = [&](Value val) {
    if (!val)
      return;

    SmallVector<Operation *> usersToProcess;
    for (Operation *user : val.getUsers())
      usersToProcess.push_back(user);

    for (Operation *user : usersToProcess) {
      /// Skip if already marked
      if (toRemove.count(user))
        continue;

      /// Direct dealloc
      if (auto deallocOp = dyn_cast<memref::DeallocOp>(user)) {
        toRemove.insert(deallocOp);
        continue;
      }

      /// Load from this value - might be for deallocation
      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        if (loadOp.getMemref() != val)
          continue;

        /// Check if this load is used for deallocation
        bool usedForDealloc = false;
        for (Operation *loadUser : loadOp.getResult().getUsers()) {
          if (isa<memref::DeallocOp>(loadUser)) {
            usedForDealloc = true;
            break;
          }
        }

        if (usedForDealloc) {
          /// Mark the load and its dealloc users
          toRemove.insert(loadOp);
          for (Operation *loadUser : loadOp.getResult().getUsers()) {
            if (auto deallocOp = dyn_cast<memref::DeallocOp>(loadUser)) {
              toRemove.insert(deallocOp);
            }
          }
        }
        continue;
      }

      /// For loop - might be deallocation loop
      if (auto forOp = dyn_cast<scf::ForOp>(user)) {
        /// Check if this loop contains deallocations related to our pattern
        bool isDeallocLoop = false;
        forOp.walk([&](memref::DeallocOp deallocOp) {
          /// Check if the dealloc is of a load from our pattern
          Value deallocMem = deallocOp.getMemref();
          if (auto loadOp = deallocMem.getDefiningOp<memref::LoadOp>()) {
            /// Trace back to see if it comes from our pattern
            Value loadSource = loadOp.getMemref();
            if (loadSource == pattern.rootAlloc ||
                loadSource == pattern.wrapperAlloca) {
              isDeallocLoop = true;
            }
            /// Also check for loads from a load of wrapper
            if (auto outerLoad = loadSource.getDefiningOp<memref::LoadOp>()) {
              if (outerLoad.getMemref() == pattern.wrapperAlloca) {
                isDeallocLoop = true;
              }
            }
          }
        });

        if (isDeallocLoop) {
          toRemove.insert(forOp);
        }
      }
    }
  };

  /// Process deallocation ops for rootAlloc
  collectDeallocOps(pattern.rootAlloc);

  /// Process deallocation ops reachable via wrapper
  if (pattern.wrapperAlloca) {
    collectDeallocOps(pattern.wrapperAlloca);

    /// Also find loads from wrapper that are used for deallocation
    for (Operation *user : pattern.wrapperAlloca.getUsers()) {
      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        if (loadOp.getMemref() == pattern.wrapperAlloca) {
          /// This load gives us the rootAlloc, check its users for deallocs
          collectDeallocOps(loadOp.getResult());

          /// If this load is only used for deallocation, mark it for removal
          bool onlyDeallocUses = true;
          for (Operation *loadUser : loadOp.getResult().getUsers()) {
            if (!toRemove.count(loadUser) &&
                !isa<memref::DeallocOp>(loadUser) &&
                !isa<memref::LoadOp>(loadUser) && !isa<scf::ForOp>(loadUser)) {
              onlyDeallocUses = false;
              break;
            }
          }
          if (onlyDeallocUses ||
              llvm::all_of(loadOp.getResult().getUsers(),
                           [&](Operation *u) { return toRemove.count(u); })) {
            toRemove.insert(loadOp);
          }
        }
      }
    }
  }

  /// Create single dealloc for the new N-D allocation
  /// We need to find an insertion point that:
  /// 1. ndAlloc dominates
  /// 2. Is after all uses of ndAlloc
  //
  /// If ndAlloc is inside a conditional block (scf.if), we should insert
  /// the dealloc at the end of that block, not before func.return

  /// Find the innermost region/block where ndAlloc is defined
  Operation *ndAllocOp = ndAlloc.getDefiningOp();
  Block *ndAllocBlock = ndAllocOp->getBlock();

  /// Find the last use of ndAlloc in the same block or nested blocks
  Operation *lastUseInScope = ndAllocOp;
  for (Operation *user : ndAlloc.getUsers()) {
    if (toRemove.count(user))
      continue;

    /// Check if this use is dominated by ndAlloc
    if (domInfo->dominates(ndAllocOp, user)) {
      /// Find the ancestor operation that's in the same block as ndAlloc
      Operation *ancestor = user;
      while (ancestor && ancestor->getBlock() != ndAllocBlock) {
        ancestor = ancestor->getParentOp();
      }
      if (ancestor && ancestor->isBeforeInBlock(lastUseInScope) == false) {
        lastUseInScope = ancestor;
      }
    }
  }

  /// Insert dealloc after the last use in the same scope
  /// We insert at the end of the block, before any terminator
  if (Operation *terminator = ndAllocBlock->getTerminator()) {
    builder.setInsertionPoint(terminator);
    builder.create<memref::DeallocOp>(ndAlloc.getLoc(), ndAlloc);
    ARTS_DEBUG("  Inserted dealloc for ndAlloc before block terminator");
  }
}

Value CanonicalizeMemrefsPass::createConstantIndex(OpBuilder &builder,
                                                   Location loc,
                                                   int64_t value) {
  return builder.create<arith::ConstantIndexOp>(loc, value);
}

ArtsMode CanonicalizeMemrefsPass::convertOmpMode(omp::ClauseTaskDepend mode) {
  switch (mode) {
  case omp::ClauseTaskDepend::taskdependin:
    return ArtsMode::in;
  case omp::ClauseTaskDepend::taskdependout:
    return ArtsMode::out;
  case omp::ClauseTaskDepend::taskdependinout:
    return ArtsMode::inout;
  }
  llvm_unreachable("Unknown OMP depend mode");
}

///===----------------------------------------------------------------------===///
/// Dimension Hoisting
///===----------------------------------------------------------------------===///

/// Check if an operation is pure and can be safely cloned/hoisted
bool CanonicalizeMemrefsPass::isPureClonable(Operation *op) {
  return isa<arith::ConstantOp, arith::AddIOp, arith::SubIOp, arith::MulIOp,
             arith::DivUIOp, arith::DivSIOp, arith::RemUIOp, arith::RemSIOp,
             arith::AndIOp, arith::OrIOp, arith::XOrIOp, arith::ShLIOp,
             arith::ShRUIOp, arith::ShRSIOp, arith::ExtSIOp, arith::ExtUIOp,
             arith::TruncIOp, arith::IndexCastOp, arith::IndexCastUIOp,
             polygeist::TypeSizeOp>(op);
}

/// Recursively hoist a value to before insertPoint.
/// Returns hoisted value, or nullptr if hoisting is not possible.
Value CanonicalizeMemrefsPass::hoistValue(Value val, Operation *insertPoint,
                                          OpBuilder &builder,
                                          DenseMap<Value, Value> &cache) {
  /// Check cache first
  if (cache.count(val))
    return cache[val];

  /// Check if value already dominates the insertion point
  if (auto *defOp = val.getDefiningOp()) {
    if (domInfo->properlyDominates(defOp, insertPoint)) {
      cache[val] = val;
      return val;
    }
  } else {
    /// Block argument - check if it dominates
    if (domInfo->dominates(val.getParentBlock(), insertPoint->getBlock())) {
      cache[val] = val;
      return val;
    }
    ARTS_DEBUG("  Cannot hoist block argument: " << val);
    return nullptr; /// Block arg that doesn't dominate - can't hoist
  }

  Operation *defOp = val.getDefiningOp();

  /// Only hoist pure arithmetic ops
  if (!isPureClonable(defOp)) {
    ARTS_DEBUG("  Cannot hoist non-pure op: " << *defOp);
    return nullptr;
  }

  /// Recursively hoist all operands
  SmallVector<Value> hoistedOperands;
  for (Value operand : defOp->getOperands()) {
    Value hoisted = hoistValue(operand, insertPoint, builder, cache);
    if (!hoisted) {
      ARTS_DEBUG("  Cannot hoist operand: " << operand);
      return nullptr;
    }
    hoistedOperands.push_back(hoisted);
  }

  /// Clone the op before insertPoint
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(insertPoint);
  Operation *cloned = builder.clone(*defOp);
  for (unsigned i = 0; i < hoistedOperands.size(); i++)
    cloned->setOperand(i, hoistedOperands[i]);

  Value result = cloned->getResult(0);
  cache[val] = result;
  ARTS_DEBUG("  Hoisted: " << *defOp << " -> " << *cloned);
  return result;
}

///===----------------------------------------------------------------------===///
/// General Dependency Handling (All Patterns)
///===----------------------------------------------------------------------===///

std::optional<DepInfo> CanonicalizeMemrefsPass::extractDepInfo(
    Value depVar, omp::TaskOp taskOp, unsigned depIdx,
    omp::ClauseTaskDepend depMode, OpBuilder &builder) {

  /// Skip if already arts.omp_dep
  if (depVar.getDefiningOp<arts::OmpDepOp>())
    return std::nullopt;

  DepInfo info;
  info.taskOp = taskOp;
  info.depVarIndex = depIdx;
  info.mode = convertOmpMode(depMode);
  Location loc = taskOp.getLoc();

  Operation *defOp = depVar.getDefiningOp();

  /// Pattern 1: Token container alloca - follow store→load chain
  if (auto allocaOp = dyn_cast_or_null<memref::AllocaOp>(defOp)) {
    if (isScalarTokenContainer(allocaOp)) {
      if (auto storedVal = getStoredValueToToken(allocaOp)) {
        /// Stored value should be from a memref.load
        if (auto loadOp = storedVal->getDefiningOp<memref::LoadOp>()) {
          info.source = loadOp.getMemref();
          info.indices.append(loadOp.getIndices().begin(),
                              loadOp.getIndices().end());
          for (size_t d = 0; d < info.indices.size(); ++d)
            info.sizes.push_back(createConstantIndex(builder, loc, 1));
          return info;
        }
      }
    }
    /// Non-token alloca - whole array dependency
    info.source = depVar;
    return info;
  }

  /// Pattern 2: memref.alloc - whole array dependency
  if (isa_and_nonnull<memref::AllocOp>(defOp)) {
    info.source = depVar;
    return info;
  }

  /// Pattern 3: SubView - chunk dependency
  if (auto subview = dyn_cast_or_null<memref::SubViewOp>(defOp)) {
    info.source = subview.getSource();
    for (auto offset : subview.getMixedOffsets())
      info.indices.push_back(materializeOpFoldResult(offset, builder, loc));
    for (auto size : subview.getMixedSizes())
      info.sizes.push_back(materializeOpFoldResult(size, builder, loc));
    return info;
  }

  /// Pattern 4: memref.load - element/row dependency
  if (auto loadOp = dyn_cast_or_null<memref::LoadOp>(defOp)) {
    info.source = loadOp.getMemref();
    info.indices.append(loadOp.getIndices().begin(), loadOp.getIndices().end());
    for (size_t d = 0; d < info.indices.size(); ++d)
      info.sizes.push_back(createConstantIndex(builder, loc, 1));
    return info;
  }

  /// Pattern 5: Block argument
  if (depVar.isa<BlockArgument>()) {
    info.source = depVar;
    return info;
  }

  return std::nullopt;
}

void CanonicalizeMemrefsPass::handleDependencies(ModuleOp module,
                                                 OpBuilder &builder) {
  ARTS_DEBUG("=== Handling Remaining OMP Dependencies ===");

  module.walk([&](omp::TaskOp task) {
    auto dependVars = task.getDependVars();
    auto dependAttrs = task.getDependsAttr();
    if (dependVars.empty() || !dependAttrs)
      return;

    for (unsigned i = 0; i < dependVars.size(); ++i) {
      Value depVar = dependVars[i];

      /// Skip already processed (arts.omp_dep)
      if (depVar.getDefiningOp<arts::OmpDepOp>())
        continue;

      /// Set insertion point before task - needed for creating constants in
      /// extractDepInfo
      builder.setInsertionPoint(task);

      auto depAttr = dependAttrs[i].cast<omp::ClauseTaskDependAttr>();
      auto depInfo =
          extractDepInfo(depVar, task, i, depAttr.getValue(), builder);
      if (!depInfo)
        continue;

      /// Create arts.omp_dep using DepInfo fields (insertion point already set)
      auto ompDepOp = builder.create<arts::OmpDepOp>(
          task.getLoc(),
          depInfo->source.getType(), /// result type
          depInfo->mode,             /// ArtsMode
          depInfo->source,           /// db
          depInfo->indices,          /// offset
          depInfo->sizes);           /// size

      task.getDependVarsMutable()[i].set(ompDepOp.getResult());
      ARTS_DEBUG("  Created arts.omp_dep for task dep "
                 << i << " with " << depInfo->indices.size() << " indices");
    }
  });

  ARTS_DEBUG("=== Dependency Handling Complete ===");
}

} // namespace

///===----------------------------------------------------------------------===///
/// Pass Registration
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCanonicalizeMemrefsPass() {
  return std::make_unique<CanonicalizeMemrefsPass>();
}
} // namespace arts
} // namespace mlir
