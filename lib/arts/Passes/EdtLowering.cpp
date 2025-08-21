//===----------------------------------------------------------------------===//
// EDT Lowering Pass - Complete Implementation
// Transforms arts.edt operations into runtime-compatible function calls
//
// This pass implements the comprehensive 6-step EDT lowering process:
// 1. Analyze EDT region for free variables and dependencies
// 2. Outline EDT region to function with ARTS runtime signature
// 3. Insert parameter packing before EDT (edt_param_pack)
// 4. Insert parameter/dependency unpacking in outlined function
// 5. Replace EDT with edt_outline_fn call returning GUID
// 6. Add dependency management (record_in_dep, increment_out_latch)
//===----------------------------------------------------------------------===//

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Environment Manager - Inspired by EdtCodegen's EdtEnvManager
//===----------------------------------------------------------------------===//

namespace {

/// Manages the environment analysis for EDT regions
/// Similar to EdtEnvManager in EdtCodegen but adapted for pass context
class EdtEnvironmentManager {
public:
  EdtEnvironmentManager(Region &region) : region(region) {}

  /// Analyze the region and collect parameters/constants
  void analyze() {
    collectFreeVariables();
    classifyVariables();
  }

  const SmallVector<Value> &getParameters() const { return parameters; }
  const SmallVector<Value> &getConstants() const { return constants; }
  const SmallVector<Value> &getFreeVariables() const { return freeVariables; }

private:
  void collectFreeVariables();
  void classifyVariables();

  Region &region;
  SmallVector<Value> freeVariables;
  SmallVector<Value> parameters;
  SmallVector<Value> constants;
};

void EdtEnvironmentManager::collectFreeVariables() {
  DenseSet<Value> definedInRegion;

  // Treat block arguments as defined within the region
  for (Block &block : region) {
    for (BlockArgument arg : block.getArguments()) {
      definedInRegion.insert(arg);
    }
  }

  // Collect all values defined within the region
  region.walk([&](Operation *op) {
    for (Value result : op->getResults()) {
      definedInRegion.insert(result);
    }
  });

  // Find values used but not defined in region (deduplicated)
  DenseSet<Value> seenFree;
  region.walk([&](Operation *op) {
    for (Value operand : op->getOperands()) {
      if (!definedInRegion.contains(operand) && seenFree.insert(operand).second)
        freeVariables.push_back(operand);
    }
  });
}

void EdtEnvironmentManager::classifyVariables() {
  for (Value val : freeVariables) {
    if (auto defOp = val.getDefiningOp()) {
      if (isa<arith::ConstantOp>(defOp) ||
          defOp->hasTrait<OpTrait::ConstantLike>()) {
        constants.push_back(val);
      } else {
        parameters.push_back(val);
      }
    } else {
      parameters.push_back(val);
    }
  }
}

//===----------------------------------------------------------------------===//
// EDT Lowering Pass Implementation
//===----------------------------------------------------------------------===//

struct EdtLoweringPass : public arts::EdtLoweringBase<EdtLoweringPass> {
  void runOnOperation() override;

private:
  /// Core transformation methods
  LogicalResult lowerEdt(EdtOp edtOp);

  /// Function outlining with ARTS signature (inspired by EdtCodegen::createFn)
  func::FuncOp createOutlinedFunction(EdtOp edtOp,
                                      const EdtEnvironmentManager &envMgr);

  /// Parameter handling (inspired by EdtCodegen::setupParameters)
  Value createParameterPack(OpBuilder &builder, Location loc,
                            const SmallVector<Value> &parameters);

  /// Dependency analysis (inspired by EdtCodegen::analyzeDependencies)
  SmallVector<Value> analyzeDependencies(EdtOp edtOp);
  StringRef getDependencyMode(Value dep);

  /// Region outlining (inspired by EdtCodegen::outlineRegion)
  LogicalResult outlineRegionToFunction(EdtOp edtOp, func::FuncOp targetFunc,
                                        const EdtEnvironmentManager &envMgr);

