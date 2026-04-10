///===----------------------------------------------------------------------===///
/// File: RaiseMemRefDimensionality.cpp
///
/// This pass raises nested pointer allocations to N-dimensional memrefs and
/// converts OpenMP task dependencies into arts.omp_dep operations.
/// It uses a two-phase approach: complete analysis first, then transformation.
///
/// Key features:
/// 1. Detects nested allocation patterns (double **A -> memref<?x?xf64>)
/// 2. Collects ALL element accesses before any transformation
/// 3. Collects ALL OMP dependencies before any transformation
/// 4. Transforms in a single pass: element accesses + OMP deps
/// 5. Creates arts.omp_dep with proper indices and sizes
///===----------------------------------------------------------------------===///

#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/utils/Debug.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/RemovalUtils.h"
#include "arts/utils/Utils.h"
#include "arts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Arith/Utils/Utils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/Interfaces/CallInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::omp;

#define GEN_PASS_DEF_RAISEMEMREFDIMENSIONALITY
#include "arts/passes/Passes.h.inc"

ARTS_DEBUG_SETUP(raise_memref_dimensionality);

namespace {

static Value getForwardedMemrefAliasSource(Value value) {
  if (!value)
    return nullptr;

  Operation *defOp = value.getDefiningOp();
  if (!defOp)
    return nullptr;

  if (auto castOp = dyn_cast<memref::CastOp>(defOp))
    return castOp.getSource();
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(defOp)) {
    if (unrealized.getInputs().size() == 1 &&
        isa<MemRefType>(unrealized.getInputs().front().getType()))
      return unrealized.getInputs().front();
  }

  return nullptr;
}

static Value getForwardedMemrefAliasResult(Operation *user, Value current) {
  if (!user || !current)
    return nullptr;

  if (auto castOp = dyn_cast<memref::CastOp>(user))
    return castOp.getSource() == current ? castOp.getResult() : Value();
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
    if (unrealized.getInputs().size() == 1 &&
        unrealized.getInputs().front() == current &&
        unrealized.getOutputs().size() == 1 &&
        isa<MemRefType>(unrealized.getOutputs().front().getType()))
      return unrealized.getOutputs().front();
  }

  return nullptr;
}

static bool isMemrefContainerValue(Value value) {
  auto memrefType = dyn_cast_or_null<MemRefType>(value.getType());
  return memrefType && isa<MemRefType>(memrefType.getElementType());
}

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

/// Pre-collected task dependency reference for O(1) lookup
struct TaskDepRef {
  omp::TaskOp taskOp;
  unsigned depIdx;
  omp::ClauseTaskDepend depMode;
};

/// Pattern for a nested allocation structure
struct AllocPattern {
  Value rootAlloc;
  Value wrapperAlloca;
  SmallVector<Value> dimensions;
  SmallVector<Value> hoistedDimensions;
  Type finalElementType;
  SmallVector<Operation *> initLoops;
  SmallVector<Operation *> initStores;
  SmallVector<Operation *> nestedAllocs;
  Operation *storeToWrapper;

  /// Collected during analysis:
  SmallVector<AccessInfo> accesses;
  SmallVector<DepInfo> dependencies;

  /// Outer wrapper allocas that store the root allocation value
  SmallVector<Value> outerWrapperAliases;

  /// Get outermost init loop
  Operation *getOutermostInitLoop() const {
    return initLoops.empty() ? nullptr : initLoops[0];
  }
};

///===----------------------------------------------------------------------===///
/// Static Helper Functions
///===----------------------------------------------------------------------===///

/// Trace a value through wrapper loads back to the root allocation.
///   %inner_wrapper = memref.alloca() : memref<memref<?x...>>
///   %root_alloc = memref.alloc(...)
///   memref.store %root_alloc, %inner_wrapper[]
///   %result = memref.load %inner_wrapper[]  /// val is this load result
static Value traceWrapperLoadToAlloc(Value val) {
  constexpr int MAX_DEPTH = 10;
  for (int depth = 0; depth < MAX_DEPTH; ++depth) {
    auto loadOp = val.getDefiningOp<memref::LoadOp>();
    if (!loadOp)
      break;

    Value wrapper = loadOp.getMemref();
    auto wrapperType = dyn_cast<MemRefType>(wrapper.getType());
    if (!wrapperType || wrapperType.getRank() != 0)
      break;

    /// Find store to this wrapper
    Value nextVal;
    for (Operation *user : wrapper.getUsers()) {
      if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        if (storeOp.getMemref() == wrapper) {
          Value storedVal = storeOp.getValue();
          /// Check if direct allocation (through casts)
          Value underlying = ValueAnalysis::getUnderlyingValue(storedVal);
          if (underlying && underlying.getDefiningOp<memref::AllocOp>())
            return underlying;
          /// Otherwise continue tracing through nested loads
          nextVal = storedVal;
          break;
        }
      }
    }
    if (!nextVal)
      break;
    val = nextVal;
  }
  return Value();
}

