#include "CompileDriver.h"

#include "carts/dialect/arts-rt/IR/RtDialect.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/passes/Passes.h"
#include "mlir/Conversion/Passes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Async/IR/Async.h"
#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Dialect/Func/Extensions/InlinerExtension.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/NVVMDialect.h"
#include "mlir/Dialect/LLVMIR/ROCDLDialect.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllExtensions.h"
#include "mlir/InitAllPasses.h"
#include "mlir/InitAllTranslations.h"
#include "mlir/Interfaces/FunctionInterfaces.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"
#include "mlir/Transforms/Passes.h"
#include "polygeist/Dialect.h"
#include "polygeist/Ops.h"
#include "polygeist/Passes/Passes.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include <optional>
#include <string>

using namespace llvm;
using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::compile_driver {
namespace {

/// Use the original models for attaching type interfaces.
class MemRefInsider
    : public MemRefElementTypeInterface::FallbackModel<MemRefInsider> {};

template <typename T>
struct PtrElementModel
    : public LLVM::PointerElementTypeInterface::ExternalModel<
          PtrElementModel<T>, T> {};

bool isArtsOutlinedEdtName(StringRef name) {
  return name.starts_with("__arts_edt_");
}

LogicalResult promoteOutlinedEdtToLLVMFunc(func::FuncOp funcOp,
                                           OpBuilder &builder) {
  mlir::FunctionType funcType = funcOp.getFunctionType();
  MLIRContext *ctx = funcOp.getContext();

  mlir::Type resultType = LLVM::LLVMVoidType::get(ctx);
  if (funcType.getNumResults() > 1)
    return funcOp.emitError("cannot realize ARTS EDT function pointer for "
                            "multi-result function");
  if (funcType.getNumResults() == 1) {
    resultType = funcType.getResult(0);
    if (!LLVM::isCompatibleType(resultType))
      return funcOp.emitError("cannot realize ARTS EDT function pointer "
                              "before LLVM conversion for result type ")
             << resultType;
  }

  SmallVector<mlir::Type, 8> inputTypes;
  inputTypes.reserve(funcType.getNumInputs());
  for (mlir::Type input : funcType.getInputs()) {
    if (!LLVM::isCompatibleType(input))
      return funcOp.emitError("cannot realize ARTS EDT function pointer "
                              "before LLVM conversion for argument type ")
             << input;
    inputTypes.push_back(input);
  }

  auto llvmType =
      LLVM::LLVMFunctionType::get(resultType, inputTypes, /*isVarArg=*/false);
  builder.setInsertionPoint(funcOp);
  auto llvmFunc = LLVM::LLVMFuncOp::create(
      builder, funcOp.getLoc(), funcOp.getName(), llvmType,
      LLVM::Linkage::External, /*dsoLocal=*/false, LLVM::CConv::C);
  cast<FunctionOpInterface>(llvmFunc.getOperation())
      .setVisibility(funcOp.getVisibility());
  llvmFunc.getBody().takeBody(funcOp.getBody());
  SmallVector<func::ReturnOp, 4> returns;
  llvmFunc.walk([&](func::ReturnOp ret) { returns.push_back(ret); });
  for (func::ReturnOp ret : returns) {
    builder.setInsertionPoint(ret);
    LLVM::ReturnOp::create(builder, ret.getLoc(), ret.getOperands());
    ret.erase();
  }
  funcOp.erase();
  return success();
}

struct RealizeArtsFunctionPointersPass
    : public PassWrapper<RealizeArtsFunctionPointersPass,
                         OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(RealizeArtsFunctionPointersPass)

  StringRef getArgument() const final {
    return "carts-realize-arts-function-pointers";
  }

  StringRef getDescription() const final {
    return "Realize ARTS EDT function pointer symbols before mixed "
           "host-OpenMP LLVM emission";
  }

  void runOnOperation() final {
    ModuleOp module = getOperation();
    if (!hasResidualOpenMP(module))
      return;

    SmallVector<polygeist::GetFuncOp, 8> getFuncOps;
    module.walk([&](polygeist::GetFuncOp op) {
      if (isArtsOutlinedEdtName(op.getName()))
        getFuncOps.push_back(op);
    });
    if (getFuncOps.empty())
      return;

    OpBuilder builder(module.getContext());
    for (polygeist::GetFuncOp getFunc : getFuncOps) {
      StringRef name = getFunc.getName();
      if (!module.lookupSymbol<LLVM::LLVMFuncOp>(name)) {
        auto funcOp = module.lookupSymbol<func::FuncOp>(name);
        if (!funcOp) {
          getFunc.emitError("ARTS EDT function pointer target was not found: ")
              << name;
          signalPassFailure();
          return;
        }
        if (failed(promoteOutlinedEdtToLLVMFunc(funcOp, builder))) {
          signalPassFailure();
          return;
        }
      }

      builder.setInsertionPoint(getFunc);
      auto address = LLVM::AddressOfOp::create(builder, getFunc.getLoc(),
                                               getFunc.getType(), name);
      getFunc.getResult().replaceAllUsesWith(address.getResult());
      getFunc.erase();
    }
  }
};

bool isLLVMMemRefDescriptorLike(mlir::Type type) {
  auto structType = dyn_cast<LLVM::LLVMStructType>(type);
  if (!structType || structType.getBody().size() < 2)
    return false;
  return isa<LLVM::LLVMPointerType>(structType.getBody()[0]) &&
         isa<LLVM::LLVMPointerType>(structType.getBody()[1]);
}

mlir::Value createLLVMIndexConstant(OpBuilder &builder, Location loc,
                                    mlir::Type type, int64_t value) {
  return LLVM::ConstantOp::create(builder, loc, type,
                                  builder.getIntegerAttr(type, value));
}

mlir::Value buildRankOneDescriptorFromBarePtr(OpBuilder &builder, Location loc,
                                              mlir::Value ptr,
                                              mlir::Type descriptorType) {
  auto structType = dyn_cast<LLVM::LLVMStructType>(descriptorType);
  if (!structType || structType.getBody().size() < 5)
    return {};

  mlir::Type indexType = structType.getBody()[2];
  mlir::Value zero = createLLVMIndexConstant(builder, loc, indexType, 0);
  mlir::Value one = createLLVMIndexConstant(builder, loc, indexType, 1);

  mlir::Value descriptor = LLVM::PoisonOp::create(builder, loc, descriptorType);
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, ptr,
                                           ArrayRef<int64_t>{0});
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, ptr,
                                           ArrayRef<int64_t>{1});
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, zero,
                                           ArrayRef<int64_t>{2});
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, zero,
                                           ArrayRef<int64_t>{3, 0});
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, one,
                                           ArrayRef<int64_t>{4, 0});
  return descriptor;
}

