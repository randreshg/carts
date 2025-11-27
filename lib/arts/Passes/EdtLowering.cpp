///==========================================================================///
/// File: EdtLowering.cpp
/// Complete implementation of EDT lowering pass that transforms arts.edt
/// operations into runtime-compatible function calls.
///
/// This pass implements a 6-step EDT lowering process:
/// 1. Analyze EDT region for free variables and deps
/// 2. Outline EDT region to function with ARTS runtime signature
/// 3. Insert parameter packing before EDT (edt_param_pack) - It should include
///    all parameters from the EDT + unique datablock sizes and indices for all
///    deps
/// 4. Insert parameter/dependency unpacking in outlined function
/// 5. Replace EDT with edt_create call returning GUID
/// 6. Add dependency management (record_in_dep, increment_out_latch)
///==========================================================================///

#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/IdRegistry.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/RegionUtils.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
#include "polygeist/Ops.h"
ARTS_DEBUG_SETUP(edt_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

///===----------------------------------------------------------------------===///
// EdtEnvManager
// Manages the environment analysis for EDT regions by collecting parameters,
// constants, and dependencies used in the region.
///===----------------------------------------------------------------------===///
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
  const DenseMap<Value, unsigned> &getValueToPackIndex() const {
    return valueToPackIndex;
  }
  DenseMap<Value, unsigned> &getValueToPackIndex() { return valueToPackIndex; }

private:
  EdtOp edtOp;
  SetVector<Value> capturedValues, parameters, constants, deps;
  DenseMap<Value, unsigned> valueToPackIndex;
};

///===----------------------------------------------------------------------===///
// EDT Lowering Pass Implementation
///===----------------------------------------------------------------------===///
struct EdtLoweringPass : public arts::EdtLoweringBase<EdtLoweringPass> {
  explicit EdtLoweringPass(uint64_t idStride = IdRegistry::DefaultStride)
      : idStride(idStride) {}
  ~EdtLoweringPass() { delete AC; }
  void runOnOperation() override;

private:
  /// Core transformation methods
  LogicalResult lowerEdt(EdtOp edtOp);

  /// Function outlining with ARTS signature
  func::FuncOp createOutlinedFunction(EdtOp edtOp, EdtEnvManager &envManager);

  /// Parameter handling
  Value packParams(Location loc, EdtEnvManager &envManager,
                   SmallVector<Type> &packTypes);

  /// Region outlining
  LogicalResult outlineRegionToFunction(EdtOp edtOp, func::FuncOp targetFunc,
                                        EdtEnvManager &envManager,
                                        const SmallVector<Type> &packTypes,
                                        size_t numUserParams);

  void transformDepUses(ArrayRef<Value> originalDeps, Value depv,
                        ArrayRef<Value> allParams, EdtEnvManager &envManager,
                        ArrayRef<Value> depIdentifiers);

  /// Clone EDT body operations with nested region remapping
  void cloneAndRemapEdtBody(Block &sourceBlock, OpBuilder &builder,
                            IRMapping &valueMapping);

  /// Dependency satisfaction
  LogicalResult insertDepManagement(Location loc, Value edtGuid,
                                    const SmallVector<Value> &deps);

  /// Attributes
  uint64_t idStride = IdRegistry::DefaultStride;
  unsigned functionCounter = 0;
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
  IdRegistry idRegistry;
};

} // namespace

///===----------------------------------------------------------------------===///
// Pass Implementation
///===----------------------------------------------------------------------===///

