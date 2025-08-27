//===----------------------------------------------------------------------===//
// EDT Lowering Pass - Complete Implementation
// Transforms arts.edt operations into runtime-compatible function calls
//
// This pass implements the comprehensive 6-step EDT lowering process:
// 1. Analyze EDT region for free variables and deps
// 2. Outline EDT region to function with ARTS runtime signature
// 3. Insert parameter packing before EDT (edt_param_pack) - It should include
//    all parameters from the EDT + unique datablock sizes and indices for all
//    deps
// 4. Insert parameter/dependency unpacking in outlined function
// 5. Replace EDT with edt_create call returning GUID
// 6. Add dependency management (record_in_dep, increment_out_latch)
//===----------------------------------------------------------------------===//

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/RegionUtils.h"

#include "llvm/ADT/DenseMap.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

///==========================================================================
/// EdtEnvManager
/// Manages the environment analysis for EDT regions by collecting parameters,
/// constants, and dependencies used in the region.
///==========================================================================

namespace {

/// Manages the environment analysis for EDT regions in lowering
class EdtEnvManager {
public:
  EdtEnvManager(EdtOp edtOp) : edtOp(edtOp) { analyze(); }

  /// Analyze the region and collect parameters/constants
  void analyze() {
    getUsedValuesDefinedAbove(edtOp.getRegion(), capturedValues);

    /// Checks if the value is a constant, if so, ignore it
    auto isConstant = [&](Value val) {
      auto defOp = val.getDefiningOp();
      if (!defOp)
        return false;

      auto constantOp = dyn_cast<arith::ConstantOp>(defOp);
      if (!constantOp)
        return false;
      constants.insert(val);
      return true;
    };

    /// Classify variables into parameters, constants, and captured values
    for (Value val : capturedValues) {
      if (isConstant(val))
        continue;

      /// Only treat integers, indices, or floats as parameters
      if (val.getType().isIntOrIndexOrFloat())
        parameters.insert(val);

      /// Ignore other types, they might be dependencies
    }

    /// Only treat explicit EDT operands as deps
    for (Value operand : edtOp.getDependencies())
      deps.insert(operand);
  }

  ArrayRef<Value> getParameters() const { return parameters.getArrayRef(); }
  ArrayRef<Value> getConstants() const { return constants.getArrayRef(); }
  ArrayRef<Value> getCapturedValues() const {
    return capturedValues.getArrayRef();
  }
  ArrayRef<Value> getDependencies() const { return deps.getArrayRef(); }

private:
  EdtOp edtOp;
  SetVector<Value> capturedValues, parameters, constants, deps;
};

//===----------------------------------------------------------------------===//
// EDT Lowering Pass Implementation
//===----------------------------------------------------------------------===//
struct EdtLoweringPass : public arts::EdtLoweringBase<EdtLoweringPass> {
  ~EdtLoweringPass() { delete AC; }

  void runOnOperation() override;

private:
  /// Core transformation methods
  LogicalResult lowerEdt(EdtOp edtOp);

  /// Function outlining with ARTS signature
  func::FuncOp createOutlinedFunction(EdtOp edtOp, EdtEnvManager &envManager);

  /// Parameter handling
  Value packParameters(Location loc, EdtEnvManager &envManager,
                       DenseMap<Value, unsigned> &valueToPackIndex,
                       SmallVector<Type> &packTypes);

  /// Region outlining
  LogicalResult outlineRegionToFunction(EdtOp edtOp, func::FuncOp targetFunc,
                                        EdtEnvManager &envManager,
                                        const SmallVector<Type> &packTypes,
                                        size_t numUserParams);

  /// Transform dependency uses to use DbSubIndexOp + DepGetPtrOp pattern
  void transformDependencyUses(Block *block, ArrayRef<Value> unpackedDeps);

  /// Helper to create final ptr from dependency struct and indices
  Value createFinalPtr(Location loc, Value depStruct, ValueRange indices,
                       Type resultType);