void cleanupResidualHostOpenMPMemrefs(ModuleOp module) {
  SmallVector<UnrealizedConversionCastOp> castsToDescriptor;
  module.walk([&](UnrealizedConversionCastOp cast) {
    if (cast.getNumOperands() != 1 || cast.getNumResults() != 1)
      return;
    if (!isLLVMMemRefDescriptorLike(cast.getResult(0).getType()))
      return;
    auto ptrToMemref =
        cast.getOperand(0).getDefiningOp<polygeist::Pointer2MemrefOp>();
    if (!ptrToMemref)
      return;
    auto memrefType = dyn_cast<MemRefType>(ptrToMemref.getType());
    if (!memrefType || memrefType.getRank() != 1 || memrefType.hasStaticShape())
      return;
    castsToDescriptor.push_back(cast);
  });

  for (UnrealizedConversionCastOp cast : castsToDescriptor) {
    if (!cast)
      continue;
    auto ptrToMemref =
        cast.getOperand(0).getDefiningOp<polygeist::Pointer2MemrefOp>();
    if (!ptrToMemref)
      continue;
    OpBuilder builder(cast);
    mlir::Value descriptor = buildRankOneDescriptorFromBarePtr(
        builder, cast.getLoc(), ptrToMemref.getSource(),
        cast.getResult(0).getType());
    if (!descriptor)
      continue;
    cast.getResult(0).replaceAllUsesWith(descriptor);
    cast.erase();
    if (ptrToMemref->use_empty())
      ptrToMemref.erase();
  }

  SmallVector<polygeist::Memref2PointerOp> memrefToPointerOps;
  module.walk([&](polygeist::Memref2PointerOp op) {
    auto sourceCast =
        op.getSource().getDefiningOp<UnrealizedConversionCastOp>();
    if (!sourceCast || sourceCast.getNumOperands() != 1 ||
        !isLLVMMemRefDescriptorLike(sourceCast.getOperand(0).getType()))
      return;
    memrefToPointerOps.push_back(op);
  });

  for (polygeist::Memref2PointerOp op : memrefToPointerOps) {
    if (!op)
      continue;
    auto sourceCast =
        op.getSource().getDefiningOp<UnrealizedConversionCastOp>();
    if (!sourceCast)
      continue;
    OpBuilder builder(op);
    mlir::Value ptr = LLVM::ExtractValueOp::create(
        builder, op.getLoc(), sourceCast.getOperand(0), /*position=*/1);
    if (ptr.getType() != op.getType())
      ptr = LLVM::AddrSpaceCastOp::create(builder, op.getLoc(), op.getType(),
                                          ptr);
    op.getResult().replaceAllUsesWith(ptr);
    op.erase();
    if (sourceCast->use_empty())
      sourceCast.erase();
  }
}

