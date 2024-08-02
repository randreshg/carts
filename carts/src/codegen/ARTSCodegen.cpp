//===- ARTSCodegen.cpp - Builder for LLVM-IR for ARTS directives ----===//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file implements the ARTSCodegen class, which is used as a
/// convenient way to create LLVM instructions for ARTS directives.
///
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
// #include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"

#include "carts/analysis/graph/EDTEdge.h"
#include "carts/analysis/graph/EDTGraph.h"
#include "carts/codegen/ARTSCodegen.h"
#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSCache.h"
#include "carts/utils/ARTSUtils.h"

#include <cassert>

// DEBUG
#define DEBUG_TYPE "arts-codegen"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

// using namespace llvm;
using namespace arts;
using namespace arts::types;

ARTSCodegen::ARTSCodegen(ARTSCache *Cache)
    : Cache(Cache), M(Cache->getModule()), Builder(Cache->getModule().getContext()) {
  initialize();
}

ARTSCodegen::~ARTSCodegen() {}

void ARTSCodegen::initialize() {
  LLVM_DEBUG(dbgs() << TAG << "Initializing ARTSCodegen\n");
  initializeTypes();
  LLVM_DEBUG(dbgs() << TAG << "ARTSCodegen initialized\n");
}

void ARTSCodegen::finalize() {
  LLVM_DEBUG(dbgs() << TAG << "Finalizing ARTSCodegen\n");
  // Finalize the module with the runtime functions.
  // finalizeModule(M);
  LLVM_DEBUG(dbgs() << TAG << "ARTSCodegen finalized\n");
}

FunctionCallee ARTSCodegen::getOrCreateRuntimeFunction(RuntimeFunction FnID) {
  FunctionType *FnTy = nullptr;
  Function *Fn = nullptr;

  // Try to find the declaration in the module first.
  switch (FnID) {
#define ARTS_RTL(Enum, Str, IsVarArg, ReturnType, ...)                         \
  case Enum:                                                                   \
    FnTy = FunctionType::get(ReturnType, ArrayRef<Type *>{__VA_ARGS__},        \
                             IsVarArg);                                        \
    Fn = M.getFunction(Str);                                                   \
    break;
#include "carts/codegen/ARTSKinds.def"
  }

  if (!Fn) {
    // Create a new declaration if we need one.
    switch (FnID) {
#define ARTS_RTL(Enum, Str, ...)                                               \
  case Enum:                                                                   \
    Fn = Function::Create(FnTy, GlobalValue::ExternalLinkage, Str, M);         \
    break;
#include "carts/codegen/ARTSKinds.def"
    }

    LLVM_DEBUG(dbgs() << TAG << "Created ARTS runtime function "
                      << Fn->getName() << " with type "
                      << *Fn->getFunctionType() << "\n");
    // addAttributes(FnID, *Fn);

  } else {
    LLVM_DEBUG(dbgs() << TAG << "Found ARTS runtime function " << Fn->getName()
                      << " with type " << *Fn->getFunctionType() << "\n");
  }

  assert(Fn && "Failed to create ARTS runtime function");

  return {FnTy, Fn};
}

Function *ARTSCodegen::getOrCreateRuntimeFunctionPtr(RuntimeFunction FnID) {
  FunctionCallee RTLFn = getOrCreateRuntimeFunction(FnID);
  auto *Fn = dyn_cast<llvm::Function>(RTLFn.getCallee());
  assert(Fn && "Failed to create ARTS runtime function pointer");
  return Fn;
}

void ARTSCodegen::initializeEDT(EDT &E) {
  // LLVM_DEBUG(dbgs() << TAG << "Initializing EDT\n");
  // auto *EntryBB = E.getEntry();
  // EntryBB->setName("edt.entry");
  // auto *ExitBB = E.getExit();
  // ExitBB->setName("edt.exit");
  // /// Change EDT Task body function signature
  // auto *EdtBody = E.getTaskBody();
  // (EdtBody->arg_begin())->setName("paramc");
  // (EdtBody->arg_begin() + 1)->setName("paramv");
  // (EdtBody->arg_begin() + 2)->setName("depc");
  // (EdtBody->arg_begin() + 3)->setName("depv");
}