  /// Dependency satisfaction
  LogicalResult insertDependencyManagement(Location loc, Value edtGuid,
                                           const SmallVector<Value> &deps);

  /// Utilities
  std::string generateUniqueFunctionName();
  bool canLowerEdt(EdtOp edtOp);

  /// State
  unsigned functionCounter = 0;
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//

void EdtLoweringPass::runOnOperation() {
  module = getOperation();
  AC = new ArtsCodegen(module, false);

  ARTS_DEBUG_HEADER(EdtLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Collect all task EDTs to process (innermost first)
  SmallVector<EdtOp> taskEdts;
  module.walk<WalkOrder::PostOrder>([&](EdtOp edtOp) {
    if (canLowerEdt(edtOp))
      taskEdts.push_back(edtOp);
  });

  ARTS_INFO("Found " << taskEdts.size() << " task EDTs to lower");

  /// Process each task EDT
  for (EdtOp edtOp : taskEdts) {
    if (failed(lowerEdt(edtOp))) {
      edtOp.emitError("Failed to lower task EDT");
      return signalPassFailure();
    }
  }

  ARTS_DEBUG_FOOTER(EdtLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););
}

LogicalResult EdtLoweringPass::lowerEdt(EdtOp edtOp) {
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(edtOp);
  Location loc = edtOp.getLoc();
  /// Analyze EDT environment (parameters, constants, deps)
  EdtEnvManager envManager(edtOp);

  /// Create local variables for parameter packing/dedup
  DenseMap<Value, unsigned> valueToPackIndex;
  SmallVector<Type> packTypes;
  ARTS_INFO("Lowering EDT with "
            << envManager.getParameters().size() << " parameters, "
            << envManager.getConstants().size() << " constants, "
            << envManager.getDependencies().size() << " deps");

  /// Create edt outlined function
  func::FuncOp outlinedFunc = createOutlinedFunction(edtOp, envManager);
  if (!outlinedFunc)
    return edtOp.emitError("Failed to create outlined function");

  /// Pack parameters
  Value paramPack =
      packParameters(loc, envManager, valueToPackIndex, packTypes);

  /// Outline region to function and replace EDT
  if (failed(outlineRegionToFunction(edtOp, outlinedFunc, envManager, packTypes,
                                     envManager.getParameters().size()))) {
    return edtOp.emitError("Failed to outline region to function");
  }

  /// Calculate total dependency count (sum of elements in all deps)
  Value depCount = AC->createIntConstant(0, AC->Int32, loc);
  for (Value dep : envManager.getDependencies()) {
    Value numElements = AC->create<DbNumElementsOp>(loc, dep);
    depCount = AC->create<arith::AddIOp>(loc, depCount, numElements);
  }

  /// Create the outline operation at the same location as the EDT
  AC->setInsertionPoint(edtOp);
  Value routeVal = edtOp.getRoute();
  if (!routeVal)
    routeVal = AC->createIntConstant(0, AC->Int32, loc);
  auto outlineOp = AC->create<EdtCreateOp>(loc, paramPack, depCount, routeVal);
  outlineOp->setAttr("outlined_func",
                     AC->getBuilder().getStringAttr(outlinedFunc.getName()));

  /// Insert dependency management after the outline op
  Value edtGuid = outlineOp.getGuid();
  AC->setInsertionPointAfter(outlineOp);
  SmallVector<Value> depsVec(envManager.getDependencies().begin(),
                             envManager.getDependencies().end());
  if (failed(insertDependencyManagement(loc, edtGuid, depsVec)))
    return edtOp.emitError("Failed to insert dependency management");

  /// Replace all uses of EDT with the outlined function result
  if (edtOp->getNumResults() > 0) {
    SmallVector<Value> replacementValues = {outlineOp.getResult()};
    edtOp->replaceAllUsesWith(replacementValues);
  }
  /// Remove original EDT
  edtOp.erase();

  return success();
}

func::FuncOp
EdtLoweringPass::createOutlinedFunction(EdtOp edtOp,
                                        EdtEnvManager &envManager) {
  Location loc = edtOp.getLoc();
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(module);
  std::string funcName = generateUniqueFunctionName();
  auto outlinedFunc = AC->create<func::FuncOp>(loc, funcName, AC->EdtFn);
  outlinedFunc.setPrivate();

  ARTS_INFO("Created outlined function: " << funcName);
  return outlinedFunc;
}

Value EdtLoweringPass::packParameters(
    Location loc, EdtEnvManager &envManager,
    DenseMap<Value, unsigned> &valueToPackIndex, SmallVector<Type> &packTypes) {
  const auto &parameters = envManager.getParameters();
  const auto &deps = envManager.getDependencies();

  SmallVector<Value> packValues;

  /// Pack user parameters first
  for (Value v : parameters) {
    /// Skip llvm.mlir.undef values - they can be easily recreated
    if (auto defOp = v.getDefiningOp()) {
      if (defOp->getName().getStringRef() == "llvm.mlir.undef")
        continue;
    }
    valueToPackIndex.try_emplace(v, packValues.size());
    packTypes.push_back(v.getType());
    packValues.push_back(v);
  }

  /// Insert indices/offsets/sizes for deps into packValues if not already
  /// present
  for (Value dep : deps) {
    auto dbAcquireOp = dep.getDefiningOp<DbAcquireOp>();
    if (!dbAcquireOp)
      continue;

    auto appendIfMissing = [&](Value val) {
      if (!val)
        return;
      /// Skip constants; they will be recreated in outlined function
      if (val.getDefiningOp<arith::ConstantOp>())
        return;
      if (valueToPackIndex.count(val) == 0) {
        valueToPackIndex[val] = packValues.size();
        packTypes.push_back(val.getType());
        packValues.push_back(val);
      }
    };

    for (Value idx : dbAcquireOp.getIndices())
      appendIfMissing(idx);
    for (Value off : dbAcquireOp.getOffsets())
      appendIfMissing(off);
    for (Value sz : dbAcquireOp.getSizes())
      appendIfMissing(sz);
  }

  if (packValues.empty()) {
    ARTS_INFO("No parameters to pack, creating empty EdtParamPackOp");
    auto emptyType = MemRefType::get({0}, AC->Int64);
    return AC->create<EdtParamPackOp>(loc, TypeRange{emptyType}, ValueRange{});
  }

  ARTS_DEBUG_REGION({
    ARTS_INFO("Creating parameter pack for " << packValues.size() << " items");
    for (size_t i = 0; i < packValues.size(); ++i) {
      ARTS_INFO("  packValues[" << i << "]: " << packValues[i]
                                << " type: " << packValues[i].getType());
    }
  });

  auto memrefType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
  ARTS_INFO("About to create EdtParamPackOp with type: " << memrefType);
  auto packOp = AC->create<EdtParamPackOp>(loc, TypeRange{memrefType},
                                           ValueRange(packValues));
  return packOp;
}

LogicalResult EdtLoweringPass::outlineRegionToFunction(
    EdtOp edtOp, func::FuncOp targetFunc, EdtEnvManager &envManager,
    const SmallVector<Type> &packTypes, size_t numUserParams) {
  Location loc = edtOp.getLoc();
  auto &builder = AC->getBuilder();
  OpBuilder::InsertionGuard IG(builder);
  auto *entryBlock = targetFunc.addEntryBlock();
  AC->setInsertionPointToStart(entryBlock);

  /// Insert parameter and dependency unpacking
  auto args = entryBlock->getArguments();
  Value paramv = args[1];
  Value depv = args[3];

  const auto &parameters = envManager.getParameters();
  SmallVector<Value> deps(envManager.getDependencies().begin(),
                          envManager.getDependencies().end());
  ARTS_INFO("analyzeDependencies returned " << deps.size() << " dependencies");
  for (size_t i = 0; i < deps.size(); ++i) {
    ARTS_INFO(
        "  dep[" << i << "] type: " << deps[i].getType() << ", defOp: "
                 << (deps[i].getDefiningOp()
                         ? deps[i].getDefiningOp()->getName().getStringRef()
                         : "none"));
  }
  SmallVector<Value> unpackedParams;

  if (!packTypes.empty()) {
    auto paramUnpackOp = AC->create<EdtParamUnpackOp>(loc, packTypes, paramv);
    auto results = paramUnpackOp.getResults();
    unpackedParams.append(results.begin(), results.begin() + numUserParams);
  }

  /// Unpack deps from depv array into the types expected by the EDT block
  SmallVector<Value> unpackedDeps;
  Region &edtRegion = edtOp.getRegion();
  Block &edtBlock = edtRegion.front();
  if (!edtBlock.getArguments().empty()) {
    SmallVector<Type> depStructTypes;
    depStructTypes.reserve(edtBlock.getNumArguments());
    for (size_t i = 0; i < edtBlock.getNumArguments(); ++i)
      depStructTypes.push_back(AC->ArtsEdtDep);

    auto depUnpackOp = AC->create<EdtDepUnpackOp>(loc, depStructTypes, depv);
    llvm::append_range(unpackedDeps, depUnpackOp.getResults());
  }

  /// Clone constants into function
  IRMapping valueMapping;

  /// Handle block arguments - create proper memref values from dependency
  /// handles
  for (auto [edtArg, unpackedDep] :
       llvm::zip(edtBlock.getArguments(), unpackedDeps)) {
    /// Create memref from dependency handle using DepGetPtrOp pattern
    Value memrefValue = createFinalPtr(loc, unpackedDep, {}, edtArg.getType());
    valueMapping.map(edtArg, memrefValue);
    ARTS_INFO("Mapping EDT arg " << edtArg.getType() << " to memref value "
                                 << memrefValue.getType());
  }

  for (Value constant : envManager.getConstants())
    if (Operation *defOp = constant.getDefiningOp())
      valueMapping.map(constant, builder.clone(*defOp)->getResult(0));

  /// Map parameters directly to their unpacked counterparts; clone constants
  /// and undef.
  size_t unpackedIndex = 0;
  for (Value param : parameters) {
    /// Handle llvm.mlir.undef by recreating it instead of unpacking
    if (auto defOp = param.getDefiningOp()) {
      if (defOp->getName().getStringRef() == "llvm.mlir.undef") {
        valueMapping.map(param, builder.clone(*defOp)->getResult(0));
        continue;
      }
    }
    /// Map to unpacked parameter
    if (unpackedIndex < unpackedParams.size()) {
      valueMapping.map(param, unpackedParams[unpackedIndex++]);
    }
  }

  for (Value freeVar : envManager.getCapturedValues())
    if (Operation *defOp = freeVar.getDefiningOp())
      if (defOp->hasTrait<OpTrait::ConstantLike>())
        valueMapping.map(freeVar, builder.clone(*defOp)->getResult(0));

  /// Clone region operations
  builder.setInsertionPointToEnd(entryBlock);
  for (Operation &op :
       llvm::make_early_inc_range(edtBlock.without_terminator())) {
    Operation *clonedOp = builder.clone(op, valueMapping);
    for (auto [orig, clone] :
         llvm::zip(op.getResults(), clonedOp->getResults()))
      valueMapping.map(orig, clone);
  }

  /// Dependencies are now properly mapped via valueMapping above
  /// transformDependencyUses is not needed since value mapping handles
  /// everything

  /// Add return terminator
  AC->create<func::ReturnOp>(loc);

  return success();
}

LogicalResult
EdtLoweringPass::insertDependencyManagement(Location loc, Value edtGuid,
                                            const SmallVector<Value> &deps) {
  if (deps.empty())
    return success();

  /// Separate deps by mode
  SmallVector<Value> inDeps, outDeps;

  for (Value dep : deps) {
    auto dbAcquireOp = dep.getDefiningOp<DbAcquireOp>();
    assert(dbAcquireOp && "Dependencies must be DbAcquireOp operations");

    ArtsMode mode = dbAcquireOp.getMode();
    if (mode == ArtsMode::in || mode == ArtsMode::inout)
      inDeps.push_back(dep);
    if (mode == ArtsMode::out || mode == ArtsMode::inout)
      outDeps.push_back(dep);
  }

  /// Record all input deps at once
  if (!inDeps.empty())
    AC->create<RecordInDepOp>(loc, edtGuid, inDeps);

  /// Increment output latches for all deps at once
  if (!outDeps.empty())
    AC->create<IncrementOutLatchOp>(loc, edtGuid, outDeps);

  return success();
}

std::string EdtLoweringPass::generateUniqueFunctionName() {
  return "__arts_edt_" + std::to_string(++functionCounter);
}

bool EdtLoweringPass::canLowerEdt(EdtOp edtOp) {
  /// Only lower task EDTs with non-empty single-block regions
  return edtOp.getType() == EdtType::task && !edtOp.getRegion().empty() &&
         edtOp.getRegion().hasOneBlock();
}

/// Helper to create final ptr from dependency struct and indices
Value EdtLoweringPass::createFinalPtr(Location loc, Value depStruct,
                                      ValueRange indices, Type resultType) {
  if (indices.empty())
    return AC->create<DepGetPtrOp>(loc, resultType, depStruct);

  auto subIndexOp =
      AC->create<DbSubIndexOp>(loc, depStruct.getType(), depStruct, indices);
  return AC->create<DepGetPtrOp>(loc, resultType, subIndexOp.getResult());
}

void EdtLoweringPass::transformDependencyUses(Block *block,
                                              ArrayRef<Value> unpackedDeps) {
  SmallVector<Operation *> toReplace;

  ARTS_INFO("transformDependencyUses: Processing " << unpackedDeps.size()
                                                   << " unpacked deps");
  for (size_t i = 0; i < unpackedDeps.size(); ++i) {
    Value dep = unpackedDeps[i];
    ARTS_INFO("  [" << i << "] Type: " << dep.getType() << ", DefOp: "
                    << (dep.getDefiningOp()
                            ? dep.getDefiningOp()->getName().getStringRef()
                            : "none"));
  }

  /// Find all load/store operations that use unpacked deps
  block->walk([&](Operation *op) {
    ARTS_INFO("  Examining op: " << op->getName().getStringRef());

    Value memref;
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      ARTS_INFO("    LoadOp found, getting memref...");
      memref = loadOp.getMemref();
      ARTS_INFO("    LoadOp memref type: " << memref.getType());
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      ARTS_INFO("    StoreOp found, getting memref...");
      memref = storeOp.getMemref();
      ARTS_INFO("    StoreOp memref type: " << memref.getType());
    } else {
      return;
    }

    if (llvm::find(unpackedDeps, memref) != unpackedDeps.end()) {
      ARTS_INFO("    Operation uses unpacked dep, marking for replacement");
      toReplace.push_back(op);
    }
  });

  /// Transform each operation to use DbSubIndexOp + DepGetPtrOp pattern
  for (Operation *op : toReplace) {
    AC->setInsertionPoint(op);
    Location loc = op->getLoc();

    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      Value finalPtr =
          createFinalPtr(loc, loadOp.getMemref(), loadOp.getIndices(),
                         loadOp.getResult().getType());
      auto newLoad = AC->create<memref::LoadOp>(loc, finalPtr, ValueRange{});
      loadOp.replaceAllUsesWith(newLoad.getResult());

    } else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      Value finalPtr =
          createFinalPtr(loc, storeOp.getMemref(), storeOp.getIndices(),
                         storeOp.getMemref().getType());
      AC->create<memref::StoreOp>(loc, storeOp.getValueToStore(), finalPtr,
                                  ValueRange{});
    }

    op->erase();
  }
}

//===----------------------------------------------------------------------===//
// Pass Registration
//===----------------------------------------------------------------------===//

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createEdtLoweringPass() {
  return std::make_unique<EdtLoweringPass>();
}

} // namespace arts
} // namespace mlir