struct ResidualHostOpenMPMemrefCleanupPass
    : public PassWrapper<ResidualHostOpenMPMemrefCleanupPass,
                         OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(
      ResidualHostOpenMPMemrefCleanupPass)

  StringRef getArgument() const final {
    return "carts-residual-host-openmp-memref-cleanup";
  }

  StringRef getDescription() const final {
    return "Clean residual host-OpenMP memref/pointer bridges before LLVM "
           "emission";
  }

  void runOnOperation() final {
    ModuleOp module = getOperation();
    if (!hasResidualOpenMP(module))
      return;
    cleanupResidualHostOpenMPMemrefs(module);
  }
};

bool hasArtsRuntimeCallsOutsideHostWrappers(llvm::Module &module) {
  for (llvm::Function &function : module) {
    if (function.isDeclaration())
      continue;
    if (function.getName() == "main" || function.getName() == "main_edt")
      continue;

    for (llvm::BasicBlock &block : function) {
      for (llvm::Instruction &instruction : block) {
        auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction);
        if (!call)
          continue;
        llvm::Function *callee = call->getCalledFunction();
        if (callee && callee->getName().starts_with("arts_"))
          return true;
      }
    }
  }
  return false;
}

} // namespace

void configureArtsDebugChannels(llvm::StringRef channels) {
  if (channels.empty())
    return;

  llvm::SmallVector<llvm::StringRef, 8> splitChannels;
  channels.split(splitChannels, ',', -1, false);

  static llvm::SmallVector<std::string, 8> ownedChannels;
  ownedChannels.clear();
  ownedChannels.reserve(splitChannels.size());
  for (llvm::StringRef channel : splitChannels) {
    llvm::StringRef trimmed = channel.trim();
    if (!trimmed.empty())
      ownedChannels.push_back(trimmed.str());
  }
  if (ownedChannels.empty())
    return;

  llvm::DebugFlag = true;
  llvm::SmallVector<const char *, 8> debugTypes;
  debugTypes.reserve(ownedChannels.size());
  for (const std::string &owned : ownedChannels)
    debugTypes.push_back(owned.c_str());
  llvm::setCurrentDebugTypes(debugTypes.data(), debugTypes.size());
}

