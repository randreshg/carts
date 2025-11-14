///==========================================================================///
/// File: ArrayOfPtrsTransformer.cpp
/// Implements the ArrayOfPtrsTransformer for eliminating array-of-pointers
/// wrapper patterns.
///==========================================================================///

#include "arts/Transforms/ArrayOfPtrsTransformer.h"
#include "arts/Utils/ArtsDebug.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

ARTS_DEBUG_SETUP(array_of_ptrs_transformer);

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Statistics
///===----------------------------------------------------------------------===///

void ArrayOfPtrsTransformer::Statistics::print(llvm::raw_ostream &os) const {
  os << "ArrayOfPtrsTransformer Statistics:\n"
     << "  Patterns detected: " << patternsDetected << "\n"
     << "  Patterns transformed: " << patternsTransformed << "\n"
     << "  Accesses rewritten: " << accessesRewritten << "\n";
}

///===----------------------------------------------------------------------===///
/// Constructor
///===----------------------------------------------------------------------===///

ArrayOfPtrsTransformer::ArrayOfPtrsTransformer(MLIRContext *context,
                                               Config config)
    : ctx_(context), config_(config) {}

///===----------------------------------------------------------------------===///
/// Main Entry Point
///===----------------------------------------------------------------------===///

LogicalResult ArrayOfPtrsTransformer::transform(ModuleOp module,
                                                OpBuilder &builder,
                                                OpRemovalManager &removalMgr) {
  module_ = module;
  stats_ = Statistics();

  ARTS_DEBUG("=== ArrayOfPtrsTransformer::transform ===");

  /// Collect all function operations
  SmallVector<func::FuncOp> functions;
  module.walk([&](func::FuncOp func) { functions.push_back(func); });

  /// For each function, detect and transform array-of-pointers patterns
  for (auto func : functions) {
    /// Collect all rank-0 allocas in the function
    SmallVector<Value> allocas;
    func.walk([&](memref::AllocaOp allocaOp) {
      auto allocaType = allocaOp.getType();
      if (allocaType.getRank() == 0)
        allocas.push_back(allocaOp.getResult());
    });

    /// Try to detect patterns for each alloca
    SmallVector<Pattern> patterns;
    for (Value alloca : allocas) {
      auto pattern = detectPattern(alloca);
      if (pattern) {
        stats_.patternsDetected++;
        patterns.push_back(std::move(*pattern));
      }
    }

    if (patterns.empty()) {
      ARTS_DEBUG("  No array-of-pointers patterns found in function "
                 << func.getName());
      continue;
    }

    ARTS_DEBUG("  Found " << stats_.patternsDetected
                          << " array-of-pointers patterns");

    /// Transform each pattern
    for (auto &pattern : patterns) {
      /// Transform all accesses to use stored allocation directly
      if (failed(transformAccesses(pattern))) {
        module.emitError(
            "Failed to transform accesses for array-of-pointers pattern");
        return failure();
      }

      /// Clean up wrapper alloca
      cleanupPattern(pattern, removalMgr);

      stats_.patternsTransformed++;
    }
  }

  ARTS_DEBUG("  Transformation complete: " << stats_.patternsTransformed
                                           << " patterns transformed");

  return success();
}

///===----------------------------------------------------------------------===///
/// Pattern Detection
///===----------------------------------------------------------------------===///

