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
#include <cstdint>
#include <sys/types.h>

// DEBUG
#define DEBUG_TYPE "arts-codegen"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

// using namespace llvm;
using namespace arts;
using namespace arts::types;

/// ---------------------------- EDTCodegen ---------------------------- ///
EDTCodegen::EDTCodegen(EDT *E) : E(E) {}
EDTCodegen::EDTCodegen(EDT *E, Function *Fn) : E(E), Fn(Fn) {}
Function *EDTCodegen::getFn() { return Fn; }
CallBase *EDTCodegen::getCB() { return CB; }
Value *EDTCodegen::getGuidAddress() { return GuidAddress; }
void EDTCodegen::setFn(Function *Fn) { this->Fn = Fn; }
void EDTCodegen::setCB(CallBase *CB) { this->CB = CB; }
void EDTCodegen::setGuidAddress(Value *V) { this->GuidAddress = V; }

bool EDTCodegen::hasParameter(int32_t Slot) {
  return Parameters.find(Slot) != Parameters.end();
}

bool EDTCodegen::hasDependency(int32_t Slot) {
  return Dependencies.find(Slot) != Dependencies.end();
}

void EDTCodegen::insertParameter(int32_t Slot, Value *V) {
  if (hasParameter(Slot))
    Parameters[Slot] = V;
}

void EDTCodegen::insertDependency(int32_t Slot, Value *V) {
  if (hasDependency(Slot))
    Dependencies[Slot] = V;
}

/// ---------------------------- ARTSCodegen ---------------------------- ///
ARTSCodegen::ARTSCodegen(ARTSCache *Cache)
    : Cache(Cache), M(Cache->getModule()),
      Builder(Cache->getModule().getContext()) {
  initialize();
}

ARTSCodegen::~ARTSCodegen() {
  for (auto &ECG : EDTs)
    delete ECG.second;
  EDTs.clear();
}

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
  EDTCodegen *ECG = getOrCreateEDT(E);
  if (ECG->getFn())
    return ECG->getFn();

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
  ECG->setFn(EDTFn);
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
  EDTCodegen *ECG = getOrCreateEDT(E);
  if (ECG->getGuidAddress())
    return ECG->getGuidAddress();

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
  ECG->setGuidAddress(GuidAddress);

  /// Restore the insertion point
  Builder.restoreIP(OldInsertPoint);
  return GuidAddress;
}