LogicalResult configurePassManager(PassManager &pm,
                                   PassTimingData *timingData) {
  pm.enableVerifier(true);
  if (failed(applyPassManagerCLOptions(pm)))
    return failure();
  applyDefaultTimingPassManagerCLOptions(pm);
  if (timingData)
    pm.addInstrumentation(
        std::make_unique<CartsPassInstrumentation>(timingData));
  return success();
}

void registerDialects(DialectRegistry &registry) {
  registry.insert<polygeist::PolygeistDialect, arts::ArtsDialect,
                  arts_rt::ArtsRtDialect, sde::CartsSdeDialect>();
  registerAllPasses();
  /// ARTS pass registration is intentionally selective: lowering-only helpers
  /// are registered here, while staged compiler pipelines wire pass ordering.
  registerDeadCodeElimination();
  registerSdeStorageToArtsDb();
  registerVerifyRawAccessCovered();
  registerSdeAccessesToArtsDeps();
  registerFinalizeSdeToArts();
  registerPartialReductionSplit();
  registerBlockContractionSplit();
  registerEdtSplitForMixedDeps();
  registerWriterOwnerRoute();
  registerRealizeEdtDistribution();
  registerDbDistributedRuntimeInit();
  registerEpochTailContinuation();
  registerVerifyArtsCdag();
  registerDbCommitDistributedDeps();
  registerVerifyArtsObjectsOnly();
  registerArtsRtPasses();
  sde::registerCartsSdePasses();
  registerAllTranslations();
  registerpolygeistPasses();
  func::registerInlinerExtension(registry);
  registerAllDialects(registry);
  registerAllExtensions(registry);
  registerAllFromLLVMIRTranslations(registry);
  registerAllToLLVMIRTranslations(registry);
}

void initializeContext(MLIRContext &context) {
  context.disableMultithreading(true);
  context.getOrLoadDialect<affine::AffineDialect>();
  context.getOrLoadDialect<func::FuncDialect>();
  context.getOrLoadDialect<DLTIDialect>();
  context.getOrLoadDialect<scf::SCFDialect>();
  context.getOrLoadDialect<async::AsyncDialect>();
  context.getOrLoadDialect<LLVM::LLVMDialect>();
  context.getOrLoadDialect<NVVM::NVVMDialect>();
  context.getOrLoadDialect<ROCDL::ROCDLDialect>();
  context.getOrLoadDialect<gpu::GPUDialect>();
  context.getOrLoadDialect<mlir::omp::OpenMPDialect>();
  context.getOrLoadDialect<math::MathDialect>();
  context.getOrLoadDialect<memref::MemRefDialect>();
  context.getOrLoadDialect<linalg::LinalgDialect>();
  context.getOrLoadDialect<bufferization::BufferizationDialect>();
  context.getOrLoadDialect<polygeist::PolygeistDialect>();
  context.getOrLoadDialect<arts::ArtsDialect>();
  context.getOrLoadDialect<arts_rt::ArtsRtDialect>();
  context.getOrLoadDialect<sde::CartsSdeDialect>();
  context.getOrLoadDialect<cf::ControlFlowDialect>();

  /// Register all necessary interfaces for LLVM conversion.
  LLVM::LLVMFunctionType::attachInterface<MemRefInsider>(context);
  LLVM::LLVMArrayType::attachInterface<MemRefInsider>(context);
  LLVM::LLVMPointerType::attachInterface<MemRefInsider>(context);
  LLVM::LLVMStructType::attachInterface<MemRefInsider>(context);
  MemRefType::attachInterface<PtrElementModel<MemRefType>>(context);
  IndexType::attachInterface<PtrElementModel<IndexType>>(context);
  LLVM::LLVMStructType::attachInterface<PtrElementModel<LLVM::LLVMStructType>>(
      context);
  LLVM::LLVMPointerType::attachInterface<
      PtrElementModel<LLVM::LLVMPointerType>>(context);
  LLVM::LLVMArrayType::attachInterface<PtrElementModel<LLVM::LLVMArrayType>>(
      context);
}