void ARTSCodegen::handleEDT(EDT &E) {
  // LLVM_DEBUG(dbgs() << TAG << "Handling EDT\n");
  // initializeEDT(E);
  // insertEDTEntry(E);
  // insertEDTCall(E);
}

Function *ARTSCodegen::getOrCreateEDTFunction(EDT &E) {
  if (EDTFunctions.find(&E) != EDTFunctions.end())
    return EDTFunctions[&E];

  LLVM_DEBUG(dbgs() << TAG << "Creating function for EDT #" << E.getID()
                    << "\n");
  Function *OldFn = E.getFn();
  auto FnName = E.getName();
  Function *EDTFn = Function::Create(EdtFunction, GlobalValue::InternalLinkage,
                                     E.getName(), M);
  (EDTFn->arg_begin())->setName("paramc");
  (EDTFn->arg_begin() + 1)->setName("paramv");
  (EDTFn->arg_begin() + 2)->setName("depc");
  (EDTFn->arg_begin() + 3)->setName("depv");
  E.setNewFn(EDTFn);
  EDTFunctions[&E] = EDTFn;
  /// Redirect Entry BB
  BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "entry", EDTFn);
  BasicBlock *OldEntryBB = &OldFn->getEntryBlock();
  OldEntryBB->setName("edt.body");
  redirectTo(EntryBB, OldEntryBB);

  /// Redirect Exit BB
  BasicBlock *ExitBB = BasicBlock::Create(M.getContext(), "exit", EDTFn);
  Builder.SetInsertPoint(ExitBB);
  Builder.CreateRetVoid();
  redirectExitsTo(OldFn, ExitBB);

  /// Splice the body of the old function right into the new function
  EDTFn->splice(++EDTFn->begin(), E.getFn());
  return EDTFn;
}

void ARTSCodegen::setIPInEDTEntry(EDTFunction &EDTFn) {
  BasicBlock &EntryBB = EDTFn.getEntryBlock();
  Instruction *EntryBBTerm = EntryBB.getTerminator();
  if (EntryBBTerm)
    Builder.SetInsertPoint(EntryBBTerm);
  else
    Builder.SetInsertPoint(&EntryBB);
}

Value *ARTSCodegen::getOrCreateEDTGuid(EDT &E) {
  /// Check if the EDT already has a GUID
  if (E.getGuidAddress())
    return E.getGuidAddress();

  LLVM_DEBUG(dbgs() << TAG << "Reserving GUID for EDT #" << E.getID() << "\n");
  /// EDTs are allocated in the parent EDT function
  EDT *EDTParent = E.getParent();
  if (!EDTParent) {
    LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                      << " doesn't have a parent EDT\n");
    return nullptr;
  }

  Function *ParentNewFn = getOrCreateEDTFunction(*EDTParent);
  BasicBlock &ParentNewEntryBB = ParentNewFn->getEntryBlock();
  auto OldInsertPoint = Builder.saveIP();

  if (ParentNewEntryBB.getTerminator())
    Builder.SetInsertPoint(ParentNewEntryBB.getTerminator());
  else
    Builder.SetInsertPoint(&ParentNewEntryBB);

  /// Create the call to reserve the GUID
  ConstantInt *ARTS_EDT_Enum =
      ConstantInt::get(Builder.getContext(), APInt(32, 1));
  Value *Args[] = {ARTS_EDT_Enum,
                   Builder.CreateIntCast(ConstantInt::get(Int32, E.getNode()),
                                         Int32, false)};
  CallInst *ReserveGuidCall = Builder.CreateCall(
      getOrCreateRuntimeFunctionPtr(ARTSRTL_artsReserveGuidRoute), Args);
  /// Create allocation of the GUID
  auto *GuidAddress =
      Builder.CreateAlloca(Int32Ptr, nullptr, E.getName() + "_guid.addr");
  Builder.CreateStore(ReserveGuidCall, GuidAddress);
  E.setGuidAddress(GuidAddress);
  EDTGuids.insert(GuidAddress);

  /// Restore the insertion point
  Builder.restoreIP(OldInsertPoint);
  return GuidAddress;
}