void ARTSCodegen::insertEDTEntry(EDT &E) {
  // EDT &E = *EDTNode.getEDT();
  LLVM_DEBUG(dbgs() << TAG << "Inserting Entry for EDT #" << E.getID() << "\n");
  auto OldInsertPoint = Builder.saveIP();
  /// Set the insertion point to the entry block of the EDT function
  Function *EDTFn = getOrCreateEDTFunction(E);
  setIPInEDTEntry(*EDTFn);

  DenseMap<Value *, Value *> RewiringMap;
  /// Get the information we know about the Module
  EDTGraph &Graph = Cache->getGraph();
  EDTCodegen *ECG = getOrCreateEDT(E);

  /// Parameters
  auto handleParameters = [&]() -> void {
    LLVM_DEBUG(dbgs() << " - Inserting ParamV\n");
    /// The input parameters and Guids always come from the parent EDT
    EDT *Parent = E.getParent();
    if (!Parent) {
      LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                        << " doesn't have a parent EDT\n");
      return;
    }
    EDTGraphDataEdge *InputParentDataEdges =
        dyn_cast<EDTGraphDataEdge>(Graph.getEdge(Parent, &E));
    if (!InputParentDataEdges ||
        InputParentDataEdges->getParameters().empty()) {
      LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                        << " doesn't have input data edges from the parent\n");
      return;
    }

    auto &InputParameters = InputParentDataEdges->getParameters();
    auto ParamVArg = EDTFn->arg_begin() + 1;
    for (auto Param : enumerate(InputParameters)) {
      uint32_t Index = Param.index();
      Value *OriginalVal = Cache->getEDTArg(&E, Param.value());
      LLVM_DEBUG(dbgs() << "   - ParamV[" << Index << "]: " << *OriginalVal
                        << "\n");
      Twine ParamVName = (OriginalVal->getName() == "")
                             ? ("paramv." + std::to_string(Index))
                             : ("paramv." + OriginalVal->getName());
      Value *ParamVElemPtr = Builder.CreateConstInBoundsGEP1_64(
          Int64, ParamVArg, Index, ParamVName);
      /// Load the value from the array
      Value *LoadedVal = Builder.CreateLoad(Int64, ParamVElemPtr);
      /// Cast the value to the original type
      Value *CastedVal = LoadedVal;
      Type *OriginalType = OriginalVal->getType();
      switch (OriginalType->getTypeID()) {
      /// Integer
      case llvm::Type::IntegerTyID: {
        LLVM_DEBUG(dbgs() << "     - Value is an Integer\n");
        if (OriginalType != Int64)
          CastedVal = Builder.CreateTrunc(LoadedVal, OriginalType);
        LLVM_DEBUG(dbgs() << "     - Casted Value: " << *CastedVal << "\n");
      } break;
      /// Float
      case llvm::Type::FloatTyID: {
        llvm_unreachable("Float type not supported yet");
      } break;
      /// Pointer
      case llvm::Type::PointerTyID: {
        llvm_unreachable("Pointer type not supported yet");
      } break;
      default:
        llvm_unreachable("Type not supported yet");
        break;
      }

      /// Add the value to the rewiring map
      RewiringMap[OriginalVal] = CastedVal;
      ECG->insertParameter(Index, CastedVal);
    }
  };

  /// Dependencies
  auto handleDependencies = [&]() -> void {
    LLVM_DEBUG(dbgs() << " - Inserting DepV\n");
    /// The input parameters and Guids always come from the parent EDT
    auto InputEdges = Graph.getIncomingEdges(&E);
    if (InputEdges.empty()) {
      LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                        << " doesn't have input edges\n");
      return;
    }

    for (auto *Edge : InputEdges) {
      auto *DataEdge = dyn_cast<EDTGraphDataEdge>(Edge);
      if (!DataEdge)
        continue;

      auto &DataBlocks = DataEdge->getDataBlocks();
      auto DepVArg = EDTFn->arg_begin() + 3;
      for (auto Dep : enumerate(DataBlocks)) {
        EDTDataBlock *DB = Dep.value();
        uint32_t Index = DB->getToSlot();
        /// If the DB slot was already filled, skip it
        if (ECG->hasDependency(Index))
          continue;

        /// Otherwise create it
        Value *OriginalVal = E.getDepArg(Index);
        LLVM_DEBUG(dbgs() << "   - DepV[" << Index << "]: " << *OriginalVal
                          << "\n");
        Twine DepVName = (OriginalVal->getName() == "")
                             ? ("depv." + std::to_string(Index))
                             : ("depv." + OriginalVal->getName());
        Value *DepVElemPtr = Builder.CreateConstInBoundsGEP2_32(
            EdtDep, DepVArg, Index, 2, DepVName);
        Value *CastedVal =
            Builder.CreateBitCast(DepVElemPtr, OriginalVal->getType());

        /// Add the value to the rewiring map
        RewiringMap[OriginalVal] = CastedVal;
        ECG->insertDependency(DB->getSlot(), CastedVal);
      }
    }
  };

  handleParameters();
  handleDependencies();
  utils::rewireValues(RewiringMap);
  Builder.restoreIP(OldInsertPoint);
}