std::optional<ArrayOfPtrsTransformer::Pattern>
ArrayOfPtrsTransformer::detectPattern(Value alloca) {
  /// Check if this is a rank-0 alloca
  auto allocaType = alloca.getType().dyn_cast<MemRefType>();
  if (!allocaType || allocaType.getRank() != 0)
    return std::nullopt;

  /// Check if element type is a memref
  auto elementType = allocaType.getElementType();
  auto innerMemRefType = elementType.dyn_cast<MemRefType>();
  if (!innerMemRefType)
    return std::nullopt;

  /// Find direct store operations (not in loops) that store AllocOp results
  Operation *storeOp = nullptr;
  Value storedAlloc;
  bool hasLoads = false;

  for (Operation *user : alloca.getUsers()) {
    /// Track loads to ensure there are cascaded accesses to rewrite
    /// Note: Only memref.load is supported; affine is already lowered
    if (auto memLoad = dyn_cast<memref::LoadOp>(user)) {
      if (memLoad.getMemref() == alloca)
        hasLoads = true;
      continue;
    }

    /// Check for memref.store (affine.store not supported - already lowered)
    if (auto memrefStore = dyn_cast<memref::StoreOp>(user)) {
      if (memrefStore.getMemref() != alloca)
        continue;

      /// Must not be inside a loop (direct store)
      if (memrefStore->getParentOfType<scf::ForOp>())
        continue;

      Value candidate = memrefStore.getValue();
      if (!candidate.getDefiningOp<memref::AllocOp>())
        continue;

      storedAlloc = candidate;
      storeOp = memrefStore;
      continue;
    }
  }

  if (!storeOp || !storedAlloc || !hasLoads)
    return std::nullopt;

  ARTS_DEBUG(" - Detected array-of-pointers pattern: alloca="
             << alloca << ", storedAlloc=" << storedAlloc);

  Pattern pattern;
  pattern.alloca = alloca;
  pattern.storedAlloc = storedAlloc;
  pattern.storeOp = storeOp;
  return pattern;
}

///===----------------------------------------------------------------------===///
/// Access Transformation
///===----------------------------------------------------------------------===///

LogicalResult ArrayOfPtrsTransformer::transformAccesses(Pattern &pattern) {
  Value alloca = pattern.alloca;
  Value storedAlloc = pattern.storedAlloc;

  /// Find all uses of the alloca and replace cascaded accesses
  SmallVector<Operation *> loadsToRemove;
  int64_t rewritten = 0;

  /// Collect all load operations from the alloca
  /// Note: Only memref.load is supported; affine is already lowered
  for (Operation *user : alloca.getUsers()) {
    Value loadedValue;

    if (auto memrefLoad = dyn_cast<memref::LoadOp>(user)) {
      if (memrefLoad.getMemref() != alloca)
        continue;

      loadedValue = memrefLoad.getResult();
      loadsToRemove.push_back(memrefLoad);
    } else {
      continue;
    }

    /// Replace all uses of the loaded value with the stored allocation
    /// This handles the cascaded access pattern: memref.load alloca[] +
    /// memref.load
    loadedValue.replaceAllUsesWith(storedAlloc);
    ++rewritten;
  }

  /// Mark loads for removal
  for (Operation *load : loadsToRemove) {
    if (load->use_empty())
      load->erase();
  }

  if (rewritten == 0) {
    ARTS_DEBUG("  Warning: No accesses were transformed");
    return failure();
  }

  stats_.accessesRewritten += rewritten;
  ARTS_DEBUG(" - Transformed " << loadsToRemove.size()
                               << " cascaded accesses to direct usage");
  return success();
}

///===----------------------------------------------------------------------===///
/// Cleanup
///===----------------------------------------------------------------------===///

void ArrayOfPtrsTransformer::cleanupPattern(Pattern &pattern,
                                            OpRemovalManager &removalMgr) {
  /// Mark the store operation for removal
  if (pattern.storeOp)
    removalMgr.markForRemoval(pattern.storeOp);

  /// Mark the alloca for removal
  if (Operation *allocaOp = pattern.alloca.getDefiningOp())
    removalMgr.markForRemoval(allocaOp);

  /// Verify that all remaining users are marked for removal
  if (failed(removalMgr.verifyAllUsersMarked(pattern.alloca,
                                             "Array-of-pointers (alloca)"))) {
    ARTS_DEBUG("  Warning: Not all users of alloca are marked");
  }
}

} // namespace arts
} // namespace mlir