void ARTSCodegen::insertEDTEntry(EDT &E) {
  // EDT &E = *EDTNode.getEDT();
  // LLVM_DEBUG(dbgs() << TAG << "Inserting Entry for EDT #" << E.getID() << "\n");
  // auto OldInsertPoint = Builder.saveIP();
  // /// Set the insertion point to the entry block of the EDT function
  // Function *EDTFn = getOrCreateEDTFunction(E);
  // setIPInEDTEntry(*EDTFn);

  // /// The input parameters and Guids always come from the parent
  // EDT *Parent = E.getParent();
  // EDTGraphDataEdge *InputDataEdges =
  //     dyn_cast<EDTGraphDataEdge>(Graph.getEdge(Parent, &E));
  // if (!InputDataEdges) {
  //   LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
  //                     << " doesn't have input data edges from the parent\n");
  //   return;
  // }

  // /// Get parameters
  // auto &InputParameters = InputDataEdges->getParameters();
  // for (auto Param : InputParameters) {
  //   Value *ParamValue = Param.getValue();
  //   if (ParamValue->getType()->isPointerTy()) {
  //     /// Load the value from the pointer
  //     ParamValue = Builder.CreateLoad(
  //         ParamValue->getType()->getPointerElementType(), ParamValue);
  //   }
  //   /// Store the value in the EDT function
  //   auto *ParamAlloca = EDTFn->getArg(1 + Param.getIndex());
  //   Builder.CreateStore(ParamValue, ParamAlloca);
  // }

  // auto InputParams
  //     /// Get input data edges
  //     auto DataEdges = Graph.getIncomingEdges(&E);

  // /// Parameters
  // auto InputParams = DataEdges.getParams();
  // ///
  // DenseMap<Value *, Value *> RewiringMap;
  // EDTEnvironment &EDTDataEnv = E.getDataEnv();
  // /// Insert ParamV
  // auto ParamVArg = EDTFn->arg_begin() + 1;
  // LLVM_DEBUG(dbgs() << " - Inserting ParamV\n");
  // for (auto ParamV : enumerate(EDTDataEnv.ParamV)) {
  //   unsigned Index = ParamV.index();
  //   Value *OriginalVal = ParamV.value();
  //   LLVM_DEBUG(dbgs() << "   - ParamV[" << Index << "]: " << *OriginalVal
  //                     << "\n");
  //   Twine ParamVName = (OriginalVal->getName() == "")
  //                          ? ("paramv." + std::to_string(Index))
  //                          : ("paramv." + OriginalVal->getName());
  //   Value *ParamVElemPtr =
  //       Builder.CreateConstInBoundsGEP1_64(Int64, ParamVArg, Index, ParamVName);
  //   /// Load the value from the array
  //   Value *LoadedVal = Builder.CreateLoad(Int64, ParamVElemPtr);
  //   /// Cast the value to the original type
  //   Value *CastedVal = LoadedVal;
  //   Type *OriginalType = OriginalVal->getType();
  //   switch (OriginalType->getTypeID()) {
  //   /// Integer
  //   case llvm::Type::IntegerTyID: {
  //     LLVM_DEBUG(dbgs() << "     - Value is an Integer\n");
  //     if (OriginalType != Int64)
  //       CastedVal = Builder.CreateTrunc(LoadedVal, OriginalType);
  //     LLVM_DEBUG(dbgs() << "     - Casted Value: " << *CastedVal << "\n");
  //   } break;
  //   /// Float
  //   case llvm::Type::FloatTyID: {
  //     llvm_unreachable("Float type not supported yet");
  //   } break;
  //   /// Pointer
  //   case llvm::Type::PointerTyID: {
  //     llvm_unreachable("Pointer type not supported yet");
  //   } break;
  //   default:
  //     llvm_unreachable("Type not supported yet");
  //     break;
  //   }
  //   /// Add the value to the rewiring map
  //   RewiringMap[OriginalVal] = CastedVal;
  // }

  // /// Insert DepV
  // LLVM_DEBUG(dbgs() << " - Inserting DepV\n");
  // auto DepVArg = EDTFn->arg_begin() + 3;
  // for (auto En : enumerate(EDTDataEnv.DepV)) {
  //   unsigned Index = En.index();
  //   Value *OriginalVal = En.value();
  //   auto DepVName = (OriginalVal->getName() == "")
  //                       ? ("depv." + std::to_string(Index))
  //                       : ("depv." + std::string(OriginalVal->getName()));
  //   LLVM_DEBUG(dbgs() << "   - DepV[" << Index << "]: " << *OriginalVal
  //                     << "\n");
  //   Value *DepVArrayElemPtr =
  //       Builder.CreateConstInBoundsGEP2_32(EdtDep, DepVArg, Index, 2, DepVName);
  //   Value *CastedVal =
  //       Builder.CreateBitCast(DepVArrayElemPtr, OriginalVal->getType());
  //   /// Add the value to the rewiring map
  //   RewiringMap[OriginalVal] = CastedVal;
  // }

  // // redirectTo(EntryBB, E.getExit());
  // utils::rewireValues(RewiringMap);
  // Builder.restoreIP(OldInsertPoint);
}

