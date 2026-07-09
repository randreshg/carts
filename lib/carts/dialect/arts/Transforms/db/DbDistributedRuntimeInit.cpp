///==========================================================================///
/// File: DbDistributedRuntimeInit.cpp
///
/// Builds distributed DB initialization callbacks from ARTS-owned DB block
/// grids and owner-route queries. The callbacks contain explicit ARTS-RT
/// runtime ops so ARTS-RT-to-LLVM only lowers ABI calls.
///==========================================================================///

#include "carts/dialect/arts-rt/Conversion/ArtsRtToLLVM/Types.h"
#include "carts/dialect/arts-rt/IR/RtDialect.h"
#include "carts/dialect/arts-rt/Utils/RtDbUtils.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/passes/Passes.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#define GEN_PASS_DEF_DBDISTRIBUTEDRUNTIMEINIT
#include "carts/passes/Passes.h.inc"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;
using namespace mlir::carts::arts_rt;

namespace {

static Value createTotalNodesValue(OpBuilder &builder, Location loc) {
  return RuntimeQueryOp::create(builder, loc, RuntimeQueryKind::totalNodes);
}

struct RuntimeInitBuilder {
  ModuleOp module;
  OpBuilder builder;
  Type Int32;
  Type Int64;
  Type llvmPtr;
  FunctionType InitPerNodeFn;
  FunctionType InitPerWorkerFn;

  explicit RuntimeInitBuilder(ModuleOp module)
      : module(module), builder(module.getContext()) {
    MLIRContext *ctx = module.getContext();
    Int32 = IntegerType::get(ctx, 32);
    Int64 = IntegerType::get(ctx, 64);
    llvmPtr = LLVM::LLVMPointerType::get(ctx);
    Type int8 = IntegerType::get(ctx, 8);
    Type int8Ptr = MemRefType::get({ShapedType::kDynamic}, int8);
    Type int8PtrPtr = MemRefType::get({ShapedType::kDynamic}, int8Ptr);
    InitPerNodeFn =
        FunctionType::get(ctx, {Int32, Int32, int8PtrPtr}, TypeRange{});
    InitPerWorkerFn =
        FunctionType::get(ctx, {Int32, Int32, Int32, int8PtrPtr}, TypeRange{});
  }

  MLIRContext *getContext() { return builder.getContext(); }
  ModuleOp getModule() { return module; }
  OpBuilder &getBuilder() { return builder; }

  template <typename OpTy, typename... Args>
  OpTy create(Location location, Args &&...args) {
    return OpTy::create(builder, location, std::forward<Args>(args)...);
  }

  void setInsertionPoint(ModuleOp target) {
    builder.setInsertionPointToStart(target.getBody());
  }
  void setInsertionPointToStart(Block *block) {
    builder.setInsertionPointToStart(block);
  }
  void setInsertionPointAfter(Operation *op) {
    builder.setInsertionPointAfter(op);
  }

  Value createIndexConstant(int64_t value, Location loc) {
    return create<arith::ConstantIndexOp>(loc, value);
  }

  Value createIntConstant(int64_t value, Type type, Location loc) {
    return create<arith::ConstantOp>(loc, type,
                                     builder.getIntegerAttr(type, value));
  }

  Value castToIndex(Value value, Location loc) {
    return ValueAnalysis::castToIndex(value, builder, loc);
  }

  Value castToInt(Type targetType, Value value, Location loc) {
    if (value.getType() == targetType)
      return value;
    if (value.getType().isIndex())
      return create<arith::IndexCastOp>(loc, targetType, value);
    auto srcInt = dyn_cast<IntegerType>(value.getType());
    auto dstInt = dyn_cast<IntegerType>(targetType);
    if (!srcInt || !dstInt)
      return value;
    if (srcInt.getWidth() < dstInt.getWidth())
      return create<arith::ExtSIOp>(loc, targetType, value);
    return create<arith::TruncIOp>(loc, targetType, value);
  }

  Value ensureI64(Value value, Location loc) {
    return castToInt(Int64, value, loc);
  }

  Value computeElementTypeSize(Type elementType, Location loc) {
    return create<polygeist::TypeSizeOp>(loc, builder.getIndexType(),
                                         elementType);
  }