void EdtLoweringPass::runOnOperation() {
  module = getOperation();
  AC = new ArtsCodegen(module, false);

  ARTS_INFO_HEADER(EdtLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Collect and lower all task EDTs
  {
    ARTS_DEBUG_HEADER(TaskEdtLowering);
    SmallVector<EdtOp, 8> taskEdts;
    module.walk<WalkOrder::PostOrder>([&](EdtOp edtOp) {
      /// Demote any remaining non-task EDTs (e.g., single) so we can lower
      /// them uniformly. Parallel EDTs should have been handled by
      /// ParallelEdtLowering already.
      if (edtOp.getType() != EdtType::task) {
        ARTS_DEBUG("Demoting non-task EDT to task: " << edtOp);
        edtOp.setType(EdtType::task);
        edtOp->removeAttr(AttrNames::Operation::Workers);
        edtOp->removeAttr(AttrNames::Operation::Nowait);
      }
      taskEdts.push_back(edtOp);
    });
    ARTS_INFO("Found " << taskEdts.size() << " task EDTs to lower");
    for (EdtOp edtOp : taskEdts) {
      if (failed(lowerEdt(edtOp))) {
        edtOp.emitError("Failed to lower task EDT");
        return signalPassFailure();
      }
    }
  }

  ARTS_INFO_FOOTER(EdtLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Lower EDT operations to runtime calls
///
/// Transforms arts.edt operations into outlined functions and runtime calls.
/// Creates an outlined function with the EDT body, packs parameters and
/// dependencies, and replaces the EDT with arts.edt_create and dependency
/// management calls. Example:
///   %result = arts.edt(%dep) {
///   ^bb0(%arg: memref<f64>):
///     %val = memref.load %arg[] : memref<f64>
///     arts.return %val : f64
///   }
/// becomes:
///   %param_pack = arts.edt_param_pack ...
///   %edt_guid = arts.edt_create %param_pack, %dep_count, %route
///   arts.record_in_dep %edt_guid, %dep
///   arts.increment_out_latch %edt_guid
///===----------------------------------------------------------------------===///
LogicalResult EdtLoweringPass::lowerEdt(EdtOp edtOp) {
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(edtOp);
  Location loc = edtOp.getLoc();
  /// Analyze EDT environment (parameters, constants, deps)
  EdtEnvManager envManager(edtOp);

  /// Create local variables for parameter packing/dedup
  DenseMap<Value, unsigned> valueToPackIndex;
  SmallVector<Type> packTypes;

  /// Create edt outlined function
  func::FuncOp outlinedFunc = createOutlinedFunction(edtOp, envManager);
  if (!outlinedFunc)
    return edtOp.emitError("Failed to create outlined function");

  /// Pack parameters
  Value paramPack = packParams(loc, envManager, packTypes);

  /// Outline region to function and replace EDT
  if (failed(outlineRegionToFunction(edtOp, outlinedFunc, envManager, packTypes,
                                     envManager.getParameters().size()))) {
    return edtOp.emitError("Failed to outline region to function");
  }

  ArrayRef<Value> edtDeps = envManager.getDependencies();

  /// Calculate total dependency count as the sum of elements of all deps
  Value depCount = AC->createIntConstant(0, AC->Int32, loc);
  for (Value dep : edtDeps) {
    SmallVector<Value> sizes = getSizesFromDb(dep);
    Value numElements = AC->create<DbNumElementsOp>(loc, sizes);
    numElements = AC->castToInt(AC->Int32, numElements, loc);
    depCount = AC->create<arith::AddIOp>(loc, depCount, numElements);
  }

  /// Create the outline operation at the same location as the EDT
  AC->setInsertionPoint(edtOp);
  Value routeVal = edtOp.getRoute();
  if (!routeVal)
    routeVal = AC->createIntConstant(0, AC->Int32, loc);

  /// If EdtOp has an explicit epoch GUID, pass it to EdtCreateOp
  EdtCreateOp outlineOp;
  if (Value epochGuid = edtOp.getEpochGuid()) {
    ARTS_DEBUG("Creating EdtCreateOp with epoch GUID: " << epochGuid);
    outlineOp =
        AC->create<EdtCreateOp>(loc, paramPack, depCount, routeVal, epochGuid);
  } else {
    ARTS_DEBUG("Creating EdtCreateOp without epoch GUID");
    outlineOp = AC->create<EdtCreateOp>(loc, paramPack, depCount, routeVal);
  }

  outlineOp->setAttr(AttrNames::Operation::OutlinedFunc,
                     AC->getBuilder().getStringAttr(outlinedFunc.getName()));
  int64_t baseId = 0;
  if (auto edtId = edtOp->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsId))
    baseId = edtId.getInt();
  else
    baseId = idRegistry.getOrCreate(edtOp.getOperation());
  /// Set the create id for the outlined operation
  if (baseId) {
    int64_t createId = baseId * static_cast<int64_t>(idStride);
    outlineOp->setAttr(AttrNames::Operation::ArtsCreateId,
                       AC->getBuilder().getI64IntegerAttr(createId));
    ARTS_DEBUG("  - EDT arts.create_id=" << createId << " (base=" << baseId
                                         << " x stride=" << idStride << ")");
  }

  /// Insert dependency management after the outline op
  Value edtGuid = outlineOp.getGuid();
  AC->setInsertionPointAfter(outlineOp);
  SmallVector<Value> depsVec(edtDeps.begin(), edtDeps.end());
  if (failed(insertDepManagement(loc, edtGuid, depsVec)))
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

///===----------------------------------------------------------------------===///
/// Create outlined function for EDT body
///
/// Generates a new private function with the ARTS EDT signature to contain
/// the outlined EDT region.
///===----------------------------------------------------------------------===///
func::FuncOp
EdtLoweringPass::createOutlinedFunction(EdtOp edtOp,
                                        EdtEnvManager &envManager) {
  Location loc = edtOp.getLoc();
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(module);
  std::string funcName = "__arts_edt_" + std::to_string(++functionCounter);
  auto outlinedFunc = AC->create<func::FuncOp>(loc, funcName, AC->EdtFn);
  outlinedFunc.setPrivate();

  ARTS_INFO("Created outlined function: " << funcName);
  return outlinedFunc;
}

///===----------------------------------------------------------------------===///
/// Pack EDT parameters and dependency metadata
///
/// Creates a parameter pack containing user-defined parameters, constants,
/// and metadata for datablock dependencies (indices, offsets, sizes).
///===----------------------------------------------------------------------===///
Value EdtLoweringPass::packParams(Location loc, EdtEnvManager &envManager,
                                  SmallVector<Type> &packTypes) {
  const auto &parameters = envManager.getParameters();
  const auto &deps = envManager.getDependencies();

  SmallVector<Value> packValues;
  auto &valueToPackIndex = envManager.getValueToPackIndex();

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
    for (size_t i = 0; i < packValues.size(); ++i)
      ARTS_DEBUG("  packValues[" << i << "]: " << packValues[i]);
  });

  auto memrefType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
  auto packOp = AC->create<EdtParamPackOp>(loc, TypeRange{memrefType},
                                           ValueRange(packValues));
  return packOp;
}

///===----------------------------------------------------------------------===///
/// Outline EDT region to target function
///
/// Moves the EDT body region into the outlined function, performs parameter
/// and dependency unpacking, and updates value references to work with the
/// new function context.
///===----------------------------------------------------------------------===///
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
  SmallVector<Value> unpackedParams, allParams;

  if (!packTypes.empty()) {
    auto paramUnpackOp = AC->create<EdtParamUnpackOp>(loc, packTypes, paramv);
    auto results = paramUnpackOp.getResults();
    allParams.assign(results.begin(), results.end());
    unpackedParams.append(allParams.begin(), allParams.begin() + numUserParams);
  }

  /// Create per-dependency placeholders for later dep_gep rewrite
  Region &edtRegion = edtOp.getRegion();
  Block &edtBlock = edtRegion.front();
  ArrayRef<Value> originalDeps = envManager.getDependencies();

  SmallVector<Value> depPlaceholders(originalDeps.size());
  for (auto it : llvm::enumerate(originalDeps))
    depPlaceholders[it.index()] =
        AC->create<UndefOp>(loc, edtBlock.getArguments()[it.index()].getType());

  /// Build value mapping for cloning EDT body into outlined function
  IRMapping valueMapping;

  /// Map EDT args to placeholders so cloned ops don't reference outer values.
  for (auto [edtArg, placeholder] :
       llvm::zip(edtBlock.getArguments(), depPlaceholders))
    valueMapping.map(edtArg, placeholder);

  /// Also map original dependency values to placeholders to catch direct uses
  for (auto [originalDep, placeholder] :
       llvm::zip(originalDeps, depPlaceholders))
    valueMapping.map(originalDep, placeholder);

  /// Clone constants and constant-like operations into the outlined function
  auto cloneConstantLike = [&](Value val) {
    if (Operation *defOp = val.getDefiningOp())
      if (defOp->hasTrait<OpTrait::ConstantLike>() ||
          defOp->getName().getStringRef() == "llvm.mlir.undef")
        valueMapping.map(val, builder.clone(*defOp)->getResult(0));
  };

  for (Value constant : envManager.getConstants())
    cloneConstantLike(constant);

  for (Value freeVar : envManager.getCapturedValues())
    cloneConstantLike(freeVar);

  /// Map parameters to their unpacked counterparts (skip undef - already
  /// cloned)
  size_t unpackedIndex = 0;
  for (Value param : parameters) {
    if (auto defOp = param.getDefiningOp())
      if (defOp->getName().getStringRef() == "llvm.mlir.undef") {
        cloneConstantLike(param);
        continue;
      }
    if (unpackedIndex < unpackedParams.size())
      valueMapping.map(param, unpackedParams[unpackedIndex++]);
  }

  /// Clone remaining captured values that are pure and regionless (e.g.,
  /// llvm.getelementptr chains computing format strings) so the outlined
  /// function does not reference parent-scope SSA values.
  DenseSet<Operation *> visited;
  std::function<LogicalResult(Value)> cloneCaptured = [&](Value val) {
    if (valueMapping.contains(val))
      return success();

    Operation *defOp = val.getDefiningOp();
    if (!defOp)
      return success();

    if (visited.contains(defOp))
      return success();
    visited.insert(defOp);

    for (Value operand : defOp->getOperands())
      if (failed(cloneCaptured(operand)))
        return failure();

    Operation *cloned = builder.clone(*defOp, valueMapping);
    for (auto [oldRes, newRes] :
         llvm::zip(defOp->getResults(), cloned->getResults()))
      valueMapping.map(oldRes, newRes);
    return success();
  };

  for (Value captured : envManager.getCapturedValues())
    if (failed(cloneCaptured(captured)))
      return failure();

  /// Clone EDT body operations into outlined function
  builder.setInsertionPointToEnd(entryBlock);
  cloneAndRemapEdtBody(edtBlock, builder, valueMapping);

  /// Add return terminator
  AC->create<func::ReturnOp>(loc);

  /// Transform dependency uses inside outlined region
  transformDepUses(originalDeps, depv, allParams, envManager, depPlaceholders);
  return success();
}