/// Check if a rank-0 wrapper alloca's loads are stored to another wrapper.
/// This indicates it's an inner wrapper from an inlined alloc_Nd pattern.
static bool isInnerWrapperOfInlinedPattern(Value alloc) {
  for (Operation *user : alloc.getUsers()) {
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      for (Operation *loadUser : loadOp.getResult().getUsers()) {
        if (auto storeOp = dyn_cast<memref::StoreOp>(loadUser)) {
          auto storeDest = storeOp.getMemref();
          auto storeDestType = dyn_cast<MemRefType>(storeDest.getType());
          if (storeDestType && storeDestType.getRank() == 0 &&
              storeDest.getDefiningOp<memref::AllocaOp>()) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

///===----------------------------------------------------------------------===///
/// Pass Implementation
///===----------------------------------------------------------------------===///

struct RaiseMemRefDimensionalityPass
    : public ::impl::RaiseMemRefDimensionalityBase<
          RaiseMemRefDimensionalityPass> {

  void runOnOperation() override;

private:
  RemovalUtils removalMgr;

  /// Phase 1: Detect pattern from an allocation
  std::optional<AllocPattern> detectPattern(Value alloc);

  /// Phase 2a: Collect all element accesses
  void collectAllAccesses(AllocPattern &pattern);
  void collectAccessesRecursively(Value current, SmallVector<Value> &indices,
                                  SmallVector<Operation *> &chain,
                                  SmallVector<AccessInfo> &accesses);

  /// Phase 2b: Collect all OMP dependencies
  void collectOmpDependencies(
      AllocPattern &pattern,
      const DenseMap<Value, SmallVector<TaskDepRef>> &taskDepsByValue);
  std::optional<DepInfo> analyzeDepVar(Value depVar, AllocPattern &pattern,
                                       omp::TaskOp taskOp, unsigned depIdx,
                                       omp::ClauseTaskDepend depMode);

  /// Helper: Trace a value back to the pattern
  std::optional<TraceResult> traceToPattern(Value val, AllocPattern &pattern);
  std::optional<TraceResult> traceValueToPattern(Value val,
                                                 AllocPattern &pattern);
  bool isPatternAliasRoot(Value val, const AllocPattern &pattern) const;
  void collectOuterWrapperAliases(AllocPattern &pattern);
  bool recordUnsupportedUse(Operation *op, llvm::Twine reason);
  bool hasOnlySupportedUseGraph(const AllocPattern &pattern);
  bool hasOnlySupportedUses(Value value, DenseSet<Value> &visitedValues);
  bool hasOnlySupportedSubviewUses(Value value, DenseSet<Value> &visitedValues);

  /// Phase 3: Transform the pattern
  LogicalResult transformPattern(AllocPattern &pattern, OpBuilder &builder);
  LogicalResult transformSimpleWrapper(AllocPattern &pattern,
                                       OpBuilder &builder);
  void rewriteTracedMemref2PointerUses(AllocPattern &pattern, Value ndAlloc,
                                       OpBuilder &builder,
                                       llvm::DenseSet<Operation *> &toRemove);

  /// Utilities
  Value createCanonicalAllocation(OpBuilder &builder, Location loc,
                                  Type elementType, ArrayRef<Value> dimSizes);
  void transferMetadata(Operation *oldAlloc, Operation *newAlloc);
  void handleDeallocations(AllocPattern &pattern, Value ndAlloc,
                           OpBuilder &builder,
                           llvm::DenseSet<Operation *> &toRemove,
                           DominanceInfo &domInfo);

  /// N-level nested allocation helpers
  bool extractNestedAllocations(Value storedVal, Operation *loopOp,
                                memref::StoreOp storeOp, AllocPattern &pattern,
                                int depth);
  bool isLoadFromValue(Value maybeLoad, Value source);
  bool tracesToRootAlloc(Value val, AllocPattern &pattern);

  Operation *unsupportedUseOp = nullptr;
  std::string unsupportedUseReason;
};

///===----------------------------------------------------------------------===///
/// Main Entry Point
///===----------------------------------------------------------------------===///

void RaiseMemRefDimensionalityPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *ctx = module.getContext();
  ARTS_INFO_HEADER(RaiseMemRefDimensionalityPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Count initial omp.task operations for validation
  unsigned initialTaskCount = 0;
  module.walk([&](omp::TaskOp) { ++initialTaskCount; });
  ARTS_DEBUG("Initial omp.task count: " << initialTaskCount);

  OpBuilder builder(ctx);
  removalMgr.clear();

  /// Pre-collect all task dependency references for O(1) pattern lookup
  DenseMap<Value, SmallVector<TaskDepRef>> taskDepsByValue;
  module.walk([&](omp::TaskOp taskOp) {
    auto dependVars = taskOp.getDependVars();
    auto dependAttrs = taskOp.getDependKindsAttr();
    if (dependVars.empty() || !dependAttrs)
      return;
    for (unsigned i = 0; i < dependVars.size(); ++i) {
      Value depVar = dependVars[i];
      auto depAttr = cast<omp::ClauseTaskDependAttr>(dependAttrs[i]);
      taskDepsByValue[depVar].push_back({taskOp, i, depAttr.getValue()});
    }
  });

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
    if (patternOpt->initLoops.empty() && patternOpt->wrapperAlloca) {
      ARTS_DEBUG("Processing simple wrapper pattern: " << alloc);

      /// Phase 2a: Collect all element accesses (reuse existing function)
      collectAllAccesses(*patternOpt);
      ARTS_DEBUG("  Collected " << patternOpt->accesses.size()
                                << " element accesses");

      /// Phase 2b: Collect all OMP dependencies (reuse existing function)
      collectOmpDependencies(*patternOpt, taskDepsByValue);
      ARTS_DEBUG("  Collected " << patternOpt->dependencies.size()
                                << " OMP dependencies");

      /// Phase 3: Transform - SROA for simple wrapper
      if (failed(transformSimpleWrapper(*patternOpt, builder))) {
        ARTS_DEBUG("  Failed to transform simple wrapper pattern");
        signalPassFailure();
        return;
      }
      continue;
    }

    /// CASE 2: Nested Allocation Pattern (2D arrays)
    unsupportedUseOp = nullptr;
    unsupportedUseReason.clear();
    if (!hasOnlySupportedUseGraph(*patternOpt)) {
      ARTS_ERROR(
          "RaiseMemRefDimensionality matched a nested memref pattern "
          "but cannot rewrite the full use graph"
          << (unsupportedUseReason.empty() ? "" : (": " + unsupportedUseReason))
          << " at " << patternOpt->rootAlloc.getLoc());
      if (unsupportedUseOp) {
        ARTS_ERROR("Unsupported use that blocks nested memref raising: "
                   << unsupportedUseOp->getName() << " at "
                   << unsupportedUseOp->getLoc());
      }
      signalPassFailure();
      return;
    }

    ARTS_DEBUG("Found nested allocation pattern: " << alloc);

    /// Phase 2a: Collect all element accesses
    collectAllAccesses(*patternOpt);
    ARTS_DEBUG("  Collected " << patternOpt->accesses.size()
                              << " element accesses");

    /// Phase 2b: Collect all OMP dependencies
    collectOmpDependencies(*patternOpt, taskDepsByValue);
    ARTS_DEBUG("  Collected " << patternOpt->dependencies.size()
                              << " OMP dependencies");

    /// Phase 3: Transform
    if (failed(transformPattern(*patternOpt, builder))) {
      ARTS_DEBUG("  Failed to transform pattern");
      signalPassFailure();
      return;
    }
  }

  /// Cleanup
  removalMgr.removeAllMarked(module, /*recursive=*/true);

  /// Validate: omp.task count must be preserved
  unsigned finalTaskCount = 0;
  module.walk([&](omp::TaskOp) { ++finalTaskCount; });
  ARTS_DEBUG("Final omp.task count: " << finalTaskCount);

  if (finalTaskCount != initialTaskCount) {
    module.emitError() << "RaiseMemRefDimensionality pass corrupted omp.task "
                          "operations: started with "
                       << initialTaskCount << " tasks, ended with "
                       << finalTaskCount << " tasks";
    signalPassFailure();
    return;
  }

  ARTS_INFO_FOOTER(RaiseMemRefDimensionalityPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Phase 1: Pattern Detection
///===----------------------------------------------------------------------===///

std::optional<AllocPattern>
RaiseMemRefDimensionalityPass::detectPattern(Value alloc) {
  AllocPattern pattern;
  pattern.rootAlloc = alloc;
  pattern.storeToWrapper = nullptr;

  auto allocType = dyn_cast<MemRefType>(alloc.getType());
  if (!allocType)
    return std::nullopt;

  /// Check for Wrapper Alloca (rank-0 alloca storing the pointer)
  if (allocType.getRank() == 0) {
    auto elemType = dyn_cast<MemRefType>(allocType.getElementType());
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
          Value underlying = ValueAnalysis::getUnderlyingValue(storedVal);
          if (underlying && underlying.getDefiningOp<memref::AllocOp>()) {
            pattern.wrapperAlloca = alloc;
            pattern.rootAlloc = storedVal; /// Keep cast result for SROA
            pattern.storeToWrapper = storeOp;
            allocType = cast<MemRefType>(storedVal.getType());
            break;
          }

          /// Handle inlined alloc_Nd pattern: stored value is a load from
          /// another wrapper (the inner wrapper from inlined function)
          ///   %inner_wrapper = memref.alloca() : memref<memref<?x...>>
          ///   %root_alloc = memref.alloc(...)
          ///   memref.store %root_alloc, %inner_wrapper[]
          ///   %result = memref.load %inner_wrapper[]  /// storedVal is this
          ///   memref.store %result, %outer_wrapper[]  /// current store
          if (auto loadOp = storedVal.getDefiningOp<memref::LoadOp>()) {
            Value loadSource = loadOp.getMemref();
            auto loadSourceType = dyn_cast<MemRefType>(loadSource.getType());
            /// Check if load source is a rank-0 wrapper alloca
            if (loadSourceType && loadSourceType.getRank() == 0 &&
                isa<MemRefType>(loadSourceType.getElementType()) &&
                loadSource.getDefiningOp<memref::AllocaOp>()) {
              /// Trace through inner wrapper to find root allocation
              Value rootAlloc = traceWrapperLoadToAlloc(storedVal);
              if (rootAlloc) {
                ARTS_DEBUG("  Detected inlined alloc_Nd pattern");
                pattern.wrapperAlloca = alloc;
                pattern.rootAlloc = rootAlloc;
                pattern.storeToWrapper = storeOp;
                allocType = cast<MemRefType>(rootAlloc.getType());
                break;
              }
            }
          }
        }
      }
    }
    if (!pattern.wrapperAlloca) {
      /// Check if this is an inner wrapper from an inlined alloc_Nd pattern.
      /// If so, skip it - will be handled when processing the outer wrapper.
      if (isInnerWrapperOfInlinedPattern(alloc)) {
        ARTS_DEBUG("  Skipping inner wrapper of inlined alloc_Nd pattern");
        return std::nullopt;
      }
      /// Cleanup: rank-0 wrapper with no store → mark removal, replace trivial
      /// uses (loads) with undef to unblock pass and remove noise wrappers.
      llvm::DenseSet<Operation *> cleanup;
      OpBuilder cleanupBuilder(alloc.getContext());

      for (Operation *user : llvm::make_early_inc_range(alloc.getUsers())) {
        if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
          cleanupBuilder.setInsertionPoint(loadOp);
          auto memType = cast<MemRefType>(loadOp.getType());
          SmallVector<Value> dynSizes;
          for (int64_t d = 0; d < memType.getRank(); ++d) {
            if (memType.isDynamicDim(d)) {
              dynSizes.push_back(
                  arts::createZeroIndex(cleanupBuilder, loadOp.getLoc()));
            }
          }
          Value repl = memref::AllocOp::create(cleanupBuilder, loadOp.getLoc(),
                                               memType, dynSizes);
          loadOp.replaceAllUsesWith(repl);
          cleanup.insert(loadOp);
        } else if (auto affineLoad = dyn_cast<affine::AffineLoadOp>(user)) {
          cleanupBuilder.setInsertionPoint(affineLoad);
          auto memType = cast<MemRefType>(affineLoad.getType());
          SmallVector<Value> dynSizes;
          for (int64_t d = 0; d < memType.getRank(); ++d) {
            if (memType.isDynamicDim(d)) {
              dynSizes.push_back(
                  arts::createZeroIndex(cleanupBuilder, affineLoad.getLoc()));
            }
          }
          Value repl = memref::AllocOp::create(
              cleanupBuilder, affineLoad.getLoc(), memType, dynSizes);
          affineLoad.replaceAllUsesWith(repl);
          cleanup.insert(affineLoad);
        } else if (isa<memref::DeallocOp>(user)) {
          cleanup.insert(user);
        }
      }

      for (auto *op : cleanup)
        removalMgr.markForRemoval(op);
      removalMgr.markForRemoval(alloc.getDefiningOp());

      return std::nullopt;
    }
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
  ///   %loaded = memref.load %wrapper[]   /// Creates new SSA value!
  ///   /// All accesses use %loaded[i], not %alloc[i]
  ///
  /// After canonicalization (SROA):
  ///   %alloc = memref.alloc(%N) : memref<?xi32>
  ///   /// All uses of %loaded replaced with %alloc
  ///   /// Wrapper alloca removed
  ///
  if (pattern.wrapperAlloca && !isa<MemRefType>(elemType)) {
    ARTS_DEBUG("Detected simple wrapper pattern (1D array)");

    /// Get array size from allocation
    OpBuilder b(pattern.rootAlloc.getContext());
    b.setInsertionPointAfter(pattern.rootAlloc.getDefiningOp());
    Location loc = pattern.rootAlloc.getLoc();

    if (allocType.getRank() >= 1) {
      /// Get underlying allocation (traces through casts)
      Value underlying = ValueAnalysis::getUnderlyingValue(pattern.rootAlloc);
      auto underlyingAlloc =
          underlying ? underlying.getDefiningOp<memref::AllocOp>() : nullptr;
      auto underlyingType =
          underlying ? cast<MemRefType>(underlying.getType()) : allocType;

      if (allocType.isDynamicDim(0)) {
        /// Case 1: underlying alloc has dynamic sizes
        if (underlyingAlloc && !underlyingAlloc.getDynamicSizes().empty()) {
          pattern.dimensions.push_back(underlyingAlloc.getDynamicSizes()[0]);
        }
        /// Case 2: rootAlloc is cast from static alloc (e.g., memref<10xf64>)
        else if (underlyingType.hasStaticShape()) {
          pattern.dimensions.push_back(
              arts::createConstantIndex(b, loc, underlyingType.getDimSize(0)));
        } else {
          ARTS_DEBUG("  Cannot get dimension from allocation");
          return std::nullopt;
        }
      } else {
        /// Static dimension in allocType
        pattern.dimensions.push_back(
            arts::createConstantIndex(b, loc, allocType.getDimSize(0)));
      }
    }

    pattern.finalElementType = elemType;
    /// No init loop for simple wrappers (initLoops remains empty)

    return pattern;
  }

  /// =======================================================================
  /// CASE 2: Nested Allocation Pattern (2D arrays like double **A)
  /// =======================================================================
  /// Pattern: memref<?xmemref<?xT>> with initialization loop
  /// rootAlloc must be an array of pointers (memref<?xmemref<...>>)
  if (!isa<MemRefType>(elemType))
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
    outerSize = arts::createConstantIndex(b, pattern.rootAlloc.getLoc(),
                                          allocType.getDimSize(0));
  }
  pattern.dimensions.push_back(outerSize);

  /// Find initialization loop with nested allocations
  /// Two patterns:
  /// 1. Direct: store %inner_alloc, %rootAlloc[%i]
  /// 2. Indirect: %loaded = load %wrapper[] ; store %inner_alloc, %loaded[%i]
  ///
  /// Now supports N-level nesting (e.g., float ***, float ****) by recursively
  /// detecting nested initialization loops.
  bool foundLoop = false;

  /// Helper lambda to check a store and extract loop/inner alloc
  /// Uses recursive extraction to support N-level nesting
  auto checkStoreForPattern = [&](memref::StoreOp storeOp) -> bool {
    Operation *loopOp = arts::findNearestLoop(storeOp.getOperation());
    if (!loopOp)
      return false;
    Value storedVal = storeOp.getValue();
    if (extractNestedAllocations(storedVal, loopOp, storeOp, pattern,
                                 /*depth=*/0)) {
      ARTS_DEBUG("  Found " << pattern.initLoops.size()
                            << "-level nested allocation pattern");
      return true;
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

  if (!foundLoop) {
    return std::nullopt;
  }

  /// Check if the outermost init loop has results (produces values)
  /// If so, we cannot simply remove it - the results are used by surrounding
  /// code
  auto *outermostLoop = pattern.getOutermostInitLoop();
  if (outermostLoop && outermostLoop->getNumResults() > 0) {
    ARTS_DEBUG("  Skipping pattern: init loop has "
               << outermostLoop->getNumResults()
               << " results that would break control flow");
    return std::nullopt;
  }

  collectOuterWrapperAliases(pattern);

  return pattern;
}

///===----------------------------------------------------------------------===///
/// Phase 2a: Collect Element Accesses
///===----------------------------------------------------------------------===///

void RaiseMemRefDimensionalityPass::collectAllAccesses(AllocPattern &pattern) {
  llvm::SetVector<Value> roots;
  if (pattern.wrapperAlloca)
    roots.insert(pattern.wrapperAlloca);
  roots.insert(pattern.rootAlloc);

  /// Add outer wrapper aliases as roots - element accesses may come through
  /// these after inlining alloc_Nd functions
  for (Value outerWrapper : pattern.outerWrapperAliases)
    roots.insert(outerWrapper);

  for (Value root : roots) {
    if (!root)
      continue;
    SmallVector<Value> indices;
    SmallVector<Operation *> chain;
    collectAccessesRecursively(root, indices, chain, pattern.accesses);
  }
}

void RaiseMemRefDimensionalityPass::collectAccessesRecursively(
    Value current, SmallVector<Value> &indices, SmallVector<Operation *> &chain,
    SmallVector<AccessInfo> &accesses) {

  auto memType = dyn_cast<MemRefType>(current.getType());
  if (!memType)
    return;

  if (Value source = getForwardedMemrefAliasSource(current)) {
    collectAccessesRecursively(source, indices, chain, accesses);
    return;
  }

  for (Operation *user : current.getUsers()) {
    /// Skip ops already marked for removal
    if (removalMgr.isMarkedForRemoval(user))
      continue;

    if (Value forwarded = getForwardedMemrefAliasResult(user, current)) {
      SmallVector<Operation *> newChain = chain;
      newChain.push_back(user);
      collectAccessesRecursively(forwarded, indices, newChain, accesses);
      continue;
    }

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

      if (isa<MemRefType>(loadOp.getType())) {
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
      if (isa<MemRefType>(storeOp.getValue().getType()))
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

void RaiseMemRefDimensionalityPass::collectOmpDependencies(
    AllocPattern &pattern,
    const DenseMap<Value, SmallVector<TaskDepRef>> &taskDepsByValue) {
  for (const auto &entry : taskDepsByValue) {
    Value depVar = entry.first;
    for (const auto &ref : entry.second) {
      auto depInfo =
          analyzeDepVar(depVar, pattern, ref.taskOp, ref.depIdx, ref.depMode);
      if (depInfo)
        pattern.dependencies.push_back(std::move(*depInfo));
    }
  }
}

std::optional<DepInfo> RaiseMemRefDimensionalityPass::analyzeDepVar(
    Value depVar, AllocPattern &pattern, omp::TaskOp taskOp, unsigned depIdx,
    omp::ClauseTaskDepend depMode) {
  DepInfo info;
  info.taskOp = taskOp;
  info.depVarIndex = depIdx;
  info.mode = arts::convertOmpMode(depMode);

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
      if (auto val = dyn_cast<Value>(offset))
        info.indices.push_back(val);
      else {
        auto attr = cast<IntegerAttr>(cast<Attribute>(offset));
        info.indices.push_back(
            arts::createConstantIndex(builder, loc, attr.getInt()));
      }
    }

    /// Get sizes from subview
    for (auto size : subview.getMixedSizes()) {
      if (auto val = dyn_cast<Value>(size))
        info.sizes.push_back(val);
      else {
        auto attr = cast<IntegerAttr>(cast<Attribute>(size));
        info.sizes.push_back(
            arts::createConstantIndex(builder, loc, attr.getInt()));
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
                info.sizes.push_back(
                    arts::createConstantIndex(builder, loc, 1));
              info.indices = arts::clampDepIndices(
                  info.source, info.indices, builder, loc, pattern.dimensions);

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
      info.sizes.push_back(arts::createConstantIndex(builder, loc, 1));
    info.indices = arts::clampDepIndices(info.source, info.indices, builder,
                                         loc, pattern.dimensions);

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
      info.indices.push_back(arts::createConstantIndex(builder, loc, 0));
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
RaiseMemRefDimensionalityPass::traceToPattern(Value val,
                                              AllocPattern &pattern) {
  TraceResult result;

  /// Direct match to root or any alias wrapper that resolves to it.
  if (isPatternAliasRoot(val, pattern)) {
    result.canonicalSource = pattern.rootAlloc;
    return result;
  }

  if (Value source = getForwardedMemrefAliasSource(val))
    return traceToPattern(source, pattern);

  /// Check if it's a load from root/wrapper
  if (auto loadOp = val.getDefiningOp<memref::LoadOp>()) {
    auto innerResult = traceToPattern(loadOp.getMemref(), pattern);
    if (innerResult) {
      result.canonicalSource = innerResult->canonicalSource;
      result.indices = innerResult->indices;
      result.chainOps = innerResult->chainOps;

      /// Add indices from this load
      auto memType = cast<MemRefType>(loadOp.getMemref().getType());
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
RaiseMemRefDimensionalityPass::traceValueToPattern(Value val,
                                                   AllocPattern &pattern) {
  /// For element values (not memrefs), we look at the defining load
  if (auto loadOp = val.getDefiningOp<memref::LoadOp>()) {
    auto memResult = traceToPattern(loadOp.getMemref(), pattern);
    if (memResult) {
      TraceResult result;
      result.canonicalSource = memResult->canonicalSource;
      result.indices = memResult->indices;
      result.chainOps = memResult->chainOps;

      /// Add final load indices
      auto memType = cast<MemRefType>(loadOp.getMemref().getType());
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

bool RaiseMemRefDimensionalityPass::isPatternAliasRoot(
    Value val, const AllocPattern &pattern) const {
  if (!val)
    return false;
  if (val == pattern.rootAlloc || val == pattern.wrapperAlloca)
    return true;
  return llvm::is_contained(pattern.outerWrapperAliases, val);
}

void RaiseMemRefDimensionalityPass::collectOuterWrapperAliases(
    AllocPattern &pattern) {
  if (!pattern.wrapperAlloca)
    return;

  llvm::SetVector<Value> discovered;
  llvm::SetVector<Value> worklist;
  worklist.insert(pattern.wrapperAlloca);

  while (!worklist.empty()) {
    Value wrapper = worklist.pop_back_val();

    for (Operation *wrapperUser : wrapper.getUsers()) {
      auto loadOp = dyn_cast<memref::LoadOp>(wrapperUser);
      if (!loadOp || loadOp.getMemref() != wrapper)
        continue;

      /// This load returns the root allocation value. Track any additional
      /// wrapper allocas that store the same loaded value so alias chains of
      /// arbitrary depth continue to trace back to the canonical root.
      for (Operation *loadUser : loadOp.getResult().getUsers()) {
        auto storeOp = dyn_cast<memref::StoreOp>(loadUser);
        if (!storeOp || storeOp.getValue() != loadOp.getResult())
          continue;

        Value outerWrapper = storeOp.getMemref();
        auto outerType = dyn_cast<MemRefType>(outerWrapper.getType());
        if (!outerType || outerType.getRank() != 0 ||
            !isa<MemRefType>(outerType.getElementType()))
          continue;
        if (outerWrapper == pattern.wrapperAlloca)
          continue;

        if (discovered.insert(outerWrapper)) {
          worklist.insert(outerWrapper);
          ARTS_DEBUG("  Found outer wrapper alias: " << outerWrapper);
        }
      }
    }
  }

  pattern.outerWrapperAliases.assign(discovered.begin(), discovered.end());
}

bool RaiseMemRefDimensionalityPass::recordUnsupportedUse(Operation *op,
                                                         llvm::Twine reason) {
  if (!unsupportedUseOp) {
    unsupportedUseOp = op;
    unsupportedUseReason = reason.str();
  }
  if (op)
    ARTS_DEBUG("  " << unsupportedUseReason << ": " << *op);
  else
    ARTS_DEBUG("  " << unsupportedUseReason);
  return false;
}

bool RaiseMemRefDimensionalityPass::hasOnlySupportedUseGraph(
    const AllocPattern &pattern) {
  DenseSet<Value> visitedValues;
  llvm::SetVector<Value> roots;

  if (pattern.wrapperAlloca)
    roots.insert(pattern.wrapperAlloca);
  roots.insert(pattern.rootAlloc);
  for (Value outerWrapper : pattern.outerWrapperAliases)
    roots.insert(outerWrapper);

  for (Value root : roots) {
    if (!hasOnlySupportedUses(root, visitedValues))
      return false;
  }

  return true;
}

bool RaiseMemRefDimensionalityPass::hasOnlySupportedSubviewUses(
    Value value, DenseSet<Value> &visitedValues) {
  if (!value || !isa<MemRefType>(value.getType()))
    return true;
  if (!visitedValues.insert(value).second)
    return true;

  for (Operation *user : value.getUsers()) {
    if (!user)
      continue;

    if (Value forwarded = getForwardedMemrefAliasResult(user, value)) {
      if (!hasOnlySupportedSubviewUses(forwarded, visitedValues))
        return false;
      continue;
    }

    if (auto taskOp = dyn_cast<omp::TaskOp>(user)) {
      (void)taskOp;
      continue;
    }

    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      if (loadOp.getMemref() != value) {
        return recordUnsupportedUse(user, "unsupported subview alias user");
      }
      if (!isa<MemRefType>(loadOp.getType())) {
        return recordUnsupportedUse(
            loadOp, "unsupported scalar escape from subview result");
      }
      if (!hasOnlySupportedSubviewUses(loadOp.getResult(), visitedValues))
        return false;
      continue;
    }

    if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      if (storeOp.getValue() == value &&
          isMemrefContainerValue(storeOp.getMemref())) {
        if (!hasOnlySupportedSubviewUses(storeOp.getMemref(), visitedValues))
          return false;
        continue;
      }
      return recordUnsupportedUse(storeOp, "unsupported subview store escape");
    }

    if (auto subviewOp = dyn_cast<memref::SubViewOp>(user)) {
      if (subviewOp.getSource() != value) {
        return recordUnsupportedUse(subviewOp,
                                    "unsupported subview alias user");
      }
      if (!hasOnlySupportedSubviewUses(subviewOp.getResult(), visitedValues))
        return false;
      continue;
    }

    if (isa<memref::DeallocOp>(user))
      continue;

    return recordUnsupportedUse(
        user, llvm::Twine("unsupported subview/dependency escape through ") +
                  user->getName().getStringRef());
  }

  return true;
}

bool RaiseMemRefDimensionalityPass::hasOnlySupportedUses(
    Value value, DenseSet<Value> &visitedValues) {
  if (!value || !isa<MemRefType>(value.getType()))
    return true;
  if (!visitedValues.insert(value).second)
    return true;

  /// Compile.cpp lowers affine ops before this stage, so the supported graph
  /// here is the memref/scf form that RaiseMemRefDimensionality actually
  /// rewrites.
  for (Operation *user : value.getUsers()) {
    if (!user)
      continue;

    if (Value forwarded = getForwardedMemrefAliasResult(user, value)) {
      if (!hasOnlySupportedUses(forwarded, visitedValues))
        return false;
      continue;
    }

    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      if (loadOp.getMemref() != value) {
        return recordUnsupportedUse(user, "unsupported memref load alias user");
      }

      if (isa<MemRefType>(loadOp.getType()) &&
          !hasOnlySupportedUses(loadOp.getResult(), visitedValues))
        return false;
      continue;
    }

    if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      if (storeOp.getMemref() == value) {
        continue;
      }

      if (storeOp.getValue() == value &&
          isMemrefContainerValue(storeOp.getMemref())) {
        if (!hasOnlySupportedUses(storeOp.getMemref(), visitedValues))
          return false;
        continue;
      }

      return recordUnsupportedUse(storeOp, "unsupported memref store escape");
    }

    if (auto subviewOp = dyn_cast<memref::SubViewOp>(user)) {
      if (subviewOp.getSource() != value) {
        return recordUnsupportedUse(subviewOp,
                                    "unsupported subview alias user");
      }
      if (!hasOnlySupportedSubviewUses(subviewOp.getResult(), visitedValues))
        return false;
      continue;
    }

    if (isa<omp::TaskOp, memref::DeallocOp, polygeist::Memref2PointerOp>(user))
      continue;

    if (isa<CallOpInterface, func::ReturnOp>(user)) {
      return recordUnsupportedUse(
          user, llvm::Twine("unsupported opaque escape through ") +
                    user->getName().getStringRef());
    }

    return recordUnsupportedUse(
        user,
        llvm::Twine("unsupported use in nested-memref raise graph through ") +
            user->getName().getStringRef());
  }

  return true;
}

///===----------------------------------------------------------------------===///
/// Phase 3: Transformation
///===----------------------------------------------------------------------===///

LogicalResult
RaiseMemRefDimensionalityPass::transformPattern(AllocPattern &pattern,
                                                OpBuilder &builder) {
  OpBuilder::InsertionGuard guard(builder);
  DominanceInfo localDomInfo(
      pattern.rootAlloc.getDefiningOp()->getParentOfType<func::FuncOp>());
  Operation *insertPoint = pattern.rootAlloc.getDefiningOp();

  /// 1. Hoist all dimensions
  pattern.hoistedDimensions.clear();

  for (Value dim : pattern.dimensions) {
    Value hoisted = ValueAnalysis::traceValueToDominating(
        dim, insertPoint, builder, localDomInfo, pattern.rootAlloc.getLoc());
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
    /// Add chain ops to removal set ONLY if they have no other uses.
    /// Some chain ops (like intermediate loads) may be used in multiple
    /// places (e.g., scf.if branches) and should not be removed.
    for (auto *op : access.chainOps) {
      bool hasOtherUses = false;
      for (Value result : op->getResults()) {
        for (Operation *user : result.getUsers()) {
          /// Skip if this is the terminal op for this access
          if (user == access.terminalOp)
            continue;
          /// Skip if user is already marked for removal
          if (toRemove.count(user))
            continue;
          /// Skip if user is a terminator
          if (user->hasTrait<OpTrait::IsTerminator>()) {
            hasOtherUses = true;
            break;
          }
          /// Any other user means we should preserve this op
          hasOtherUses = true;
          break;
        }
        if (hasOtherUses)
          break;
      }
      if (!hasOtherUses)
        toRemove.insert(op);
    }

#ifndef NDEBUG
    for (Value idx : access.indices) {
      if (auto *defOp = idx.getDefiningOp())
        assert(!toRemove.count(defOp) &&
               "index references op marked for removal");
    }
#endif

    switch (access.kind) {
    case AccessInfo::Kind::ElementLoad: {
      auto loadOp = cast<memref::LoadOp>(access.terminalOp);
      builder.setInsertionPoint(loadOp);
      auto newLoad = memref::LoadOp::create(builder, loadOp.getLoc(), ndAlloc,
                                            access.indices);
      loadOp.replaceAllUsesWith(newLoad.getResult());
      toRemove.insert(loadOp);
      break;
    }
    case AccessInfo::Kind::ElementStore: {
      auto storeOp = cast<memref::StoreOp>(access.terminalOp);
      builder.setInsertionPoint(storeOp);
      memref::StoreOp::create(builder, storeOp.getLoc(), storeOp.getValue(),
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
    auto ompDepOp =
        arts::OmpDepOp::create(builder, dep.taskOp.getLoc(), ndAlloc.getType(),
                               dep.mode, ndAlloc, dep.indices, dep.sizes);

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
  handleDeallocations(pattern, ndAlloc, builder, toRemove, localDomInfo);

  /// 6. Replace traced memref-to-pointer conversions before recursive cleanup.
  /// Null checks and other pointer-based consumers often use values loaded
  /// through wrapper alias chains instead of the root allocation directly.
  rewriteTracedMemref2PointerUses(pattern, ndAlloc, builder, toRemove);

  /// 7. Mark old structures for removal
  /// For N-level nesting, we need to remove ALL init loops, not just the
  /// outermost The initLoops vector contains loops from outermost to innermost
  if (!pattern.initLoops.empty()) {
    /// Remove the outermost init loop unless it is an OpenMP wsloop.
    /// Parallel init loops can include non-allocation work, so keep them.
    Operation *outermost = pattern.initLoops[0];
    if (!isa<omp::WsloopOp>(outermost)) {
      toRemove.insert(outermost);
      ARTS_DEBUG("  Marking outermost init loop for removal (contains "
                 << pattern.initLoops.size() << " nested loops)");
    } else {
      ARTS_DEBUG("  Keeping omp.wsloop init loop to preserve parallel work");
    }
  }
  for (auto *op : pattern.initStores)
    toRemove.insert(op);
  toRemove.insert(pattern.rootAlloc.getDefiningOp());
  for (auto *op : pattern.nestedAllocs)
    toRemove.insert(op);
  if (pattern.wrapperAlloca)
    toRemove.insert(pattern.wrapperAlloca.getDefiningOp());
  if (pattern.storeToWrapper)
    toRemove.insert(pattern.storeToWrapper);

  /// 7.5. Mark outer wrapper allocas for removal
  /// After rewriting element accesses, the outer wrappers are no longer needed.
  for (Value outerWrapper : pattern.outerWrapperAliases) {
    if (auto *defOp = outerWrapper.getDefiningOp()) {
      /// Only mark the outer wrapper alloca if it has no remaining uses
      /// after our element access rewrites
      if (defOp->use_empty()) {
        toRemove.insert(defOp);
        ARTS_DEBUG("  Marking unused outer wrapper alloca for removal");
      }
    }
  }

  /// 8. Queue all ops for removal
  for (auto *op : toRemove)
    removalMgr.markForRemoval(op);

  ARTS_DEBUG("  Marked " << toRemove.size() << " operations for removal");
  return success();
}

void RaiseMemRefDimensionalityPass::rewriteTracedMemref2PointerUses(
    AllocPattern &pattern, Value ndAlloc, OpBuilder &builder,
    llvm::DenseSet<Operation *> &toRemove) {
  auto parentFunc =
      pattern.rootAlloc.getDefiningOp()->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return;

  SmallVector<polygeist::Memref2PointerOp> memrefToPointers;
  parentFunc.walk(
      [&](polygeist::Memref2PointerOp op) { memrefToPointers.push_back(op); });

  for (polygeist::Memref2PointerOp m2p : memrefToPointers) {
    auto sourceType = dyn_cast<MemRefType>(m2p.getSource().getType());
    if (!sourceType)
      continue;

    /// Wrapper allocas themselves are just stack slots that hold memref values.
    /// Rewriting them to the canonical N-D allocation would change semantics.
    if (sourceType.getRank() == 0 &&
        isa<MemRefType>(sourceType.getElementType()))
      continue;

    auto traceResult = traceToPattern(m2p.getSource(), pattern);
    if (!traceResult)
      continue;

    /// We currently rewrite root-equivalent aliases. Sliced memref-to-pointer
    /// users need explicit subview materialization, which is a separate path.
    if (!traceResult->indices.empty()) {
      ARTS_DEBUG("  Skipping traced memref2pointer with non-empty indices");
      continue;
    }

    builder.setInsertionPoint(m2p);
    auto newM2P = polygeist::Memref2PointerOp::create(builder, m2p.getLoc(),
                                                      m2p.getType(), ndAlloc);
    m2p.replaceAllUsesWith(newM2P.getResult());
    toRemove.insert(m2p);
    ARTS_DEBUG("  Replaced traced memref2pointer through alias chain");

    for (Operation *chainOp : traceResult->chainOps) {
      if (chainOp && chainOp->use_empty())
        toRemove.insert(chainOp);
    }
  }
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
///   memref.store %val, %alloc[%i]   /// Direct use of %alloc
///
LogicalResult
RaiseMemRefDimensionalityPass::transformSimpleWrapper(AllocPattern &pattern,
                                                      OpBuilder &builder) {
  OpBuilder::InsertionGuard guard(builder);
  DominanceInfo localDomInfo(
      pattern.rootAlloc.getDefiningOp()->getParentOfType<func::FuncOp>());

  Value actualAlloc = pattern.rootAlloc;
  Value wrapper = pattern.wrapperAlloca;
  Operation *insertPoint = actualAlloc.getDefiningOp();

  /// 1. Hoist dimensions
  pattern.hoistedDimensions.clear();

  for (Value dim : pattern.dimensions) {
    Value hoisted = ValueAnalysis::traceValueToDominating(
        dim, insertPoint, builder, localDomInfo, pattern.rootAlloc.getLoc());
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

    auto ompDepOp = arts::OmpDepOp::create(builder, dep.taskOp.getLoc(),
                                           actualAlloc.getType(), dep.mode,
                                           actualAlloc, dep.indices, dep.sizes);

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
  handleDeallocations(pattern, actualAlloc, builder, toRemove, localDomInfo);
  for (auto *op : toRemove)
    removalMgr.markForRemoval(op);

  ARTS_DEBUG("  Simple wrapper transformation complete");
  return success();
}

///===----------------------------------------------------------------------===///
/// Utilities
///===----------------------------------------------------------------------===///

Value RaiseMemRefDimensionalityPass::createCanonicalAllocation(
    OpBuilder &builder, Location loc, Type elementType,
    ArrayRef<Value> dimSizes) {
  SmallVector<int64_t> shape;
  SmallVector<Value> dynamicDims;
  shape.reserve(dimSizes.size());
  dynamicDims.reserve(dimSizes.size());

  for (Value dim : dimSizes) {
    if (auto folded = ValueAnalysis::tryFoldConstantIndex(dim)) {
      shape.push_back(*folded);
      continue;
    }
    shape.push_back(ShapedType::kDynamic);
    dynamicDims.push_back(dim);
  }

  auto memrefType = MemRefType::get(shape, elementType);
  auto allocOp = memref::AllocOp::create(builder, loc, memrefType, dynamicDims);
  return allocOp.getResult();
}

void RaiseMemRefDimensionalityPass::transferMetadata(Operation *oldAlloc,
                                                     Operation *newAlloc) {
  if (!oldAlloc || !newAlloc)
    return;
  for (auto namedAttr : oldAlloc->getAttrs()) {
    if (namedAttr.getName().strref().starts_with("arts.")) {
      newAlloc->setAttr(namedAttr.getName(), namedAttr.getValue());
    }
  }
}

void RaiseMemRefDimensionalityPass::handleDeallocations(
    AllocPattern &pattern, Value ndAlloc, OpBuilder &builder,
    llvm::DenseSet<Operation *> &toRemove, DominanceInfo &domInfo) {
  /// We need to find and remove the deallocation pattern which can be:
  /// 1. Direct dealloc of rootAlloc
  /// 2. Dealloc loop that frees inner arrays then outer array
  /// 3. Loads from wrapper used for deallocation
  ///
  /// For N-dimensional arrays (float ***, float ****, etc.), the deallocation
  /// pattern is recursive:
  ///   %val = load %wrapper[]
  ///   for %i {
  ///     %row = load %val[%i]
  ///     for %j {
  ///       %inner = load %row[%j]
  ///       dealloc %inner
  ///     }
  ///     dealloc %row
  ///   }
  ///   dealloc %val

  /// Build patternValues set for tracing
  llvm::DenseSet<Value> patternValues;
  patternValues.insert(pattern.rootAlloc);
  if (pattern.wrapperAlloca)
    patternValues.insert(pattern.wrapperAlloca);
  for (auto *allocOp : pattern.nestedAllocs) {
    if (auto alloc = dyn_cast<memref::AllocOp>(allocOp))
      patternValues.insert(alloc.getResult());
  }

  /// Recursive helper: Check if a value eventually leads to deallocation
  /// through chains of loads. Returns true if any path leads to dealloc.
  std::function<bool(Value, int)> isEventuallyDeallocated;
  isEventuallyDeallocated = [&](Value v, int depth) -> bool {
    if (depth > 10)
      return false; /// Prevent infinite recursion
    for (Operation *user : v.getUsers()) {
      if (isa<memref::DeallocOp>(user))
        return true;
      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        /// If load returns a memref (not scalar), recurse
        if (isa<MemRefType>(loadOp.getResult().getType())) {
          if (isEventuallyDeallocated(loadOp.getResult(), depth + 1))
            return true;
        }
      }
    }
    return false;
  };

  /// Recursive helper: Collect ALL dealloc-related ops from a value
  /// This handles N-dimensional deallocation patterns
  std::function<void(Value, int)> collectDeallocChain;
  collectDeallocChain = [&](Value val, int depth) {
    if (!val || depth > 10)
      return;

    for (Operation *user : val.getUsers()) {
      if (toRemove.count(user))
        continue;

      /// Direct dealloc
      if (auto deallocOp = dyn_cast<memref::DeallocOp>(user)) {
        toRemove.insert(deallocOp);
        ARTS_DEBUG("    Marking dealloc at depth " << depth);
        continue;
      }

      /// Load from this value
      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        if (loadOp.getMemref() != val)
          continue;

        /// Check if this load eventually leads to deallocation
        bool eventuallyDeallocated = false;
        Value loadResult = loadOp.getResult();

        /// For memref results, check recursively
        if (isa<MemRefType>(loadResult.getType())) {
          eventuallyDeallocated =
              isEventuallyDeallocated(loadResult, depth + 1);
        } else {
          /// Scalar result - not a dealloc path
          continue;
        }

        if (eventuallyDeallocated) {
          /// Mark this load and recurse into its result
          toRemove.insert(loadOp);
          ARTS_DEBUG("    Marking intermediate load at depth " << depth);
          collectDeallocChain(loadResult, depth + 1);
        }
        continue;
      }
    }
  };

  /// Helper to collect all dealloc-related ops starting from a value.
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

        /// Use recursive check for N-level patterns
        bool usedForDealloc = false;
        Value loadResult = loadOp.getResult();
        if (isa<MemRefType>(loadResult.getType())) {
          usedForDealloc = isEventuallyDeallocated(loadResult, 0);
        } else {
          /// For scalar loads, check direct dealloc (shouldn't happen)
          for (Operation *loadUser : loadResult.getUsers()) {
            if (isa<memref::DeallocOp>(loadUser)) {
              usedForDealloc = true;
              break;
            }
          }
        }

        if (usedForDealloc) {
          /// Mark the load and recursively collect its dealloc chain
          toRemove.insert(loadOp);
          collectDeallocChain(loadResult, 1);
        }
        continue;
      }

      /// For loop - might be deallocation loop
      if (auto forOp = dyn_cast<scf::ForOp>(user)) {
        /// Check if this loop contains deallocations related to our pattern
        /// For N-level nesting, need to check against rootAlloc, wrapper, and
        /// all nestedAllocs
        bool isDeallocLoop = false;

        forOp.walk([&](memref::DeallocOp deallocOp) {
          /// Check if the dealloc is of a load from our pattern
          Value deallocMem = deallocOp.getMemref();
          if (auto loadOp = deallocMem.getDefiningOp<memref::LoadOp>()) {
            /// Trace back through loads to see if it comes from our pattern
            Value current = loadOp.getMemref();
            int traceDepth = 0;
            while (current && traceDepth < 10) {
              if (patternValues.count(current)) {
                isDeallocLoop = true;
                return;
              }
              /// Try to trace through loads
              if (auto innerLoad = current.getDefiningOp<memref::LoadOp>()) {
                current = innerLoad.getMemref();
                traceDepth++;
              } else {
                break;
              }
            }
          }
        });

        if (isDeallocLoop) {
          toRemove.insert(forOp);
          ARTS_DEBUG("  Marking dealloc loop for removal");
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

  /// Additional pass: Walk ALL ForOps to find deallocation loops
  /// This handles N-level nested patterns (3D, 4D, etc.) where the ForOp
  /// is not a direct user of the pattern values, but contains operations
  /// that use values derived from the pattern through chains of loads.
  {
    /// Find parent function and walk all ForOps
    if (auto parentFunc = pattern.rootAlloc.getDefiningOp()
                              ->getParentOfType<func::FuncOp>()) {
      parentFunc.walk([&](scf::ForOp forOp) {
        /// Skip if already marked for removal
        if (toRemove.count(forOp))
          return;

        /// Check if this loop contains deallocations that trace back to pattern
        bool isDeallocLoop = false;
        forOp.walk([&](memref::DeallocOp deallocOp) {
          if (isDeallocLoop)
            return; /// Already found one

          /// Trace through loads to see if dealloc comes from pattern
          Value current = deallocOp.getMemref();
          for (int depth = 0; depth < 10 && current; ++depth) {
            if (patternValues.count(current)) {
              isDeallocLoop = true;
              return;
            }
            /// Try to trace through loads
            if (auto loadOp = current.getDefiningOp<memref::LoadOp>()) {
              current = loadOp.getMemref();
            } else {
              break;
            }
          }
        });

        if (isDeallocLoop) {
          toRemove.insert(forOp);
          ARTS_DEBUG("  Marking dealloc loop (N-level) for removal at "
                     << forOp.getLoc());
        }
      });
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

  /// Insert dealloc at the end of the block, before any terminator
  if (Operation *terminator = ndAllocBlock->getTerminator()) {
    builder.setInsertionPoint(terminator);
    memref::DeallocOp::create(builder, ndAlloc.getLoc(), ndAlloc);
    ARTS_DEBUG("  Inserted dealloc for ndAlloc before block terminator");
  }
}

///===----------------------------------------------------------------------===///
/// N-Level Nested Allocation Helpers
///===----------------------------------------------------------------------===///

/// Check if maybeLoad is a load operation from the given source value
bool RaiseMemRefDimensionalityPass::isLoadFromValue(Value maybeLoad,
                                                    Value source) {
  if (auto loadOp = maybeLoad.getDefiningOp<memref::LoadOp>()) {
    return loadOp.getMemref() == source;
  }
  if (auto loadOp = maybeLoad.getDefiningOp<affine::AffineLoadOp>()) {
    return loadOp.getMemref() == source;
  }
  return false;
}

/// Recursively extract nested allocation patterns to support N-level nesting
/// (e.g., float ***, float ****, etc.)
///
/// For 3-level (float ***x):
///   for (b) {
///     alloc_1 = malloc(...)           /// Level 1: memref<?xmemref<?xf32>>
///     store alloc_1, root[b]
///     for (c) {
///       row = load root[b]            /// Load from ROOT, not from alloc_1!
///       alloc_2 = malloc(...)         /// Level 2: memref<?xf32>
///       store alloc_2, row[c]
///     }
///   }
///
/// Key insight: at depth N, the store target is loaded from the ROOT allocation
/// through a chain of N loads, NOT from the previous level's allocation
/// directly.
///
/// Returns true if a complete pattern was found (reaching scalar element type)
bool RaiseMemRefDimensionalityPass::extractNestedAllocations(
    Value storedVal, Operation *loopOp, memref::StoreOp storeOp,
    AllocPattern &pattern, int depth) {
  /// Limit recursion depth to prevent infinite loops
  constexpr int MAX_DEPTH = 10;
  if (depth >= MAX_DEPTH) {
    ARTS_DEBUG("  Reached max recursion depth " << MAX_DEPTH);
    return false;
  }

  /// Trace through casts to find the underlying allocation
  Value underlying = ValueAnalysis::getUnderlyingValue(storedVal);
  auto innerAlloc = underlying ? underlying.getDefiningOp<memref::AllocOp>()
                               : storedVal.getDefiningOp<memref::AllocOp>();
  if (!innerAlloc) {
    ARTS_DEBUG("  No inner allocation found at depth " << depth);
    return false;
  }

  /// Record this init loop and allocation
  pattern.initLoops.push_back(loopOp);
  pattern.initStores.push_back(storeOp);
  pattern.nestedAllocs.push_back(innerAlloc);

  auto innerType = cast<MemRefType>(innerAlloc.getType());

  /// Extract dimension for this level
  Value innerSize;
  if (innerType.isDynamicDim(0)) {
    if (innerAlloc.getDynamicSizes().empty()) {
      ARTS_DEBUG("  Dynamic dim but no dynamic sizes at depth " << depth);
      return false;
    }
    innerSize = innerAlloc.getDynamicSizes()[0];
  } else {
    OpBuilder innerB(loopOp);
    innerSize = arts::createConstantIndex(innerB, innerAlloc.getLoc(),
                                          innerType.getDimSize(0));
  }
  pattern.dimensions.push_back(innerSize);

  Type elemType = innerType.getElementType();

  /// Check if we've reached the final scalar element type
  if (!isa<MemRefType>(elemType)) {
    /// Base case: scalar element type - pattern is complete
    pattern.finalElementType = elemType;
    ARTS_DEBUG("  Found scalar element type at depth " << depth << ": "
                                                       << elemType);
    return true;
  }

  /// Recursive case: element type is still a memref, need to find nested init
  /// loop
  ARTS_DEBUG("  Element type is memref at depth "
             << depth << ", searching for nested loop");

  /// Look for nested initialization loop within current loop
  bool foundNested = false;

  /// For nested stores, the target is loaded from ROOT (possibly via wrapper),
  /// not from innerAlloc. Example:
  ///   %row = load %root[%i]    /// Load from root
  ///   store %alloc, %row[%j]   /// Store to loaded row
  loopOp->walk([&](memref::StoreOp nestedStore) {
    if (foundNested)
      return WalkResult::interrupt();

    /// Check if this store is inside a nested loop
    Operation *nestedLoop = arts::findNearestLoop(nestedStore.getOperation());
    if (!nestedLoop || nestedLoop == loopOp) {
      /// Not in a nested loop, skip
      return WalkResult::advance();
    }

    Value storeMem = nestedStore.getMemref();

    /// Check if the store target traces back to the root allocation
    /// through a chain of loads (depth determines expected chain length)
    if (tracesToRootAlloc(storeMem, pattern)) {
      if (extractNestedAllocations(nestedStore.getValue(), nestedLoop,
                                   nestedStore, pattern, depth + 1)) {
        foundNested = true;
        return WalkResult::interrupt();
      }
    }

    return WalkResult::advance();
  });

  if (!foundNested) {
    ARTS_DEBUG("  Could not find nested init loop at depth " << depth);
  }

  return foundNested;
}

/// Check if a value traces back to the root allocation through a chain of loads
bool RaiseMemRefDimensionalityPass::tracesToRootAlloc(Value val,
                                                      AllocPattern &pattern) {
  constexpr int MAX_TRACE_DEPTH = 10;
  Value current = val;

  for (int i = 0; i < MAX_TRACE_DEPTH; ++i) {
    /// Check if we've reached the root or any alias wrapper that resolves to
    /// it.
    if (isPatternAliasRoot(current, pattern))
      return true;

    /// Try to trace through a load
    if (auto loadOp = current.getDefiningOp<memref::LoadOp>()) {
      current = loadOp.getMemref();
      continue;
    }

    /// Can't trace further
    break;
  }

  return false;
}

} // namespace

///===----------------------------------------------------------------------===///
/// Pass Registration
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createRaiseMemRefDimensionalityPass() {
  return std::make_unique<RaiseMemRefDimensionalityPass>();
}
} // namespace arts
} // namespace mlir