  /// Dependency satisfaction (inspired by EdtCodegen::satisfyDependencies)
  LogicalResult insertDependencyManagement(OpBuilder &builder, Location loc,
                                           Value edtGuid,
                                           const SmallVector<Value> &deps);

  /// Utilities
  std::string generateUniqueFunctionName();
  bool canLowerEdt(EdtOp edtOp);

  /// State
  unsigned functionCounter = 0;
  ModuleOp module;
};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//

void EdtLoweringPass::runOnOperation() {
  module = getOperation();

  ARTS_DEBUG_HEADER(EdtLoweringPass);
  ARTS_DEBUG_REGION(module->dump(););

  // Collect all task EDTs to process, ordered by nesting depth (innermost
  // first)
  SmallVector<std::pair<EdtOp, unsigned>> taskEdtsWithDepth;
  module.walk([&](EdtOp edtOp) {
    if (canLowerEdt(edtOp)) {
      // Calculate nesting depth
      unsigned depth = 0;
      Operation *parent = edtOp->getParentOp();
      while (parent && parent != module) {
        if (isa<EdtOp>(parent)) {
          depth++;
        }
        parent = parent->getParentOp();
      }
      taskEdtsWithDepth.push_back({edtOp, depth});
    }
  });

  // Sort by depth (descending) - innermost EDTs first
  llvm::sort(taskEdtsWithDepth,
             [](const auto &a, const auto &b) { return a.second > b.second; });

  SmallVector<EdtOp> taskEdts;
  taskEdts.reserve(taskEdtsWithDepth.size());
  for (auto &pair : taskEdtsWithDepth)
    taskEdts.push_back(pair.first);

  ARTS_INFO("Found " << taskEdts.size()
                     << " task EDTs to lower (innermost first)");

  // Process each task EDT (innermost first)
  bool hasFailures = false;
  for (EdtOp edtOp : taskEdts) {
    if (failed(lowerEdt(edtOp))) {
      edtOp.emitError("Failed to lower task EDT");
      hasFailures = true;
    }
  }

  if (hasFailures) {
    return signalPassFailure();
  }

  ARTS_DEBUG_FOOTER(EdtLoweringPass);
  ARTS_DEBUG_REGION(module->dump(););
}

LogicalResult EdtLoweringPass::lowerEdt(EdtOp edtOp) {
  Location loc = edtOp.getLoc();

  // Step 1: Analyze EDT environment (parameters, constants, dependencies)
  EdtEnvironmentManager envMgr(edtOp.getRegion());
  envMgr.analyze();

  SmallVector<Value> dependencies = analyzeDependencies(edtOp);

  ARTS_INFO("Lowering EDT with "
            << envMgr.getParameters().size() << " parameters, "
            << envMgr.getConstants().size() << " constants, "
            << dependencies.size() << " dependencies");

  // Step 2: Create outlined function with ARTS signature
  func::FuncOp outlinedFunc = createOutlinedFunction(edtOp, envMgr);
  if (!outlinedFunc) {
    return edtOp.emitError("Failed to create outlined function");
  }

  // Step 3: Create parameter pack if needed
  OpBuilder builder(edtOp);
  Value paramPack = createParameterPack(builder, loc, envMgr.getParameters());

  // Step 4 & 5: Outline region to function and replace EDT
  if (failed(outlineRegionToFunction(edtOp, outlinedFunc, envMgr))) {
    return edtOp.emitError("Failed to outline region to function");
  }

  // Create edt_outline_fn call
  if (!paramPack) {
    auto emptyType = MemRefType::get({0}, builder.getI64Type());
    paramPack = builder.create<memref::AllocOp>(loc, emptyType);
  }

  // Calculate total dependency count (sum of elements in all dependencies)
  Value depCount = builder.create<arith::ConstantOp>(
      loc, builder.getI32Type(), builder.getI32IntegerAttr(0));
  
  for (Value dep : dependencies) {
    Value numElements = builder.create<DbNumElementsOp>(loc, dep);
    depCount = builder.create<arith::AddIOp>(loc, depCount, numElements);
  }

  auto outlineOp = builder.create<EdtOutlineFnOp>(loc, paramPack, depCount);
  outlineOp->setAttr("outlined_func",
                     builder.getStringAttr(outlinedFunc.getName()));

  Value edtGuid = outlineOp.getGuid();

  // Step 6: Insert dependency management
  builder.setInsertionPointAfter(edtOp);
  if (failed(insertDependencyManagement(builder, loc, edtGuid, dependencies))) {
    return edtOp.emitError("Failed to insert dependency management");
  }

  // Remove original EDT
  edtOp.erase();

  return success();
}

func::FuncOp
EdtLoweringPass::createOutlinedFunction(EdtOp edtOp,
                                        const EdtEnvironmentManager &envMgr) {
  Location loc = edtOp.getLoc();
  OpBuilder moduleBuilder(module.getBodyRegion());

  // Create ARTS outlined function signature: (i32, memref<?xi64>, i32,
  // memref<?xi64>) -> void Paramv/Depv are modeled as memref of i64 words, per
  // edt_param_unpack/edt_dep_unpack requirements
  auto i32Type = moduleBuilder.getI32Type();
  auto i64Type = moduleBuilder.getI64Type();
  auto memrefI64Dyn = MemRefType::get({ShapedType::kDynamic}, i64Type);

  FunctionType funcType =
      FunctionType::get(moduleBuilder.getContext(),
                        {i32Type, memrefI64Dyn, i32Type, memrefI64Dyn}, {});

  std::string funcName = generateUniqueFunctionName();
  auto outlinedFunc =
      moduleBuilder.create<func::FuncOp>(loc, funcName, funcType);
  outlinedFunc.setPrivate();

  ARTS_INFO("Created outlined function: " << funcName);

  return outlinedFunc;
}

Value EdtLoweringPass::createParameterPack(
    OpBuilder &builder, Location loc, const SmallVector<Value> &parameters) {
  if (parameters.empty()) {
    return nullptr;
  }

  ARTS_INFO("Creating parameter pack for " << parameters.size()
                                           << " parameters");

  // Create memref type for parameter pack - i64 word buffer (matches runtime)
  auto memrefType =
      MemRefType::get({ShapedType::kDynamic}, builder.getI64Type());
  auto packOp = builder.create<EdtParamPackOp>(loc, memrefType, parameters);
  return packOp.getMemref();
}

SmallVector<Value> EdtLoweringPass::analyzeDependencies(EdtOp edtOp) {
  SmallVector<Value> dependencies;
  DenseSet<Value> seen;

  // Only treat explicit EDT operands as dependencies; preserve order and dedup.
  for (Value operand : edtOp.getOperands()) {
    if (seen.insert(operand).second)
      dependencies.push_back(operand);
  }

  return dependencies;
}

StringRef EdtLoweringPass::getDependencyMode(Value dep) {
  // Analyze dependency mode based on defining operation
  if (auto dbControlOp = dep.getDefiningOp<DbControlOp>()) {
    switch (dbControlOp.getMode()) {
    case ArtsMode::in:
      return "in";
    case ArtsMode::out:
      return "out";
    case ArtsMode::inout:
      return "inout";
    }
  }

  if (auto dbAcquireOp = dep.getDefiningOp<DbAcquireOp>()) {
    switch (dbAcquireOp.getMode()) {
    case ArtsMode::in:
      return "in";
    case ArtsMode::out:
      return "out";
    case ArtsMode::inout:
      return "inout";
    }
  }

  return "inout"; // Conservative default
}

LogicalResult
EdtLoweringPass::outlineRegionToFunction(EdtOp edtOp, func::FuncOp targetFunc,
                                         const EdtEnvironmentManager &envMgr) {
  Location loc = edtOp.getLoc();
  auto *entryBlock = targetFunc.addEntryBlock();
  OpBuilder funcBuilder(entryBlock, entryBlock->begin());

  // Insert parameter and dependency unpacking
  auto args = entryBlock->getArguments();
  Value paramv = args[1]; // Parameter array (memref<?xi64>)
  Value depv = args[3];   // Dependency array (memref<?xi64>)

  const auto &parameters = envMgr.getParameters();
  SmallVector<Value> unpackedParams;
  if (!parameters.empty()) {
    SmallVector<Type> paramTypes;
    paramTypes.reserve(parameters.size());
    for (Value param : parameters)
      paramTypes.push_back(param.getType());
    auto paramUnpackOp =
        funcBuilder.create<EdtParamUnpackOp>(loc, paramTypes, paramv);
    llvm::append_range(unpackedParams, paramUnpackOp.getResults());
  }

  // Unpack dependencies from dependency array into the types expected by the
  // EDT block
  SmallVector<Value> unpackedDeps;
  Region &edtRegion = edtOp.getRegion();
  Block &edtBlock = edtRegion.front();
  if (!edtBlock.getArguments().empty()) {
    SmallVector<Type> depTypes;
    depTypes.reserve(edtBlock.getNumArguments());
    for (BlockArgument arg : edtBlock.getArguments())
      depTypes.push_back(arg.getType());
    auto depUnpackOp = funcBuilder.create<EdtDepUnpackOp>(loc, depTypes, depv);
    llvm::append_range(unpackedDeps, depUnpackOp.getResults());
  }

  // Clone constants into function
  IRMapping valueMapping;

  // Handle block arguments if any (map them to unpacked dependencies)
  for (auto [edtArg, unpackedDep] :
       llvm::zip(edtBlock.getArguments(), unpackedDeps)) {
    valueMapping.map(edtArg, unpackedDep);
  }

  for (Value constant : envMgr.getConstants())
    if (Operation *defOp = constant.getDefiningOp())
      valueMapping.map(constant, funcBuilder.clone(*defOp)->getResult(0));

  // Handle free variables (values defined outside EDT but used inside)
  // Map parameters directly to their unpacked counterparts; clone true
  // constants.
  if (!parameters.empty())
    for (auto [param, unpacked] : llvm::zip(parameters, unpackedParams))
      valueMapping.map(param, unpacked);
  for (Value freeVar : envMgr.getFreeVariables())
    if (Operation *defOp = freeVar.getDefiningOp())
      if (defOp->hasTrait<OpTrait::ConstantLike>())
        valueMapping.map(freeVar, funcBuilder.clone(*defOp)->getResult(0));

  // Clone region operations (excluding terminator)
  funcBuilder.setInsertionPointToEnd(entryBlock);

  for (Operation &op :
       llvm::make_early_inc_range(edtBlock.without_terminator())) {
    Operation *clonedOp = funcBuilder.clone(op, valueMapping);
    for (auto [orig, clone] :
         llvm::zip(op.getResults(), clonedOp->getResults()))
      valueMapping.map(orig, clone);
  }

  // Add return terminator
  funcBuilder.create<func::ReturnOp>(loc);

  return success();
}

LogicalResult
EdtLoweringPass::insertDependencyManagement(OpBuilder &builder, Location loc,
                                            Value edtGuid,
                                            const SmallVector<Value> &deps) {
  if (deps.empty()) {
    return success();
  }

  // Separate dependencies by mode
  SmallVector<Value> inDeps, outDeps;
  
  for (Value dep : deps) {
    StringRef mode = getDependencyMode(dep);

    // Collect input dependencies
    if (mode == "in" || mode == "inout") {
      inDeps.push_back(dep);
    }

    // Collect output dependencies
    if (mode == "out" || mode == "inout") {
      outDeps.push_back(dep);
    }
  }

  // Record all input dependencies at once
  if (!inDeps.empty()) {
    builder.create<RecordInDepOp>(loc, edtGuid, inDeps);
  }

  // Increment output latches for all dependencies at once
  if (!outDeps.empty()) {
    builder.create<IncrementOutLatchOp>(loc, edtGuid, outDeps);
  }

  return success();
}

std::string EdtLoweringPass::generateUniqueFunctionName() {
  return "__arts_edt_" + std::to_string(++functionCounter);
}

bool EdtLoweringPass::canLowerEdt(EdtOp edtOp) {
  // Only lower task EDTs with non-empty single-block regions
  return edtOp.getType() == EdtType::task && !edtOp.getRegion().empty() &&
         edtOp.getRegion().hasOneBlock();
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