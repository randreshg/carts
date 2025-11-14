///==========================================================================///
/// File: OmpDepsTransformer.cpp
/// Implements the OmpDepsTransformer for canonicalizing OpenMP task
/// dependencies.
///==========================================================================///

#include "arts/Transforms/OmpDepsTransformer.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsDebug.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"

ARTS_DEBUG_SETUP(omp_deps_transformer);

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Statistics
///===----------------------------------------------------------------------===///

void OmpDepsTransformer::Statistics::print(llvm::raw_ostream &os) const {
  os << "OmpDepsTransformer Statistics:\n"
     << "  Tasks processed: " << tasksProcessed << "\n"
     << "  Dependencies canonicalized: " << depsCanonical << "\n";
}

///===----------------------------------------------------------------------===///
/// Constructor
///===----------------------------------------------------------------------===///

OmpDepsTransformer::OmpDepsTransformer(MLIRContext *context, Config config)
    : ctx_(context), config_(config) {}

///===----------------------------------------------------------------------===///
/// Main Entry Point
///===----------------------------------------------------------------------===///

LogicalResult OmpDepsTransformer::transform(ModuleOp module,
                                            OpBuilder &builder) {
  module_ = module;
  stats_ = Statistics();

  ARTS_DEBUG("=== OmpDepsTransformer::transform ===");

  canonicalizeDependencies();

  ARTS_DEBUG("  Transformation complete: " << stats_.tasksProcessed
                                           << " tasks processed");

  return success();
}

///===----------------------------------------------------------------------===///
/// Dependency Canonicalization
///===----------------------------------------------------------------------===///

void OmpDepsTransformer::canonicalizeDependencies() {
  /// Walk all OpenMP task operations and canonicalize their dependencies
  ARTS_DEBUG(" - Canonicalizing OpenMP task dependencies");

  SmallVector<omp::TaskOp> tasks;
  module_.walk([&](omp::TaskOp task) { tasks.push_back(task); });

  if (tasks.empty()) {
    ARTS_DEBUG("   No OpenMP tasks found");
    return;
  }

  ARTS_DEBUG(" - Found " << tasks.size() << " OpenMP tasks");

  for (auto task : tasks) {
    canonicalizeTask(task);
    stats_.tasksProcessed++;
  }

  ARTS_DEBUG("   Dependency canonicalization complete");
}

void OmpDepsTransformer::canonicalizeTask(omp::TaskOp task) {
  /// Canonicalize dependencies for a single task operation
  /// This handles three types of dependencies:
  /// 1. Scalar dependencies (direct values)
  /// 2. Array element dependencies (via load operations)
  /// 3. Chunk dependencies (via subview operations)

  OpBuilder builder(ctx_);
  Location loc = task.getLoc();

  /// Process dependencies using the new TaskOp API
  auto dependList = task.getDependsAttr();
  if (!dependList) {
    ARTS_DEBUG("    - No dependList found for task");
    return;
  }

  auto dependVars = task.getDependVars();
  ARTS_DEBUG("    - Processing " << dependList.size() << " dependencies");

  for (unsigned i = 0, e = dependList.size(); i < e && i < dependVars.size();
       ++i) {
    /// Get dependency clause and type
    auto depClause = dyn_cast<omp::ClauseTaskDependAttr>(dependList[i]);
    if (!depClause) {
      ARTS_DEBUG("      - Dep " << i << ": not a ClauseTaskDependAttr");
      continue;
    }

    Value depVar = dependVars[i];
    ARTS_DEBUG("      - Dep " << i << ": extracting from " << depVar);

    /// Extract dependency information
    auto depInfo = extractDepInfo(depVar, builder, loc);
    if (!depInfo) {
      ARTS_DEBUG("      - Dep " << i << ": extractDepInfo returned nullopt");
      continue;
    }

    /// Create arts.omp_dep operation to represent the dependency
    builder.setInsertionPoint(task);

    /// Convert ClauseTaskDepend to ArtsMode
    ArtsMode accessMode;
    switch (depClause.getValue()) {
    case omp::ClauseTaskDepend::taskdependin:
      accessMode = ArtsMode::in;
      break;
    case omp::ClauseTaskDepend::taskdependout:
      accessMode = ArtsMode::out;
      break;
    case omp::ClauseTaskDepend::taskdependinout:
      accessMode = ArtsMode::inout;
      break;
    }

    ARTS_DEBUG("      - Dep "
               << i << ": creating arts.omp_dep with source=" << depInfo->source
               << ", indices=" << depInfo->indices.size()
               << ", sizes=" << depInfo->sizes.size());

    /// Create arts.omp_dep operation to represent the dependency
    auto ompDepOp = builder.create<arts::OmpDepOp>(
        loc, accessMode, depInfo->source, depInfo->indices, depInfo->sizes);

    /// Replace the depVar with the new arts.omp_dep operation
    auto dependVarsMutable = task.getDependVarsMutable();
    dependVarsMutable[i].set(ompDepOp.getResult());

    stats_.depsCanonical++;

    ARTS_DEBUG("      - Canonicalized dependency: mode="
               << accessMode << ", indices=" << depInfo->indices.size()
               << ", sizes=" << depInfo->sizes.size());
  }
}

///===----------------------------------------------------------------------===///
/// Helper Methods
///===----------------------------------------------------------------------===///