///===----------------------------------------------------------------------===///
/// Insert dependency management operations
///
/// Adds runtime dependency tracking operations (record_in_dep,
/// increment_out_latch) for EDT execution. Determines access mode based on
/// dependency source and extracts GUIDs appropriately. Example:
///   EDT with input deps %d1, %d2 and output deps %d3
///   becomes: arts.rec_dep %edt_guid, [%d1_guid, %d2_guid, %d3_guid]
///   {access_mode = direct}
///            arts.inc_dep %edt_guid, [%d3_guid] {access_mode = direct}
///===----------------------------------------------------------------------===///
LogicalResult
EdtLoweringPass::insertDepManagement(Location loc, Value edtGuid,
                                     const SmallVector<Value> &deps) {
  if (deps.empty())
    return success();

  /// Determine access mode based on dependency sources
  /// If any dependency comes from DepDbAcquireOp, use from_depv mode
  DepAccessMode accessMode = DepAccessMode::direct;
  for (Value dep : deps) {
    if (dep.getDefiningOp<DepDbAcquireOp>()) {
      accessMode = DepAccessMode::from_depv;
      break;
    }
  }

  /// Extract GUIDs and acquire modes from dependencies
  SmallVector<Value> depGuids;
  SmallVector<int32_t> acquireModes;
  SmallVector<bool> twinDiffHints;
  for (Value dep : deps) {
    /// Handle both DbAcquireOp and DepDbAcquireOp as dependency sources, even
    /// when they are threaded through block arguments.
    Operation *underlyingDb = arts::getUnderlyingDb(dep);
    auto dbAcquireOp = dyn_cast_or_null<DbAcquireOp>(underlyingDb);
    auto depDbAcquireOp = dyn_cast_or_null<DepDbAcquireOp>(underlyingDb);
    if (!dbAcquireOp && !depDbAcquireOp) {
      return mlir::emitError(
                 loc,
                 "Dependency must be from DbAcquireOp or DepDbAcquireOp, got: ")
             << dep;
    }

    /// Get the GUID from the acquire operation
    Value depGuid =
        dbAcquireOp ? dbAcquireOp.getGuid() : depDbAcquireOp.getGuid();
    depGuids.push_back(depGuid);

    DbAllocOp allocForHint =
        dyn_cast_or_null<DbAllocOp>(arts::getUnderlyingDbAlloc(dep));
    if (dbAcquireOp && !allocForHint)
      allocForHint = dyn_cast<DbAllocOp>(
          arts::getUnderlyingDbAlloc(dbAcquireOp.getSourcePtr()));

    /// Extract acquire mode: Convert ArtsMode to DbMode enum
    ArtsMode artsMode = ArtsMode::inout;
    if (dbAcquireOp)
      artsMode = dbAcquireOp.getMode();

    /// Map ArtsMode to DbMode
    /// Preserve explicit READ acquisitions (e.g., aggregator reading partials)
    /// even if the underlying allocation is WRITE-only. The allocation mode is
    /// only used to narrow (not widen) the access when the arts mode is not
    /// already read.
    DbMode dbMode = (artsMode == ArtsMode::in) ? DbMode::read : DbMode::write;
    if (allocForHint && dbMode != DbMode::read) {
      DbMode allocMode = allocForHint.getDbMode();
      if (allocMode == DbMode::read || allocMode == DbMode::write)
        dbMode = allocMode;
    }
    acquireModes.push_back(static_cast<int32_t>(dbMode));

    /// Twin-diff hint travels with the underlying DbAllocOp when partitioning
    /// falls back to a coarse grain strategy.
    bool useTwinDiff = false;
    if (allocForHint) {
      if (auto twinAttr =
              allocForHint->getAttrOfType<BoolAttr>(
                  AttrNames::Operation::ArtsTwinDiff))
        useTwinDiff = twinAttr.getValue();
    }
    twinDiffHints.push_back(useTwinDiff);
  }

  /// Create dependency management ops with appropriate access mode
  auto modeAttr =
      DepAccessModeAttr::get(AC->getBuilder().getContext(), accessMode);

  DenseI32ArrayAttr acquireAttr;
  if (!acquireModes.empty())
    acquireAttr =
        DenseI32ArrayAttr::get(AC->getBuilder().getContext(), acquireModes);

  DenseBoolArrayAttr twinDiffAttr;
  if (!twinDiffHints.empty())
    twinDiffAttr =
        DenseBoolArrayAttr::get(AC->getBuilder().getContext(), twinDiffHints);

  AC->create<RecordDepOp>(loc, edtGuid, depGuids, modeAttr, acquireAttr,
                          twinDiffAttr);

  return success();
}