CallInst *ARTSCodegen::insertEDTCall(EDT &E) {
  LLVM_DEBUG(dbgs() << TAG << "Inserting Call for EDT #" << E.getID() << "\n");
  // auto *EdtBody = E.getTaskBody();
  // auto &EdtEnv = E.getEnv();
  // const auto EdtName = E.getName();
  // /// Guid
  // reserveEDTGuid(E);
  // /// ParamC
  // AllocaInst *ParamC =
  //     Builder.CreateAlloca(Int32, nullptr, EdtName + "_paramc");
  // Builder.CreateStore(ConstantInt::get(Int32, EdtEnv.getParamC()), ParamC);
  // auto *LoadedParamC = Builder.CreateLoad(Int32, ParamC);
  // /// ParamV
  // AllocaInst *ParamV =
  //     Builder.CreateAlloca(Int64, LoadedParamC, EdtName + "_paramv");
  // for (auto En : enumerate(EdtEnv.ParamV)) {
  //   unsigned Index = En.index();
  //   Value *Val = En.value();
  //   auto ParamVName = (Val->getName() == "")
  //                         ? (EdtName + "_paramv." + std::to_string(Index))
  //                         : (EdtName + "_paramv." + Val->getName());
  //   /// Create the GEP to store the value in the ParamV array
  //   Value *ParamVElemPtr =
  //       Builder.CreateConstInBoundsGEP1_64(Int64, ParamV, Index, ParamVName);
  //   /// Cast the value to int64
  //   Value *CastedVal = nullptr;
  //   auto *ValType = Val->getType();
  //   switch (ValType->getTypeID()) {
  //   /// Integer
  //   case llvm::Type::IntegerTyID: {
  //     if (ValType != Int64)
  //       CastedVal = Builder.CreateSExtOrTrunc(Val, Int64);
  //     else
  //       CastedVal = Val;
  //   } break;
  //   /// Default
  //   default:
  //     assert(false && "Type not supported yet");
  //     break;
  //   }
  //   Builder.CreateStore(CastedVal, ParamVElemPtr);
  // }
  // /// Depc
  // AllocaInst *DepC = Builder.CreateAlloca(Int32, nullptr);
  // Builder.CreateStore(ConstantInt::get(Int32, EdtEnv.getDepC()), DepC);
  // Value *Args[] = {Builder.CreateBitCast(EdtBody, EdtFunctionPtr),
  //                  Builder.CreateBitCast(E.getGuidAddr(), Int32Ptr),
  //                  LoadedParamC, ParamV,
  //                  //  Builder.CreateBitCast(ParamV, Int64Ptr),
  //                  Builder.CreateLoad(Int32, DepC)};
  // Function *F = getOrCreateRuntimeFunctionPtr(ARTSRTL_artsEdtCreateWithGuid);
  // return Builder.CreateCall(F, Args);
}