bool hasResidualOpenMP(ModuleOp module) {
  bool found = false;
  module.walk([&](Operation *op) {
    if (op->getDialect() && op->getDialect()->getNamespace() == "omp") {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

std::unique_ptr<Pass> createRealizeArtsFunctionPointersPass() {
  return std::make_unique<RealizeArtsFunctionPointersPass>();
}

std::unique_ptr<Pass> createResidualHostOpenMPMemrefCleanupPass() {
  return std::make_unique<ResidualHostOpenMPMemrefCleanupPass>();
}

void foldResidualHostOpenMPMemrefPointerCasts(ModuleOp module) {
  SmallVector<UnrealizedConversionCastOp> casts;
  module.walk([&](UnrealizedConversionCastOp cast) {
    if (cast.getNumOperands() != 1 || cast.getNumResults() != 1)
      return;
    if (!isa<LLVM::LLVMPointerType>(cast.getResult(0).getType()))
      return;
    if (!isa<MemRefType>(cast.getOperand(0).getType()))
      return;
    casts.push_back(cast);
  });

  for (UnrealizedConversionCastOp cast : casts) {
    if (!cast || cast->use_empty())
      continue;
    auto sourceCast =
        cast.getOperand(0).getDefiningOp<UnrealizedConversionCastOp>();
    if (!sourceCast || sourceCast.getNumOperands() != 1 ||
        sourceCast.getNumResults() != 1)
      continue;
    mlir::Value descriptor = sourceCast.getOperand(0);
    if (!isLLVMMemRefDescriptorLike(descriptor.getType()))
      continue;

    OpBuilder builder(cast);
    mlir::Value ptr = LLVM::ExtractValueOp::create(builder, cast.getLoc(),
                                                   descriptor, /*position=*/1);
    if (ptr.getType() != cast.getResult(0).getType())
      ptr = LLVM::AddrSpaceCastOp::create(builder, cast.getLoc(),
                                          cast.getResult(0).getType(), ptr);
    cast.getResult(0).replaceAllUsesWith(ptr);
    cast.erase();
    if (sourceCast->use_empty())
      sourceCast.erase();
  }
}

void ensureRuntimeConfigDataVisibleForValidation(ModuleOp module) {
  std::optional<StringRef> configData = arts::getRuntimeConfigData(module);
  if (!configData || configData->empty())
    return;
  constexpr llvm::StringLiteral kConfigGlobalName = "__carts_embedded_arts_cfg";
  if (module.lookupSymbol<LLVM::GlobalOp>(kConfigGlobalName))
    return;

  OpBuilder builder(module.getContext());
  builder.setInsertionPointToStart(module.getBody());
  auto i8 = mlir::IntegerType::get(module.getContext(), 8);
  auto type = LLVM::LLVMArrayType::get(i8, configData->size() + 1);
  LLVM::GlobalOp::create(builder, module.getLoc(), type, /*isConstant=*/true,
                         LLVM::Linkage::Internal, kConfigGlobalName,
                         builder.getStringAttr(configData->str() + '\0'));
}

void bypassArtsRuntimeForHostOpenMP(llvm::Module &module) {
  llvm::Function *main = module.getFunction("main");
  llvm::Function *mainBody = module.getFunction("mainBody");
  if (!main || !mainBody || main->arg_size() != 2)
    return;
  if (mainBody->arg_size() != 0 && mainBody->arg_size() != 2)
    return;
  if (hasArtsRuntimeCallsOutsideHostWrappers(module))
    return;

  main->deleteBody();
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(module.getContext(), "entry", main);
  llvm::IRBuilder<> builder(entry);
  SmallVector<llvm::Value *, 2> args;
  if (mainBody->arg_size() == 2)
    for (llvm::Argument &arg : main->args())
      args.push_back(&arg);
  llvm::CallInst *result = builder.CreateCall(mainBody, args);
  if (main->getReturnType()->isVoidTy())
    builder.CreateRetVoid();
  else
    builder.CreateRet(result);
}

} // namespace mlir::carts::compile_driver