std::optional<std::pair<Value, SmallVector<Value>>>
OmpDepsTransformer::extractLoadInfo(Operation *op) {
  /// Extract memref and indices from memref.load
  if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
    SmallVector<Value> indices(loadOp.getIndices().begin(),
                               loadOp.getIndices().end());
    ARTS_DEBUG("        extractLoadInfo: memref.load with " << indices.size()
                                                            << " indices");
    return std::make_pair(loadOp.getMemref(), indices);
  }

  return std::nullopt;
}

std::optional<Value> OmpDepsTransformer::followTokenContainer(Value memref) {
  /// Check if memref is an alloca
  auto allocaOp = memref.getDefiningOp<memref::AllocaOp>();
  if (!allocaOp)
    return std::nullopt;

  /// Check if this is a scalar token container (rank-0 or rank-1 with size 1)
  auto allocaType = allocaOp.getType().dyn_cast<MemRefType>();
  bool isTokenContainer = allocaType && (allocaType.getRank() == 0 ||
                                         (allocaType.getRank() == 1 &&
                                          allocaType.hasStaticShape() &&
                                          allocaType.getNumElements() <= 1));

  if (!isTokenContainer)
    return std::nullopt;

  /// Find the store operation that wrote to this token container
  for (Operation *user : allocaOp->getUsers()) {
    /// Check memref.store
    if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      return storeOp.getValue();
    }
    /// Check affine.store
    if (auto storeOp = dyn_cast<affine::AffineStoreOp>(user)) {
      return storeOp.getValue();
    }
  }

  return std::nullopt;
}

std::optional<OmpDepsTransformer::DepInfo>
OmpDepsTransformer::extractDepInfo(Value depVar, OpBuilder &builder,
                                   Location loc) {
  /// Extract dependency information from a value
  /// Handles:
  /// 1. Direct memref allocation
  /// 2. SubView (chunk dependency)
  /// 3. Token container (alloca -> store -> load pattern)
  /// 4. Load operations (memref.load)
  /// 5. Polygeist pointer2memref
  /// 6. Block arguments

  DepInfo info;

  /// Pattern 1: Direct memref allocation
  if (auto allocOp = depVar.getDefiningOp<memref::AllocOp>()) {
    info.source = depVar;
    return info;
  }

  /// Pattern 2: Alloca (check if token container)
  if (auto allocaOp = depVar.getDefiningOp<memref::AllocaOp>()) {
    if (auto actualDep = followTokenContainer(depVar)) {
      /// Token container - follow to actual dependency
      return extractDepInfo(*actualDep, builder, loc);
    }
    /// Not a token container - whole array dependency
    info.source = depVar;
    return info;
  }

  /// Pattern 3: SubView operation (chunk dependency)
  if (auto subviewOp = depVar.getDefiningOp<memref::SubViewOp>()) {
    info.source = subviewOp.getSource();

    /// Extract indices from offsets
    for (auto offset : subviewOp.getMixedOffsets()) {
      Value offsetVal = materializeOpFoldResult(offset, builder, loc);
      if (offsetVal)
        info.indices.push_back(offsetVal);
    }

    /// Extract sizes
    for (auto size : subviewOp.getMixedSizes()) {
      Value sizeVal = materializeOpFoldResult(size, builder, loc);
      if (sizeVal)
        info.sizes.push_back(sizeVal);
    }

    return info;
  }

  /// Pattern 4: Load operation (memref.load)
  Operation *defOp = depVar.getDefiningOp();
  if (defOp) {
    if (auto loadInfo = extractLoadInfo(defOp)) {
      auto [memref, indices] = *loadInfo;

      /// Check if loading from a token container
      if (auto actualDep = followTokenContainer(memref)) {
        return extractDepInfo(*actualDep, builder, loc);
      }

      /// Normal array element dependency
      info.source = memref;
      info.indices = indices;
      return info;
    }
  }

  /// Pattern 5: Polygeist pointer2memref
  if (auto p2mOp = depVar.getDefiningOp<polygeist::Pointer2MemrefOp>()) {
    info.source = depVar;
    return info;
  }

  /// Pattern 6: Block argument (function argument or loop induction variable)
  if (depVar.isa<BlockArgument>()) {
    info.source = depVar;
    return info;
  }

  /// Unable to extract dependency info
  return std::nullopt;
}

std::optional<omp::ClauseTaskDepend>
OmpDepsTransformer::getDepModeFromTask(omp::TaskOp task, Value depVar) {
  auto dependList = task.getDependsAttr();
  auto dependVars = task.getDependVars();

  if (!dependList || dependVars.empty())
    return std::nullopt;

  /// Find the index of depVar in dependVars
  for (unsigned i = 0, e = dependVars.size(); i < e; ++i) {
    if (dependVars[i] == depVar && i < dependList.size()) {
      if (auto depClause = dyn_cast<omp::ClauseTaskDependAttr>(dependList[i])) {
        return depClause.getValue();
      }
    }
  }

  return std::nullopt;
}

Value OmpDepsTransformer::materializeOpFoldResult(OpFoldResult ofr,
                                                  OpBuilder &builder,
                                                  Location loc) {
  if (auto value = ofr.dyn_cast<Value>()) {
    return value;
  } else if (auto attr = ofr.dyn_cast<Attribute>()) {
    auto intAttr = attr.cast<IntegerAttr>();
    return builder.create<arith::ConstantIndexOp>(loc, intAttr.getInt());
  }
  return nullptr;
}

} // namespace arts
} // namespace mlir