  Value computeTotalElements(ArrayRef<Value> sizes, Location loc) {
    if (sizes.empty())
      return createIndexConstant(1, loc);
    Value total = castToIndex(sizes.front(), loc);
    for (Value size : sizes.drop_front())
      total = create<arith::MulIOp>(loc, total, castToIndex(size, loc));
    return total;
  }
};

static memref::GlobalOp getOrCreateMemrefGlobal(RuntimeInitBuilder *AC,
                                                StringRef symbolName,
                                                MemRefType type, Location loc) {
  ModuleOp module = AC->getModule();
  if (auto existing = module.lookupSymbol<memref::GlobalOp>(symbolName))
    return existing;

  OpBuilder::InsertionGuard guard(AC->getBuilder());
  AC->setInsertionPoint(module);
  auto global = AC->create<memref::GlobalOp>(loc, symbolName, StringAttr(),
                                             type, Attribute(),
                                             /*constant=*/false, IntegerAttr());
  global.setSymVisibility("private");
  return global;
}

static LogicalResult collectDependencies(Value value, IRMapping &mapper,
                                         llvm::DenseSet<Value> &visited,
                                         llvm::SetVector<Value> &values) {
  if (!value)
    return failure();
  if (!visited.insert(value).second || mapper.contains(value))
    return success();

  if (auto blockArg = dyn_cast<BlockArgument>(value))
    return mapper.contains(blockArg) ? success() : failure();

  Operation *defOp = value.getDefiningOp();
  if (!defOp)
    return failure();
  for (Value operand : defOp->getOperands())
    if (failed(collectDependencies(operand, mapper, visited, values)))
      return failure();
  values.insert(value);
  return success();
}

static bool isRuntimeTopologyCall(Operation *cloneOp) {
  if (auto queryOp = dyn_cast<RuntimeQueryOp>(cloneOp)) {
    auto kind = queryOp.getKind();
    return kind == RuntimeQueryKind::totalNodes ||
           kind == RuntimeQueryKind::totalWorkers;
  }
  auto callOp = dyn_cast<func::CallOp>(cloneOp);
  if (!callOp)
    return false;
  auto callee = callOp.getCallee();
  return callee == "artsGetTotalNodes" || callee == "artsGetTotalWorkers";
}

static LogicalResult
cloneDbSizeDependencies(RuntimeInitBuilder *AC, DbAllocOp op,
                        func::FuncOp callback, IRMapping &mapper,
                        ValueRange dbSizes, ValueRange elementSizes,
                        SmallVectorImpl<Value> &mappedSizes,
                        SmallVectorImpl<Value> &mappedElementSizes) {
  llvm::SetVector<Value> valuesToClone;
  llvm::DenseSet<Value> visited;
  for (Value size : dbSizes) {
    if (failed(collectDependencies(size, mapper, visited, valuesToClone)))
      return op.emitOpError()
             << "cannot clone distributed DB size expression into init "
                "callback";
  }
  for (Value size : elementSizes) {
    if (failed(collectDependencies(size, mapper, visited, valuesToClone)))
      return op.emitOpError()
             << "cannot clone distributed DB element-size expression into "
                "init callback";
  }

  if (!ValueAnalysis::cloneValuesIntoRegion(
          valuesToClone, &callback.getBody(), mapper, AC->getBuilder(),
          /*allowMemoryEffectFree=*/true, isRuntimeTopologyCall))
    return op.emitOpError()
           << "cannot clone distributed DB size expressions into init callback";

  auto mapValue = [&](Value value) { return mapper.lookupOrNull(value); };
  for (Value size : dbSizes) {
    Value mapped = mapValue(size);
    if (!mapped)
      return op.emitOpError()
             << "missing mapped DB size value in init callback";
    mappedSizes.push_back(mapped);
  }
  for (Value size : elementSizes) {
    Value mapped = mapValue(size);
    if (!mapped)
      return op.emitOpError()
             << "missing mapped DB element-size value in init callback";
    mappedElementSizes.push_back(mapped);
  }
  return success();
}

struct DbDistributedInitBuilder {
  RuntimeInitBuilder *AC = nullptr;
  DbAllocOp op;
  std::optional<int64_t> nextId;
  DbOwnerRouteFacts ownerRoute;

  Value computeOwnerRouteForLinearIndex(ArrayRef<Value> dbSizes,
                                        Value linearIndex, Location loc) const {
    return createDbOwnerRouteForLinearIndex(
        AC->getBuilder(), loc, dbSizes, linearIndex,
        createTotalNodesValue(AC->getBuilder(), loc), ownerRoute);
  }