void ARTSCodegen::insertEDTCall(EDT &E) {
  LLVM_DEBUG(dbgs() << TAG << "Inserting Call for EDT #" << E.getID() << "\n");
  EDT *Parent = E.getParent();
  if (!Parent) {
    LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                      << " doesn't have a parent EDT\n");
    return;
  }

  EDTGraph &Graph = Cache->getGraph();
  EDTGraphDataEdge *InputParentDataEdges =
      dyn_cast<EDTGraphDataEdge>(Graph.getEdge(Parent, &E));
  if (!InputParentDataEdges) {
    LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                      << " doesn't have input data edges from the parent\n");
    return;
  }

  auto EDTName = E.getName();
  /// Set insertion point to the EDT Call
  auto OldInsertPoint = Builder.saveIP();
  Builder.SetInsertPoint(E.getCall());

  /// ParamC
  uint32_t ParamSize = InputParentDataEdges->getParameters().size();
  AllocaInst *ParamC =
      Builder.CreateAlloca(Int32, nullptr, EDTName + "_paramc");
  Builder.CreateStore(ConstantInt::get(Int32, ParamSize), ParamC);
  auto *LoadedParamC = Builder.CreateLoad(Int32, ParamC);

  /// ParamV
  AllocaInst *ParamV =
      Builder.CreateAlloca(Int64, LoadedParamC, EDTName + "_paramv");
  auto &InputParameters = InputParentDataEdges->getParameters();
  for (auto Param : enumerate(InputParameters)) {
    LLVM_DEBUG(dbgs() << " - ParamV[" << Param.index() << "]: "
                      << *Param.value() << "\n");
    unsigned Index = Param.index();
    Value *Val = Param.value();
    auto ParamVName = EDTName + "_paramv." + std::to_string(Index);
    /// Create the GEP to store the value in the ParamV array
    Value *ParamVElemPtr =
        Builder.CreateConstInBoundsGEP1_64(Int64, ParamV, Index, ParamVName);
    /// Cast the value to int64
    Value *CastedVal = nullptr;
    auto *ValType = Val->getType();
    switch (ValType->getTypeID()) {
    /// Integer
    case llvm::Type::IntegerTyID: {
      if (ValType != Int64)
        CastedVal = Builder.CreateSExtOrTrunc(Val, Int64);
      else
        CastedVal = Val;
    } break;
    /// Default
    default:
      assert(false && "Type not supported yet");
      break;
    }
    Builder.CreateStore(CastedVal, ParamVElemPtr);
  }

  /// DepC
  EDTCodegen *ECG = getOrCreateEDT(E);
  EDTEnvironment &EDTEnv = E.getDataEnv();
  AllocaInst *DepC = Builder.CreateAlloca(Int32, nullptr);
  Builder.CreateStore(ConstantInt::get(Int32, EDTEnv.getDepC()), DepC);
  Value *Args[] = {Builder.CreateBitCast(ECG->getFn(), EdtFunctionPtr),
                   Builder.CreateBitCast(ECG->getGuidAddress(), Int32Ptr),
                   LoadedParamC, ParamV,
                   //  Builder.CreateBitCast(ParamV, Int64Ptr),
                   Builder.CreateLoad(Int32, DepC)};

  /// Create EDT
  Function *F = getOrCreateRuntimeFunctionPtr(ARTSRTL_artsEdtCreateWithGuid);
  ECG->setCB(Builder.CreateCall(F, Args));
  utils::removeValue(E.getCall());

  /// Restore the insertion point
  Builder.restoreIP(OldInsertPoint);
}

void ARTSCodegen::signalEDTGuid(EDT &From, EDT &To, Value *Signal) {
  // Value *FromGuidInt = Builder.CreatePtrToInt(From.getGuidAddress(), Int32);
  // Value *ToGuidInt = Builder.CreatePtrToInt(To.getGuidAddress(), Int32);
  // Value *Args[] = {FromGuidInt, ToGuidInt};

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
EDTCodegen *ARTSCodegen::getOrCreateEDT(EDT &E) {
  if (EDTs.find(&E) != EDTs.end())
    return EDTs[&E];

  LLVM_DEBUG(dbgs() << TAG << "Creating codegen for EDT #" << E.getID()
                    << "\n");
  EDTCodegen *ECG = new EDTCodegen(&E);
  EDTs[&E] = ECG;
  return ECG;
}

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