///===----------------------------------------------------------------------===///
/// Clone EDT body operations with nested region remapping
///
/// Clones all operations from the EDT body into the outlined function while
/// recursively remapping operands in nested regions to use values from the
/// provided IRMapping. This ensures operations with nested regions (like
/// scf.for, arts.epoch) have their operands correctly updated.
///===----------------------------------------------------------------------===///
void EdtLoweringPass::cloneAndRemapEdtBody(Block &sourceBlock,
                                           OpBuilder &builder,
                                           IRMapping &valueMapping) {
  /// Lambda to recursively remap operands in nested regions
  std::function<void(Operation *, IRMapping &)> remapNestedRegions =
      [&remapNestedRegions](Operation *op, IRMapping &mapping) {
        for (Region &region : op->getRegions()) {
          for (Block &block : region) {
            for (Operation &nestedOp : block) {
              /// Remap each operand of the nested operation
              for (OpOperand &operand : nestedOp.getOpOperands()) {
                if (Value newValue = mapping.lookupOrNull(operand.get()))
                  operand.set(newValue);
              }
              /// Recursively process nested regions
              if (nestedOp.getNumRegions() > 0)
                remapNestedRegions(&nestedOp, mapping);
            }
          }
        }
      };

  /// Clone operations and update mappings
  for (Operation &op :
       llvm::make_early_inc_range(sourceBlock.without_terminator())) {
    Operation *clonedOp = builder.clone(op, valueMapping);
    /// Update mapping with cloned results so later ops reference cloned values
    for (auto [orig, clone] :
         llvm::zip(op.getResults(), clonedOp->getResults()))
      valueMapping.map(orig, clone);
    /// Recursively remap operands in nested regions
    remapNestedRegions(clonedOp, valueMapping);
  }
}