  Value createDbFromGuidAtIndex(Value dbMemref, Value guid, Value linearIndex,
                                Value elementSize, Location loc,
                                Value hintRoute, bool requireLocalOwner) {
    Value elemSize64 = AC->ensureI64(elementSize, loc);
    Value artsIdValue;
    if (nextId.has_value()) {
      Value baseArtsId = AC->createIntConstant(*nextId, AC->Int64, loc);
      Value linearIndex64 = AC->ensureI64(linearIndex, loc);
      artsIdValue = AC->create<arith::AddIOp>(loc, baseArtsId, linearIndex64);
    } else {
      artsIdValue = AC->createIntConstant(0, AC->Int64, loc);
    }
    if (!hintRoute)
      hintRoute = AC->createIntConstant(0, AC->Int32, loc);
    Value dbType = AC->createIntConstant(ARTS_DB_DEFAULT, AC->Int32, loc);
    Value ptr =
        requireLocalOwner
            ? AC->create<DbCreateWithGuidLocalOp>(loc, AC->llvmPtr, guid,
                                                  elemSize64, dbType, hintRoute,
                                                  artsIdValue)
                  .getPtr()
            : AC->create<DbCreateWithGuidOp>(loc, AC->llvmPtr, guid, elemSize64,
                                             dbType, hintRoute, artsIdValue)
                  .getPtr();
    AC->create<memref::StoreOp>(loc, ptr, dbMemref, ValueRange{linearIndex});
    return ptr;
  }

  LogicalResult
  createSingleDb(Value dbMemref, Value guidMemref, Value elementSize,
                 Location loc, bool createDb, ArrayRef<Value> sizes = {},
                 std::optional<Value> linearIndexOverride = std::nullopt,
                 Value localNodeForCreate = {}) {
    Value linearIndex = linearIndexOverride.has_value()
                            ? *linearIndexOverride
                            : AC->createIndexConstant(0, loc);
    Value reserveRoute =
        computeOwnerRouteForLinearIndex(sizes, linearIndex, loc);
    if (!reserveRoute)
      return op.emitOpError()
             << "cannot build owner route for distributed DB init";

    Value dbTypeConst = AC->createIntConstant(ARTS_DB, AC->Int32, loc);
    Value guid =
        AC->create<DbGuidReserveOp>(loc, AC->Int64, dbTypeConst, reserveRoute)
            .getGuid();
    AC->create<memref::StoreOp>(loc, guid, guidMemref, ValueRange{linearIndex});

    if (!createDb)
      return success();

    auto createDbBody = [&]() -> LogicalResult {
      createDbFromGuidAtIndex(dbMemref, guid, linearIndex, elementSize, loc,
                              reserveRoute,
                              static_cast<bool>(localNodeForCreate));
      return success();
    };
    if (localNodeForCreate) {
      Value localRoute = AC->castToInt(AC->Int32, localNodeForCreate, loc);
      Value isLocal = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                                reserveRoute, localRoute);
      auto guard = AC->create<scf::IfOp>(loc, isLocal, false);
      AC->setInsertionPointToStart(&guard.getThenRegion().front());
      if (failed(createDbBody()))
        return failure();
      AC->setInsertionPointAfter(guard);
      return success();
    }
    return createDbBody();
  }

  LogicalResult createMultiDbs(Value dbMemref, Value guidMemref,
                               ArrayRef<Value> sizes, Value elementSize,
                               Location loc, bool createDb,
                               Value localNodeForCreate = {}) {
    Value totalElems = AC->computeTotalElements(sizes, loc);
    Value lowerBound = AC->createIndexConstant(0, loc);
    Value step = AC->createIndexConstant(1, loc);
    auto linearLoop = AC->create<scf::ForOp>(loc, lowerBound, totalElems, step);
    AC->setInsertionPointToStart(&linearLoop.getRegion().front());
    LogicalResult result =
        createSingleDb(dbMemref, guidMemref, elementSize, loc, createDb, sizes,
                       linearLoop.getInductionVar(), localNodeForCreate);
    AC->setInsertionPointAfter(linearLoop);
    return result;
  }
};

