///==========================================================================
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
// 6. Add dependency management (record_in_dep, increment_out_latch)
///==========================================================================

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

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
#include "polygeist/Ops.h"
ARTS_DEBUG_SETUP(edt_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

//===----------------------------------------------------------------------===//
// EdtEnvManager
// Manages the environment analysis for EDT regions by collecting parameters,
// constants, and dependencies used in the region.
//===----------------------------------------------------------------------===//
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

//===----------------------------------------------------------------------===//
// EDT Lowering Pass Implementation
//===----------------------------------------------------------------------===//
struct EdtLoweringPass : public arts::EdtLoweringBase<EdtLoweringPass> {
  ~EdtLoweringPass() { delete AC; }

  void runOnOperation() override;

private:
  /// Core transformation methods
  LogicalResult lowerEdt(EdtOp edtOp);
  LogicalResult lowerForOp(ForOp forOp);

  /// Function outlining with ARTS signature
  func::FuncOp createOutlinedFunction(EdtOp edtOp, EdtEnvManager &envManager);

  /// Parameter handling
  Value packParameters(Location loc, EdtEnvManager &envManager,
                       SmallVector<Type> &packTypes);

  /// Region outlining
  LogicalResult outlineRegionToFunction(EdtOp edtOp, func::FuncOp targetFunc,
                                        EdtEnvManager &envManager,
                                        const SmallVector<Type> &packTypes,
                                        size_t numUserParams);

  void transformDepUses(Block *block, ArrayRef<Value> originalDeps, Value depv,
                        ArrayRef<Value> allParams, EdtEnvManager &envManager,
                        ArrayRef<Value> depIdentifiers);

  /// Dependency satisfaction
  LogicalResult insertDepManagement(Location loc, Value edtGuid,
                                    const SmallVector<Value> &deps);

  /// Utilities
  std::string generateUniqueFunctionName();
  bool canLowerEdt(EdtOp edtOp);

  /// Parallel EDT lowering
  bool lowerParallel(EdtOp &op);

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

  /// First, collect and lower all parallel EDTs
  SmallVector<EdtOp, 8> parallelEdts;
  module.walk<WalkOrder::PostOrder>([&](EdtOp edtOp) {
    if (edtOp.getType() == EdtType::parallel)
      parallelEdts.push_back(edtOp);
  });
  ARTS_INFO("Found " << parallelEdts.size() << " parallel EDTs to lower");
  for (EdtOp edtOp : parallelEdts) {
    if (!lowerParallel(edtOp)) {
      edtOp.emitError("Failed to lower parallel EDT");
      return signalPassFailure();
    }
  }

  /// Now collect and lower all regular EDTs
  SmallVector<EdtOp, 8> taskEdts;
  module.walk<WalkOrder::PostOrder>([&](EdtOp edtOp) {
    if (canLowerEdt(edtOp))
      taskEdts.push_back(edtOp);
  });
  ARTS_INFO("Found " << taskEdts.size() << " task EDTs to lower");
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

  /// Create edt outlined function
  func::FuncOp outlinedFunc = createOutlinedFunction(edtOp, envManager);
  if (!outlinedFunc)
    return edtOp.emitError("Failed to create outlined function");

  /// Pack parameters
  Value paramPack = packParameters(loc, envManager, packTypes);

  /// Outline region to function and replace EDT
  if (failed(outlineRegionToFunction(edtOp, outlinedFunc, envManager, packTypes,
                                     envManager.getParameters().size()))) {
    return edtOp.emitError("Failed to outline region to function");
  }

  /// Calculate total dependency count (sum of elements in all deps)
  Value depCount = AC->createIntConstant(0, AC->Int32, loc);
  for (Value dep : envManager.getDependencies()) {
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

  /// Create the outline operation at the same location as the EDT
  auto outlineOp = AC->create<EdtCreateOp>(loc, paramPack, depCount, routeVal);

  outlineOp->setAttr("outlined_func",
                     AC->getBuilder().getStringAttr(outlinedFunc.getName()));

  /// Insert dependency management after the outline op
  Value edtGuid = outlineOp.getGuid();
  AC->setInsertionPointAfter(outlineOp);
  SmallVector<Value> depsVec(envManager.getDependencies().begin(),
                             envManager.getDependencies().end());
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

Value EdtLoweringPass::packParameters(Location loc, EdtEnvManager &envManager,
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
  SmallVector<Value> unpackedParams, allParams;

  if (!packTypes.empty()) {
    auto paramUnpackOp = AC->create<EdtParamUnpackOp>(loc, packTypes, paramv);
    auto results = paramUnpackOp.getResults();
    allParams.assign(results.begin(), results.end());
    unpackedParams.append(allParams.begin(), allParams.begin() + numUserParams);
  }

  /// Store dependency information for direct use
  SmallVector<Value> originalDeps(envManager.getDependencies().begin(),
                                  envManager.getDependencies().end());
  Region &edtRegion = edtOp.getRegion();
  Block &edtBlock = edtRegion.front();

  /// Create compact per-dependency placeholders for later dep_gep rewrite.
  SmallVector<Value> depPlaceholders(originalDeps.size());
  for (auto it : llvm::enumerate(originalDeps))
    depPlaceholders[it.index()] =
        AC->create<UndefOp>(loc, it.value().getType());

  /// Clone constants into function
  IRMapping valueMapping;

  /// Map EDT args to placeholders so cloned ops don't reference outer values.
  for (auto [edtArg, ph] : llvm::zip(edtBlock.getArguments(), depPlaceholders))
    valueMapping.map(edtArg, ph);

  /// Also map the original dependency values to their corresponding
  /// placeholders to catch any direct uses that bypassed the block arguments.
  for (auto [originalDep, placeholder] :
       llvm::zip(originalDeps, depPlaceholders))
    valueMapping.map(originalDep, placeholder);

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
    if (unpackedIndex < unpackedParams.size())
      valueMapping.map(param, unpackedParams[unpackedIndex++]);
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

  /// Add return terminator
  AC->create<func::ReturnOp>(loc);

  /// Transform dependency uses inside outlined region
  transformDepUses(entryBlock, originalDeps, depv, allParams, envManager,
                   depPlaceholders);
  return success();
}

LogicalResult
EdtLoweringPass::insertDepManagement(Location loc, Value edtGuid,
                                     const SmallVector<Value> &deps) {
  if (deps.empty())
    return success();

  /// Separate deps by mode and extract GUIDs
  SmallVector<Value> inDepGuids, outDepGuids;

  for (Value dep : deps) {
    auto dbAcquireOp = dep.getDefiningOp<DbAcquireOp>();
    assert(dbAcquireOp && "Dependencies must be DbAcquireOp operations");

    /// Get the GUID from the DbAcquireOp
    Value depGuid = dbAcquireOp.getGuid();

    /// Always add to in-dependencies
    inDepGuids.push_back(depGuid);
  
    /// Only add to out-dependencies if mode is out or inout
    ArtsMode mode = dbAcquireOp.getMode();
    if (mode == ArtsMode::out || mode == ArtsMode::inout)
      outDepGuids.push_back(depGuid);
  }

  /// Record all input deps at once using GUIDs
  if (!inDepGuids.empty())
    AC->create<RecordInDepOp>(loc, edtGuid, inDepGuids);

  /// Increment output latches for all deps at once using GUIDs
  if (!outDepGuids.empty())
    AC->create<IncrementOutLatchOp>(loc, edtGuid, outDepGuids);

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

/// Lowers a parallel EDT into an arts::EpochOp wrapping an scf.for loop over
/// workers. Responsibility: structural lowering; optional propagation of inner
/// dependencies as a conservative union.
bool EdtLoweringPass::lowerParallel(EdtOp &op) {
  ARTS_INFO("Lowering parallel EDT");
  auto loc = op.getLoc();
  OpBuilder builder(op);

  /// Create an arts::EpochOp to scope the parallel work.
  auto epochOp = builder.create<arts::EpochOp>(loc);
  auto &region = epochOp.getRegion();
  if (region.empty())
    region.push_back(new Block());
  Block *newBlock = &region.front();

  /// Set the insertion point to the end of the new block.
  builder.setInsertionPointToEnd(newBlock);

  /// Generate the SCF for-loop.
  /// Build loop bounds: [0, numWorkers) with step = 1.
  Value lowerBound = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value workers = builder.create<arts::GetTotalWorkersOp>(loc);
  Value upperBound =
      builder.create<arith::IndexCastOp>(loc, builder.getIndexType(), workers);
  Value step = builder.create<arith::ConstantIndexOp>(loc, 1);
  auto forOp = builder.create<scf::ForOp>(loc, lowerBound, upperBound, step);

  /// Create terminator for the epochOp.
  builder.create<arts::YieldOp>(loc);

  /// Collect all dependencies before creating new EDT
  SetVector<Value> allDeps;
  op.walk([&](EdtOp inner) {
    if (inner == op)
      return WalkResult::advance();
    for (Value dep : inner.getDependencies())
      allDeps.insert(dep);
    return WalkResult::advance();
  });

  /// Create new EDT with merged dependencies and move the region
  builder.setInsertionPointToStart(forOp.getBody());
  auto newEdtOp = builder.create<arts::EdtOp>(loc, arts::EdtType::task,
                                              allDeps.getArrayRef());

  /// Move the region from the original EDT to the new EDT
  Region &sourceRegion = op.getRegion();
  Region &newRegion = newEdtOp.getRegion();
  if (newRegion.empty())
    newRegion.push_back(new Block());
  Block &newBody = newRegion.front();
  newBody.clear();

  /// Move operations from source region to new region
  SmallVector<Operation *, 16> opsToMove;
  for (auto &block : sourceRegion) {
    for (auto &opInBlock : block)
      opsToMove.push_back(&opInBlock);
  }

  /// Move operations to new region
  for (auto *opToMove : opsToMove)
    opToMove->moveBefore(&newBody, newBody.end());

  /// Replace original EDT with new one
  op->replaceAllUsesWith(newEdtOp);

  /// Remove the original EDT
  op.erase();

  return true;
}

void EdtLoweringPass::transformDepUses(Block *block,
                                       ArrayRef<Value> originalDeps, Value depv,
                                       ArrayRef<Value> allParams,
                                       EdtEnvManager &envManager,
                                       ArrayRef<Value> depIdentifiers) {

  const auto &paramMap = envManager.getValueToPackIndex();
  auto resolveSizes = [&](Value dep, Location loc) {
    SmallVector<Value> sizes = getSizesFromDb(dep);
    SmallVector<Value> resolved;
    for (Value sz : sizes) {
      auto it = paramMap.find(sz);
      if (it != paramMap.end() && it->second < allParams.size())
        resolved.push_back(allParams[it->second]);
      else if (auto c = sz.getDefiningOp<arith::ConstantIndexOp>())
        resolved.push_back(AC->createIndexConstant(c.value(), loc));
      else
        resolved.push_back(sz);
    }
    if (resolved.empty())
      resolved.push_back(AC->createIndexConstant(1, loc));
    return resolved;
  };

  auto computeBaseOffset = [&](size_t depIndex, Location loc) {
    Value base = AC->createIndexConstant(0, loc);
    for (size_t i = 0; i < depIndex; ++i) {
      SmallVector<Value> prevResolved = resolveSizes(originalDeps[i], loc);
      Value prevElems = AC->computeTotalElements(prevResolved, loc);
      base = AC->create<arith::AddIOp>(loc, base, prevElems);
    }
    return base;
  };

  /// For each dependency placeholder, rewrite its direct uses.
  for (size_t depIndex = 0; depIndex < depIdentifiers.size(); ++depIndex) {
    Value placeholder = depIdentifiers[depIndex];
    SmallVector<Operation *, 16> users;
    for (auto &use : placeholder.getUses())
      users.push_back(use.getOwner());

    for (Operation *op : users) {
      if (auto mp = dyn_cast<polygeist::Memref2PointerOp>(op)) {
        if (mp.getSource() != placeholder)
          continue;
        AC->setInsertionPoint(op);
        Location loc = op->getLoc();
        Value baseOffset = computeBaseOffset(depIndex, loc);
        SmallVector<Value> sizes = resolveSizes(originalDeps[depIndex], loc);
        SmallVector<Value> strides = AC->computeStridesFromSizes(sizes, loc);
        SmallVector<Value> zeroIndices(sizes.size(),
                                       AC->createIndexConstant(0, loc));
        Value newPtr = AC->create<DepGepOp>(loc, AC->llvmPtr, depv, baseOffset,
                                            zeroIndices, strides);
        op->getResult(0).replaceAllUsesWith(newPtr);
        op->erase();
      } else if (auto dbGep = dyn_cast<arts::DbGepOp>(op)) {
        if (dbGep.getBasePtr() != placeholder)
          continue;
        AC->setInsertionPoint(op);
        Location loc = op->getLoc();
        Value baseOffset = computeBaseOffset(depIndex, loc);
        SmallVector<Value> sizes = resolveSizes(originalDeps[depIndex], loc);
        SmallVector<Value> strides = AC->computeStridesFromSizes(sizes, loc);
        SmallVector<Value> indices = dbGep.getIndices();
        Value newDepGep = AC->create<DepGepOp>(loc, AC->llvmPtr, depv,
                                               baseOffset, indices, strides);
        op->getResult(0).replaceAllUsesWith(newDepGep);
        op->erase();
      }
    }
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