///===----------------------------------------------------------------------===///
/// Transform dependency uses in outlined function
///
/// Rewrites dependency access operations in the outlined EDT function to use
/// the packed dependency data structure. Computes proper base offsets and
/// strides for each dependency, adjusting indices to account for datablock
/// offsets.
///===----------------------------------------------------------------------===///
void EdtLoweringPass::transformDepUses(ArrayRef<Value> originalDeps, Value depv,
                                       ArrayRef<Value> allParams,
                                       EdtEnvManager &envManager,
                                       ArrayRef<Value> depIdentifiers) {
  /// Get the parameter map
  const auto &paramMap = envManager.getValueToPackIndex();

  /// Collect placeholders to erase at the end
  SmallVector<Operation *> placeholdersToErase;

  /// Resolve parameters within the outlined Edt
  auto resolveParam = [&](ArrayRef<Value> params, Location loc) {
    SmallVector<Value> resolved;
    for (Value param : params) {
      auto it = paramMap.find(param);
      if (it != paramMap.end() && it->second < allParams.size())
        resolved.push_back(allParams[it->second]);
      else if (auto c = param.getDefiningOp<arith::ConstantIndexOp>())
        resolved.push_back(AC->createIndexConstant(c.value(), loc));
      else
        resolved.push_back(param);
    }
    if (resolved.empty())
      resolved.push_back(AC->createIndexConstant(1, loc));
    return resolved;
  };

  /// Compute the base offset of the dependency within the outlined Edt
  /// This corresponds to the sum of number of elements in the previous
  /// dependencies
  auto computeBaseOffset = [&](size_t depIndex, Location loc) {
    Value base = AC->createIndexConstant(0, loc);
    for (size_t i = 0; i < depIndex; ++i) {
      SmallVector<Value> prevSizes = getSizesFromDb(originalDeps[i]);
      SmallVector<Value> prevResolved = resolveParam(prevSizes, loc);
      Value prevElems = AC->computeTotalElements(prevResolved, loc);
      base = AC->create<arith::AddIOp>(loc, base, prevElems);
    }
    return base;
  };

  /// For each dependency placeholder, rewrite its direct uses.
  for (size_t depIndex = 0; depIndex < depIdentifiers.size(); ++depIndex) {
    Value placeholder = depIdentifiers[depIndex];
    Location loc = placeholder.getLoc();

    /// Get the base offset, sizes, strides, and offsets of the dependency
    ARTS_DEBUG("Processing Dep[" << depIndex
                                 << "]: " << originalDeps[depIndex]);
    AC->setInsertionPoint(placeholder.getDefiningOp());
    Value baseOffset = computeBaseOffset(depIndex, loc);
    SmallVector<Value> depSizes =
        resolveParam(getSizesFromDb(originalDeps[depIndex]), loc);
    SmallVector<Value> depStrides = AC->computeStridesFromSizes(depSizes, loc);

    /// Replace remaining uses of the dependency placeholder with the dependency
    bool isSingleElement =
        dbHasSingleSize(originalDeps[depIndex].getDefiningOp<DbAcquireOp>());

    /// Get the users of the dependency placeholder
    SmallVector<Operation *, 16> users, dbAcquireUsers;
    for (auto &use : placeholder.getUses()) {
      if (auto dbAcquire = dyn_cast<arts::DbAcquireOp>(use.getOwner()))
        dbAcquireUsers.push_back(use.getOwner());
      else
        users.push_back(use.getOwner());
    }

    /// Rewrite the dbAcquire users
    ARTS_DEBUG(" - Rewriting " << dbAcquireUsers.size() << " dbAcquire users");
    for (Operation *op : dbAcquireUsers) {
      /// Replace with DepDbAcquireOp to access datablock from depv
      auto dbAcquire = dyn_cast<arts::DbAcquireOp>(op);
      assert(dbAcquire && "dbAcquire must be a DbAcquireOp");
      AC->setInsertionPoint(op);
      SmallVector<Value> indices(dbAcquire.getIndices().begin(),
                                 dbAcquire.getIndices().end());
      SmallVector<Value> offsets(dbAcquire.getOffsets().begin(),
                                 dbAcquire.getOffsets().end());
      SmallVector<Value> sizes(dbAcquire.getSizes().begin(),
                               dbAcquire.getSizes().end());
      indices = resolveParam(indices, dbAcquire.getLoc());
      offsets = resolveParam(offsets, dbAcquire.getLoc());
      sizes = resolveParam(sizes, dbAcquire.getLoc());
      auto depDbAcquire = AC->create<arts::DepDbAcquireOp>(
          dbAcquire.getLoc(), dbAcquire.getResult(0).getType(),
          dbAcquire.getResult(1).getType(), depv, baseOffset, indices, offsets,
          sizes);
      /// Replace the uses of the dbAcquire with the depDbAcquire
      dbAcquire.getResult(0).replaceAllUsesWith(depDbAcquire.getResult(0));
      dbAcquire.getResult(1).replaceAllUsesWith(depDbAcquire.getResult(1));
      dbAcquire.erase();
    }

    /// If single element, rewrite the placeholder with a DepGepOp
    if (isSingleElement) {
      ARTS_DEBUG(" - Rewriting single element placeholder");
      Operation *placeholderOp = placeholder.getDefiningOp();
      AC->setInsertionPoint(placeholderOp);
      auto depGep =
          AC->create<DepGepOp>(loc, AC->llvmPtr, AC->llvmPtr, depv, baseOffset,
                               ValueRange(), ValueRange());
      placeholder.replaceAllUsesWith(depGep.getPtr());
      /// Collect placeholder for later erasure
      placeholdersToErase.push_back(placeholderOp);
      continue;
    }

    /// If not, for each user of the dependency placeholder, rewrite the
    /// operation
    ARTS_DEBUG(" - Rewriting " << users.size() << " users");
    for (Operation *op : users) {
      if (auto mp = dyn_cast<polygeist::Memref2PointerOp>(op)) {
        AC->setInsertionPoint(op);
        SmallVector<Value> emptyArgs;
        auto depGep = AC->create<DepGepOp>(loc, AC->llvmPtr, AC->llvmPtr, depv,
                                           baseOffset, emptyArgs, emptyArgs);
        op->getResult(0).replaceAllUsesWith(depGep.getPtr());
        op->erase();
      } else if (auto dbGep = dyn_cast<arts::DbGepOp>(op)) {
        AC->setInsertionPoint(op);
        SmallVector<Value> dbGepIndices(dbGep.getIndices().begin(),
                                        dbGep.getIndices().end());
        auto depGep =
            AC->create<DepGepOp>(loc, AC->llvmPtr, AC->llvmPtr, depv,
                                 baseOffset, dbGepIndices, depStrides);
        op->getResult(0).replaceAllUsesWith(depGep.getPtr());
        op->erase();
      } else if (auto dbRef = dyn_cast<arts::DbRefOp>(op)) {
        AC->setInsertionPoint(op);
        SmallVector<Value> refIndices(dbRef.getIndices().begin(),
                                      dbRef.getIndices().end());
        auto depGep = AC->create<DepGepOp>(loc, AC->llvmPtr, AC->llvmPtr, depv,
                                           baseOffset, refIndices, depStrides);
        auto newMemref = AC->create<polygeist::Pointer2MemrefOp>(
            loc, dbRef.getType(), depGep.getPtr());
        dbRef.getResult().replaceAllUsesWith(newMemref.getResult());
        op->erase();
      } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
        AC->setInsertionPoint(op);
        SmallVector<Value> storeIndices(store.getIndices().begin(),
                                        store.getIndices().end());
        auto depGep =
            AC->create<DepGepOp>(loc, AC->llvmPtr, AC->llvmPtr, depv,
                                 baseOffset, storeIndices, depStrides);
        AC->create<LLVM::StoreOp>(loc, store.getValue(), depGep.getPtr());
        op->erase();
      } else if (auto load = dyn_cast<memref::LoadOp>(op)) {
        AC->setInsertionPoint(op);
        SmallVector<Value> loadIndices(load.getIndices().begin(),
                                       load.getIndices().end());
        auto depGep = AC->create<DepGepOp>(loc, AC->llvmPtr, AC->llvmPtr, depv,
                                           baseOffset, loadIndices, depStrides);
        Value loaded =
            AC->create<LLVM::LoadOp>(loc, load.getType(), depGep.getPtr());
        op->getResult(0).replaceAllUsesWith(loaded);
        op->erase();
      } else if (auto release = dyn_cast<DbReleaseOp>(op)) {
        /// DbReleaseOp using placeholder can be safely erased
        /// Dependency management is now handled through depv
        op->erase();
      }
    }

    /// Collect placeholder for later erasure
    if (auto defOp = placeholder.getDefiningOp())
      placeholdersToErase.push_back(defOp);
  }

  /// Erase all placeholders at the end
  for (Operation *op : placeholdersToErase) {
    /// Replace any remaining placeholder uses with undef before erasing
    if (op->getNumResults() > 0 && !op->getResult(0).use_empty()) {
      OpBuilder::InsertionGuard IG(AC->getBuilder());
      AC->setInsertionPoint(op);
      auto newUndef =
          AC->create<UndefOp>(op->getLoc(), op->getResult(0).getType());
      op->getResult(0).replaceAllUsesWith(newUndef);
    }
    op->erase();
  }
}

///===----------------------------------------------------------------------===///
// Pass Registration
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createEdtLoweringPass() {
  return std::make_unique<EdtLoweringPass>();
}

std::unique_ptr<Pass> createEdtLoweringPass(uint64_t idStride) {
  return std::make_unique<EdtLoweringPass>(idStride);
}

} // namespace arts
} // namespace mlir