void ARTSCodegen::signalEDTGuid(EDT &From, EDT &To, Value *Signal) {
  Value *FromGuidInt = Builder.CreatePtrToInt(From.getGuidAddress(), Int32);
  Value *ToGuidInt = Builder.CreatePtrToInt(To.getGuidAddress(), Int32);
  Value *Args[] = {FromGuidInt, ToGuidInt};

  // void artsSignalEdtValue(artsGuid_t edtGuid, uint32_t slot, uint64_t data);
  // void artsSignalEdtPtr(artsGuid_t edtGuid, uint32_t slot, void * ptr,
  // unsigned int size);

  // Function *F = getOrCreateRuntimeFunctionPtr(ARTSRTL_artsSignalEdt);
  // Builder.CreateCall(F, Args);
}

/// ---------------------------- Utils ---------------------------- ///
void ARTSCodegen::redirectTo(BasicBlock *Source, BasicBlock *Target) {
  if (Instruction *Term = Source->getTerminator()) {
    auto *Br = cast<BranchInst>(Term);
    assert(!Br->isConditional() &&
           "BB's terminator must be an unconditional branch (or degenerate)");
    BasicBlock *Succ = Br->getSuccessor(0);
    Succ->removePredecessor(Source, /*KeepOneInputPHIs=*/true);
    Br->setSuccessor(0, Target);
    return;
  }
  /// Create unconditional branch
  BranchInst::Create(Target, Source);
}

void ARTSCodegen::redirectExitsTo(Function *Source, BasicBlock *Target) {
  for (BasicBlock &BB : *Source) {
    Instruction *Terminator = BB.getTerminator();
    if (Terminator->getNumSuccessors() > 0)
      continue;
    /// Remove the terminator and redirect the exit and
    Terminator->eraseFromParent();
    redirectTo(&BB, Target);
  }
}

// void ARTSCodegen::redirectTo(Function *Source, BasicBlock *Target) {
//   for (BasicBlock &SourceBB : *Source)
//     redirectTo(&SourceBB, Target);
// }

void ARTSCodegen::redirectEntryAndExit(EDT &E, BasicBlock *OriginalEntry) {
  /// Redirect Entry
  // auto *ClonedEntry = E.getCloneOfOriginalBasicBlock(OriginalEntry);
  // ClonedEntry->setName("edt.body");
  // E.setBody(ClonedEntry);
  // redirectTo(E.getEntry(), ClonedEntry);
  // /// Redirect Exit
  // auto OriginalParent = OriginalEntry->getParent();
  // for (auto &BB : *OriginalParent) {
  //   auto *Terminator = BB.getTerminator();
  //   if (Terminator->getNumSuccessors() == 0) {
  //     auto *ClonedBB = E.getCloneOfOriginalBasicBlock(&BB);
  //     ClonedBB->getTerminator()->eraseFromParent();
  //     redirectTo(ClonedBB, E.getExit());
  //   }
  // }
}

void ARTSCodegen::setInsertPoint(BasicBlock *BB) { Builder.SetInsertPoint(BB); }

void ARTSCodegen::setInsertPoint(Instruction *I) { Builder.SetInsertPoint(I); }

/// ---------------------------- Private ---------------------------- ///
void ARTSCodegen::initializeTypes() {
  LLVMContext &Ctx = M.getContext();
  StructType *T;
#define ARTS_TYPE(VarName, InitValue) VarName = InitValue;
#define ARTS_ARRAY_TYPE(VarName, ElemTy, ArraySize)                            \
  VarName##Ty = ArrayType::get(ElemTy, ArraySize);                             \
  VarName##PtrTy = PointerType::getUnqual(VarName##Ty);
#define ARTS_FUNCTION_TYPE(VarName, IsVarArg, ReturnType, ...)                 \
  VarName = FunctionType::get(ReturnType, {__VA_ARGS__}, IsVarArg);            \
  VarName##Ptr = PointerType::getUnqual(VarName);
#define ARTS_STRUCT_TYPE(VarName, StructName, Packed, ...)                     \
  T = StructType::getTypeByName(Ctx, StructName);                              \
  if (!T)                                                                      \
    T = StructType::create(Ctx, {__VA_ARGS__}, StructName, Packed);            \
  VarName = T;                                                                 \
  VarName##Ptr = PointerType::getUnqual(T);
#include "carts/codegen/ARTSKinds.def"
}