static LogicalResult emitDistributedInitCallbacks(RuntimeInitBuilder *AC,
                                                  DbAllocOp op) {
  DbLoweringInfo dbLowering = RtDbUtils::extractDbLoweringInfo(op);
  SmallVector<Value> dbSizes(dbLowering.sizes.begin(), dbLowering.sizes.end());
  if (dbSizes.empty())
    return op.emitOpError()
           << "distributed DB init requires an explicit DB block grid";

  FailureOr<DbOwnerRouteFacts> ownerRoute =
      readDistributedDbOwnerRouteFactsFromTypedAbi(op);
  if (failed(ownerRoute))
    return op.emitOpError()
           << "distributed DB init requires typed ARTS DB layout and "
              "placement facts";

  std::optional<int64_t> nextId;
  if (auto createIdAttr = op->getAttrOfType<IntegerAttr>(
          arts::AttrNames::Operation::ArtsCreateId))
    nextId = createIdAttr.getInt();
  else
    nextId = getArtsId(op);

  std::optional<std::string> baseName = getDistributedDbRuntimeInitBaseName(op);
  if (!baseName)
    return op.emitOpError()
           << "distributed DB init requires stable arts.id or arts.create_id";
  std::string guidHolderSymbol = *baseName + "_guid_holder";
  std::string ptrHolderSymbol = *baseName + "_ptr_holder";
  std::string nodeInitSymbol;
  std::string workerInitSymbol = *baseName + "_worker_init";

  bool explicitSingleNode = false;
  bool hasExplicitNodeCount = false;
  std::optional<int64_t> totalNodes =
      arts::getRuntimeTotalNodes(AC->getModule());
  if (totalNodes) {
    hasExplicitNodeCount = true;
    explicitSingleNode = *totalNodes <= 1;
  }
  bool parallelInit = !(hasExplicitNodeCount && explicitSingleNode);
  nodeInitSymbol = *baseName + (parallelInit ? "_reserve_init" : "_init");

  auto guidDynamicType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
  auto ptrDynamicType = MemRefType::get({ShapedType::kDynamic}, AC->llvmPtr);
  auto guidHolderType = MemRefType::get({1}, guidDynamicType);
  auto ptrHolderType = MemRefType::get({1}, ptrDynamicType);
  getOrCreateMemrefGlobal(AC, guidHolderSymbol, guidHolderType, op.getLoc());
  getOrCreateMemrefGlobal(AC, ptrHolderSymbol, ptrHolderType, op.getLoc());

  ModuleOp module = AC->getModule();
  if (module.lookupSymbol<func::FuncOp>(nodeInitSymbol))
    return success();

  OpBuilder::InsertionGuard guard(AC->getBuilder());
  AC->setInsertionPoint(module);
  func::FuncOp initFn =
      AC->create<func::FuncOp>(op.getLoc(), nodeInitSymbol, AC->InitPerNodeFn);
  initFn.setPrivate();
  Block *entry = initFn.addEntryBlock();
  AC->setInsertionPointToStart(entry);

  IRMapping mapper;
  auto parentFn = op->getParentOfType<func::FuncOp>();
  if (parentFn && parentFn.getNumArguments() > 0) {
    if (parentFn.getNumArguments() != 2 || initFn.getNumArguments() != 3)
      return op.emitOpError()
             << "cannot map parent function arguments into DB init callback";
    mapper.map(parentFn.getArgument(0), initFn.getArgument(1));
    mapper.map(parentFn.getArgument(1), initFn.getArgument(2));
  }

  SmallVector<Value, 4> callbackDbSizes;
  SmallVector<Value, 4> callbackElementSizes;
  if (failed(cloneDbSizeDependencies(AC, op, initFn, mapper, dbSizes,
                                     op.getElementSizes(), callbackDbSizes,
                                     callbackElementSizes)))
    return failure();

  Value callbackTotalElems =
      AC->computeTotalElements(callbackDbSizes, op.getLoc());
  Value guidBuffer = AC->create<memref::AllocOp>(
      op.getLoc(), guidDynamicType, ValueRange{callbackTotalElems});
  Value ptrBuffer = AC->create<memref::AllocOp>(op.getLoc(), ptrDynamicType,
                                                ValueRange{callbackTotalElems});

  Value guidHolder = AC->create<memref::GetGlobalOp>(
      op.getLoc(), guidHolderType, guidHolderSymbol);
  Value ptrHolder = AC->create<memref::GetGlobalOp>(op.getLoc(), ptrHolderType,
                                                    ptrHolderSymbol);
  Value zero = AC->createIndexConstant(0, op.getLoc());
  AC->create<memref::StoreOp>(op.getLoc(), guidBuffer, guidHolder,
                              ValueRange{zero});
  AC->create<memref::StoreOp>(op.getLoc(), ptrBuffer, ptrHolder,
                              ValueRange{zero});

  Value callbackElementSize =
      AC->computeElementTypeSize(op.getElementType(), op.getLoc());
  Value callbackPayloadSize = AC->createIndexConstant(1, op.getLoc());
  for (Value dim : callbackElementSizes)
    callbackPayloadSize =
        AC->create<arith::MulIOp>(op.getLoc(), callbackPayloadSize, dim);
  Value callbackTotalDbSize = AC->create<arith::MulIOp>(
      op.getLoc(), callbackElementSize, callbackPayloadSize);
  Value callbackNodeId = initFn.getArgument(0);

  DbDistributedInitBuilder builder{AC, op, nextId, *ownerRoute};
  if (!parallelInit) {
    if (dbLowering.isSingleElement) {
      if (failed(builder.createSingleDb(ptrBuffer, guidBuffer,
                                        callbackTotalDbSize, op.getLoc(),
                                        /*createDb=*/true, callbackDbSizes,
                                        std::nullopt, callbackNodeId)))
        return failure();
    } else if (failed(builder.createMultiDbs(
                   ptrBuffer, guidBuffer, callbackDbSizes, callbackTotalDbSize,
                   op.getLoc(), /*createDb=*/true, callbackNodeId))) {
      return failure();
    }
    AC->create<func::ReturnOp>(op.getLoc());
    return success();
  }

  if (dbLowering.isSingleElement) {
    if (failed(builder.createSingleDb(ptrBuffer, guidBuffer,
                                      callbackTotalDbSize, op.getLoc(),
                                      /*createDb=*/false, callbackDbSizes)))
      return failure();
  } else if (failed(builder.createMultiDbs(ptrBuffer, guidBuffer,
                                           callbackDbSizes, callbackTotalDbSize,
                                           op.getLoc(), /*createDb=*/false))) {
    return failure();
  }
  AC->create<func::ReturnOp>(op.getLoc());

  if (module.lookupSymbol<func::FuncOp>(workerInitSymbol))
    return success();

  AC->setInsertionPoint(module);
  func::FuncOp workerInitFn = AC->create<func::FuncOp>(
      op.getLoc(), workerInitSymbol, AC->InitPerWorkerFn);
  workerInitFn.setPrivate();

  Block *workerEntry = workerInitFn.addEntryBlock();
  AC->setInsertionPointToStart(workerEntry);
  IRMapping workerMapper;
  if (parentFn && parentFn.getNumArguments() > 0) {
    if (parentFn.getNumArguments() != 2 || workerInitFn.getNumArguments() != 4)
      return op.emitOpError()
             << "cannot map parent function arguments into DB worker callback";
    workerMapper.map(parentFn.getArgument(0), workerInitFn.getArgument(2));
    workerMapper.map(parentFn.getArgument(1), workerInitFn.getArgument(3));
  }

  SmallVector<Value, 4> workerDbSizes;
  SmallVector<Value, 4> workerElementSizes;
  if (failed(cloneDbSizeDependencies(AC, op, workerInitFn, workerMapper,
                                     dbSizes, op.getElementSizes(),
                                     workerDbSizes, workerElementSizes)))
    return failure();

  Value workerGuidHolder = AC->create<memref::GetGlobalOp>(
      op.getLoc(), guidHolderType, guidHolderSymbol);
  Value workerPtrHolder = AC->create<memref::GetGlobalOp>(
      op.getLoc(), ptrHolderType, ptrHolderSymbol);
  Value zeroIdx = AC->createIndexConstant(0, op.getLoc());
  Value workerGuidBuffer = AC->create<memref::LoadOp>(
      op.getLoc(), workerGuidHolder, ValueRange{zeroIdx});
  Value workerPtrBuffer = AC->create<memref::LoadOp>(
      op.getLoc(), workerPtrHolder, ValueRange{zeroIdx});
  Value workerTotalElems = AC->computeTotalElements(workerDbSizes, op.getLoc());

  Value workerElementSize =
      AC->computeElementTypeSize(op.getElementType(), op.getLoc());
  Value workerPayloadSize = AC->createIndexConstant(1, op.getLoc());
  for (Value dim : workerElementSizes)
    workerPayloadSize =
        AC->create<arith::MulIOp>(op.getLoc(), workerPayloadSize, dim);
  Value workerTotalDbSize = AC->create<arith::MulIOp>(
      op.getLoc(), workerElementSize, workerPayloadSize);

  Value workerNodeId = workerInitFn.getArgument(0);
  Value workerLocalId = workerInitFn.getArgument(1);
  Value zeroI32 = AC->createIntConstant(0, AC->Int32, op.getLoc());
  Value isPrimaryWorker = AC->create<arith::CmpIOp>(
      op.getLoc(), arith::CmpIPredicate::eq, workerLocalId, zeroI32);
  auto primaryWorkerIf = AC->create<scf::IfOp>(op.getLoc(), isPrimaryWorker,
                                               /*withElseRegion=*/false);
  AC->setInsertionPointToStart(&primaryWorkerIf.getThenRegion().front());
  Value workerNodeIndex = AC->castToIndex(workerNodeId, op.getLoc());
  if (ownerRoute->policy == DbOwnerRoutePolicy::LinearModNodes) {
    Value workerTotalNodes = AC->castToIndex(
        createTotalNodesValue(AC->getBuilder(), op.getLoc()), op.getLoc());
    auto workerLoop = AC->create<scf::ForOp>(
        op.getLoc(), workerNodeIndex, workerTotalElems, workerTotalNodes);
    AC->setInsertionPointToStart(&workerLoop.getRegion().front());
    Value linearIndex = workerLoop.getInductionVar();
    Value reservedGuid = AC->create<memref::LoadOp>(
        op.getLoc(), workerGuidBuffer, ValueRange{linearIndex});
    builder.createDbFromGuidAtIndex(workerPtrBuffer, reservedGuid, linearIndex,
                                    workerTotalDbSize, op.getLoc(),
                                    workerNodeId,
                                    /*requireLocalOwner=*/true);
    AC->setInsertionPointAfter(workerLoop);
  } else {
    Value lowerBound = AC->createIndexConstant(0, op.getLoc());
    Value step = AC->createIndexConstant(1, op.getLoc());
    auto workerLoop =
        AC->create<scf::ForOp>(op.getLoc(), lowerBound, workerTotalElems, step);
    AC->setInsertionPointToStart(&workerLoop.getRegion().front());
    Value linearIndex = workerLoop.getInductionVar();
    Value ownerRouteValue = builder.computeOwnerRouteForLinearIndex(
        workerDbSizes, linearIndex, op.getLoc());
    if (!ownerRouteValue)
      return op.emitOpError()
             << "cannot build owner route for distributed DB worker init";
    Value workerRoute = AC->castToInt(AC->Int32, workerNodeId, op.getLoc());
    Value ownsBlock = AC->create<arith::CmpIOp>(
        op.getLoc(), arith::CmpIPredicate::eq, ownerRouteValue, workerRoute);
    auto ownerIf =
        AC->create<scf::IfOp>(op.getLoc(), ownsBlock, /*withElseRegion=*/false);
    AC->setInsertionPointToStart(&ownerIf.getThenRegion().front());
    Value reservedGuid = AC->create<memref::LoadOp>(
        op.getLoc(), workerGuidBuffer, ValueRange{linearIndex});
    builder.createDbFromGuidAtIndex(workerPtrBuffer, reservedGuid, linearIndex,
                                    workerTotalDbSize, op.getLoc(),
                                    ownerRouteValue,
                                    /*requireLocalOwner=*/true);
    AC->setInsertionPointAfter(ownerIf);
    AC->setInsertionPointAfter(workerLoop);
  }
  AC->setInsertionPointAfter(primaryWorkerIf);
  AC->create<func::ReturnOp>(op.getLoc());
  return success();
}

struct DbDistributedRuntimeInitPass
    : public ::impl::DbDistributedRuntimeInitBase<
          DbDistributedRuntimeInitPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    RuntimeInitBuilder builder(module);
    RuntimeInitBuilder *AC = &builder;

    SmallVector<DbAllocOp, 8> distributedAllocs;
    module.walk([&](DbAllocOp alloc) {
      if (hasDistributedDbAllocation(alloc.getOperation()))
        distributedAllocs.push_back(alloc);
    });

    for (DbAllocOp alloc : distributedAllocs) {
      if (failed(emitDistributedInitCallbacks(AC, alloc))) {
        signalPassFailure();
        return;
      }
    }
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createDbDistributedRuntimeInitPass() {
  return std::make_unique<DbDistributedRuntimeInitPass>();
}
