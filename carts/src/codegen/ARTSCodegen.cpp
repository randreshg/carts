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
#include <string>
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
BasicBlock *EDTCodegen::getEntry() { return Entry; }
BasicBlock *EDTCodegen::getExit() { return Exit; }
void EDTCodegen::setEntry(BasicBlock *BB) { Entry = BB; }
void EDTCodegen::setExit(BasicBlock *BB) { Exit = BB; }
bool EDTCodegen::hasParameter(int32_t Slot) {
  return Parameters.find(Slot) != Parameters.end();
}

bool EDTCodegen::hasDependency(int32_t Slot) {
  return Dependencies.find(Slot) != Dependencies.end();
}

bool EDTCodegen::hasGuid(EDT *E) { return Guids.find(E) != Guids.end(); }

void EDTCodegen::insertParameter(int32_t Slot, Value *V) {
  if (!hasParameter(Slot))
    Parameters[Slot] = V;
}

void EDTCodegen::insertDependency(int32_t Slot, Value *V) {
  if (!hasDependency(Slot))
    Dependencies[Slot] = V;
}

void EDTCodegen::insertGuid(EDT *E, Value *V) {
  if (!hasGuid(E))
    Guids[E] = V;
}

Value *EDTCodegen::getParameter(int32_t Slot) {
  if (hasParameter(Slot))
    return Parameters[Slot];
  return nullptr;
}

Value *EDTCodegen::getDependency(int32_t Slot) {
  if (hasDependency(Slot))
    return Dependencies[Slot];
  return nullptr;
}

Value *EDTCodegen::getGuid(EDT *E) {
  if (hasGuid(E))
    return Guids[E];
  return nullptr;
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

    // LLVM_DEBUG(dbgs() << TAG << "Created ARTS runtime function "
    //                   << Fn->getName() << " with type "
    //                   << *Fn->getFunctionType() << "\n");
    // addAttributes(FnID, *Fn);

  } else {
    // LLVM_DEBUG(dbgs() << TAG << "Found ARTS runtime function " <<
    // Fn->getName()
    //                   << " with type " << *Fn->getFunctionType() << "\n");
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

Function *ARTSCodegen::getOrCreateEDTFunction(EDT &E) {
  EDTCodegen *ECG = getOrCreateEDTCodegen(E);
  if (ECG->getFn())
    return ECG->getFn();

  LLVM_DEBUG(dbgs() << TAG << "Creating function for EDT #" << E.getID()
                    << "\n");
  Function *OldFn = E.getFn();
  auto FnName = E.getName();
  Function *EDTFn = Function::Create(EdtFunction, GlobalValue::InternalLinkage,
                                     OldFn->getAddressSpace(), E.getName(), &M);
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

  /// Set new entry and exit blocks
  ECG->setEntry(EntryBB);
  ECG->setExit(ExitBB);
  return EDTFn;
}

Value *ARTSCodegen::getOrCreateEDTGuid(string EDTName, uint32_t EDTNode) {
  /// Create the call to reserve the GUID
  ConstantInt *ARTS_EDT_Enum =
      ConstantInt::get(Builder.getContext(), APInt(32, 1));
  Value *Args[] = {
      ARTS_EDT_Enum,
      Builder.CreateIntCast(ConstantInt::get(Int32, EDTNode), Int32, false)};
  CallInst *ReserveGuidCall = Builder.CreateCall(
      getOrCreateRuntimeFunctionPtr(ARTSRTL_artsReserveGuidRoute), Args);
  /// Create allocation of the GUID
  auto *GuidAddress =
      Builder.CreateAlloca(Int32Ptr, nullptr, EDTName + "_guid.addr");
  Builder.CreateStore(ReserveGuidCall, GuidAddress);
  return GuidAddress;
}

Value *ARTSCodegen::getOrCreateEDTGuid(EDT &E, BasicBlock *InsertionBB) {
  /// Check if the EDT already has a GUID
  EDTCodegen *ECG = getOrCreateEDTCodegen(E);
  if (ECG->getGuidAddress())
    return ECG->getGuidAddress();

  LLVM_DEBUG(dbgs() << TAG << "Reserving GUID for EDT #" << E.getID() << "\n");
  EDT *EDTParent = E.getParent();
  if (!InsertionBB) {
    /// If no insertion BB provided, try to get the parent EDT
    if (!EDTParent) {
      LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                        << " doesn't have a parent EDT\n");
      return nullptr;
    }
    /// If it has parent EDT set insertion BB to the parent EDT entry block
    Function *ParentNewFn = getOrCreateEDTFunction(*EDTParent);
    InsertionBB = &ParentNewFn->getEntryBlock();
  }

  auto OldInsertPoint = Builder.saveIP();
  if (InsertionBB->getTerminator())
    Builder.SetInsertPoint(InsertionBB->getTerminator());
  else
    Builder.SetInsertPoint(InsertionBB);
  /// Create the GUID
  Value *GuidAddress = getOrCreateEDTGuid(E.getName(), E.getNode());
  ECG->setGuidAddress(GuidAddress);
  /// If parent EDT is not null, add the ChildEDTGuid Address to the set of
  /// known GUIDs
  if (EDTParent) {
    EDTCodegen *ParentECG = getOrCreateEDTCodegen(*EDTParent);
    ParentECG->insertGuid(&E, GuidAddress);
  }
  /// Restore the insertion point
  Builder.restoreIP(OldInsertPoint);
  return GuidAddress;
}

void ARTSCodegen::insertEDTEntry(EDT &E) {
  // EDT &E = *EDTNode.getEDT();
  LLVM_DEBUG(dbgs() << TAG << "Inserting Call for  for EDT #" << E.getID()
                    << "\n");
  auto OldInsertPoint = Builder.saveIP();
  /// Set the insertion point to the entry block of the EDT function
  Function *EDTFn = getOrCreateEDTFunction(E);
  setInsertionPointInBB(EDTFn->getEntryBlock());

  DenseMap<Value *, Value *> RewiringMap;
  /// Get the information we know about the Module
  EDTGraph &Graph = Cache->getGraph();
  EDTCodegen *ECG = getOrCreateEDTCodegen(E);

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
    if (!InputParentDataEdges) {
      LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                        << " doesn't have input data edges from the parent\n");
      return;
    }

    /// Parameter function argument
    auto ParamVArg = EDTFn->arg_begin() + 1;

    /// Input parameters
    auto &InputParameters = InputParentDataEdges->getParameters();
    for (auto Param : enumerate(InputParameters)) {
      uint32_t Index = Param.index();
      Value *OriginalVal = Cache->getEDTArg(&E, Param.value());
      LLVM_DEBUG(dbgs() << "   - ParamV[" << Index << "]: " << *OriginalVal
                        << "\n");
      auto ParamVName = (OriginalVal->getName() == "")
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
      insertRewiredValue(OriginalVal, CastedVal);
      ECG->insertParameter(Index, CastedVal);
    }

    /// Input Guids
    const auto InitialIndex = InputParameters.size();
    auto &InputGuids = InputParentDataEdges->getGuids();
    for (auto Guid : enumerate(InputGuids)) {
      uint32_t Index = Guid.index() + InitialIndex;
      EDT *Edt = Guid.value();
      LLVM_DEBUG(dbgs() << "   - ParamVGuid[" << Index << "]: EDT"
                        << Edt->getID() << "\n");
      auto ParamVName = "paramv.guid.edt_" + std::to_string(Edt->getID());
      Value *ParamVElemPtr = Builder.CreateConstInBoundsGEP1_64(
          Int64, ParamVArg, Index, ParamVName);
      /// Load the value from the array
      Value *LoadedVal = Builder.CreateLoad(Int64, ParamVElemPtr);
      /// Cast the value to the original type
      Value *CastedVal = Builder.CreateIntToPtr(LoadedVal, Int32Ptr);
      ECG->insertGuid(Edt, CastedVal);
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
        Value *OriginalVal = E.getDepArg(Index);
        LLVM_DEBUG(dbgs() << "   - DepV[" << Index << "]: " << *OriginalVal
                          << "\n");
        /// If the DB slot was already filled, skip it
        if (ECG->hasDependency(Index)) {
          LLVM_DEBUG(dbgs() << "     - DepV[" << Index
                            << "] already filled, skipping\n");
          continue;
        }

        Twine DepVName = (OriginalVal->getName() == "")
                             ? ("depv." + std::to_string(Index))
                             : ("depv." + OriginalVal->getName());
        Value *DepVElemPtr = Builder.CreateConstInBoundsGEP2_32(
            EdtDep, DepVArg, Index, 2, DepVName);
        Value *CastedVal =
            Builder.CreateBitCast(DepVElemPtr, OriginalVal->getType());

        /// Add the value to the rewiring map
        RewiringMap[OriginalVal] = CastedVal;
        insertRewiredValue(OriginalVal, CastedVal);
        ECG->insertDependency(Index, CastedVal);
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
  EDT *ParentEDT = E.getParent();
  if (!ParentEDT) {
    LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                      << " doesn't have a parent EDT\n");
    return;
  }

  EDTGraph &Graph = Cache->getGraph();
  EDTGraphDataEdge *InputParentDataEdges =
      dyn_cast<EDTGraphDataEdge>(Graph.getEdge(ParentEDT, &E));
  auto EDTName = E.getName();
  /// Set insertion point to the EDT Call
  auto OldInsertPoint = Builder.saveIP();
  Builder.SetInsertPoint(E.getCall());

  /// ParamC
  uint32_t ParamSize = 0;
  uint32_t GuidSize = 0;
  if (InputParentDataEdges) {
    ParamSize = InputParentDataEdges->getParameters().size();
    GuidSize = InputParentDataEdges->getGuids().size();
  }
  AllocaInst *ParamC =
      Builder.CreateAlloca(Int32, nullptr, EDTName + "_paramc");
  Builder.CreateStore(ConstantInt::get(Int32, ParamSize + GuidSize), ParamC);
  LoadInst *LoadedParamC = Builder.CreateLoad(Int32, ParamC);

  /// ParamV
  AllocaInst *ParamV =
      Builder.CreateAlloca(Int64, LoadedParamC, EDTName + "_paramv");
  if (InputParentDataEdges) {
    auto &InputParameters = InputParentDataEdges->getParameters();
    for (auto Param : enumerate(InputParameters)) {
      LLVM_DEBUG(dbgs() << " - ParamV[" << Param.index()
                        << "]: " << *Param.value() << "\n");
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

    /// Guids
    EDTCodegen *ParentECG = getOrCreateEDTCodegen(*ParentEDT);
    const uint32_t InitialIndex = InputParameters.size();
    auto &InputGuids = InputParentDataEdges->getGuids();
    for (auto Guid : enumerate(InputGuids)) {
      LLVM_DEBUG(dbgs() << " - ParamVGuid[" << Guid.index() << "]: EDT"
                        << Guid.value()->getID() << "\n");
      unsigned Index = Guid.index() + InitialIndex;
      EDT *Edt = Guid.value();

      /// If the the GUID of the EDT is created in the parent EDT, get it,
      /// otherwise, check if my parent knows it.
      Value *GuidValue = ParentECG->getGuid(Edt);
      if (!GuidValue) {
        LLVM_DEBUG(dbgs() << "     Parent EDT #" << ParentEDT->getID()
                          << " doesn't have a GUID for EDT #" << Edt->getID()
                          << "\n");
        llvm_unreachable("Parent EDT doesn't have a GUID for the child EDT");
      }
      /// Assert the GuidValue is a pointer
      assert(GuidValue && "GuidValue is null");
      assert(GuidValue->getType() == Int32Ptr &&
             "GuidValue is not an Int32Ptr");
      /// Create the GEP to store the Guid value in the ParamV array
      auto ParamVGuidName =
          EDTName + "_paramv_guid.edt_" + std::to_string(Edt->getID());
      Value *ParamVGuiElemPtr = Builder.CreateConstInBoundsGEP1_64(
          Int64, ParamV, Index, ParamVGuidName);
      /// Cast the value to int32
      Value *CastedVal = Builder.CreatePtrToInt(GuidValue, Int32);
      Builder.CreateStore(CastedVal, ParamVGuiElemPtr);
    }
  }

  /// DepC
  EDTEnvironment &EDTEnv = E.getDataEnv();
  AllocaInst *DepC = Builder.CreateAlloca(Int32, nullptr);
  Builder.CreateStore(ConstantInt::get(Int32, EDTEnv.getDepC()), DepC);

  /// Create EDT
  LLVM_DEBUG(dbgs() << " - Creating EDT\n");
  EDTCodegen *ECG = getOrCreateEDTCodegen(E);
  Function *F = getOrCreateRuntimeFunctionPtr(ARTSRTL_artsEdtCreateWithGuid);
  Value *Args[] = {Builder.CreateBitCast(ECG->getFn(), EdtFunctionPtr),
                   Builder.CreateBitCast(ECG->getGuidAddress(), Int32Ptr),
                   LoadedParamC, ParamV, Builder.CreateLoad(Int32, DepC)};
  ECG->setCB(Builder.CreateCall(F, Args));

  /// We can remove both the function and the call
  utils::insertValueToRemove(E.getCall());
  utils::insertValueToRemove(E.getFn());

  /// Restore the insertion point
  Builder.restoreIP(OldInsertPoint);
}

void ARTSCodegen::insertEDTSignals(EDT &E) {
  LLVM_DEBUG(dbgs() << TAG << "Inserting Signals for EDT #" << E.getID()
                    << "\n");
  /// Analyze all the data outgoing edges from the EDT
  EDTCodegen *ECG = getOrCreateEDTCodegen(E);
  EDTGraph &Graph = Cache->getGraph();
  auto OutgoingEdges = Graph.getOutgoingEdges(&E);
  Function *F = getOrCreateRuntimeFunctionPtr(ARTSRTL_artsSignalEdtValue);
  /// Set insertion point to the exit block of the EDT function
  setInsertionPointInBB(*ECG->getExit());
  for (auto *Edge : OutgoingEdges) {
    auto *DataEdge = dyn_cast<EDTGraphDataEdge>(Edge);
    if (!DataEdge)
      continue;

    EDT *ToEDT = DataEdge->getTo()->getEDT();
    // EDTCodegen *ToECG = getOrCreateEDTCodegen(*ToEDT);
    LLVM_DEBUG(dbgs() << " - DataEdge to EDT #" << ToEDT->getID() << "\n");
    Value *ToEDTGuid = ECG->getGuid(ToEDT);
    assert(ToEDTGuid && "ToEDTGuid is null");

    auto &DataBlocks = DataEdge->getDataBlocks();
    for (auto Dep : enumerate(DataBlocks)) {
      EDTDataBlock *DB = Dep.value();
      LLVM_DEBUG(dbgs() << " - Signal: " << *DB->getValue()
                        << " / Slot: " << DB->getSlot() << "\n");
      Value *DBValue = ECG->getDependency(DB->getSlot());
      if (DBValue) {
        if (Value *RewiredValue = getRewiredValue(DBValue))
          DBValue = RewiredValue;
      } else
        DBValue = DB->getValue();

      LLVM_DEBUG(dbgs() << "    - " << *DBValue << "\n");
      /// Load ToEDTSlot in an alloca integer
      uint32_t ToEDTSlot = DB->getToSlot();
      AllocaInst *ToEDTSlotAlloca =
          Builder.CreateAlloca(Int32, nullptr,
                               "edt." + std::to_string(ToEDT->getID()) +
                                   ".slot." + std::to_string(ToEDTSlot));
      Builder.CreateStore(ConstantInt::get(Int32, ToEDTSlot), ToEDTSlotAlloca);
      /// Create Call arguments
      Value *Args[] = {Builder.CreateBitCast(ToEDTGuid, Int32Ptr),
                       Builder.CreateLoad(Int32, ToEDTSlotAlloca),
                       Builder.CreateLoad(Int64, DBValue)};
      Builder.CreateCall(F, Args);
    }
  }
}

void ARTSCodegen::createInitPerNodeFn() {
  auto *FnTy = FunctionType::get(Void, {Int32, Int32, Int8PtrPtr}, false);
  auto *Fn =
      Function::Create(FnTy, GlobalValue::InternalLinkage, "initPerNode", &M);
  Fn->arg_begin()->setName("nodeId");
  (Fn->arg_begin() + 1)->setName("argc");
  (Fn->arg_begin() + 2)->setName("argv");
  /// Create the entry block
  BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "entry", Fn);
  Builder.SetInsertPoint(EntryBB);
  Builder.CreateRetVoid();
}

void ARTSCodegen::createInitPerWorkerFn() {
  auto *FnTy =
      FunctionType::get(Void, {Int32, Int32, Int32, Int8PtrPtr}, false);
  auto *Fn =
      Function::Create(FnTy, GlobalValue::InternalLinkage, "initPerWorker", &M);
  Fn->arg_begin()->setName("nodeId");
  (Fn->arg_begin() + 1)->setName("workerId");
  (Fn->arg_begin() + 2)->setName("argc");
  (Fn->arg_begin() + 3)->setName("argv");
  /// Create the entry block
  BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "entry", Fn);
  Builder.SetInsertPoint(EntryBB);

  /// if nodeId == 0 && workerId == 0
  /// Create conditional branch
  Value *NodeId = Fn->arg_begin();
  Value *WorkerId = Fn->arg_begin() + 1;
  Value *IsMainWorker = Builder.CreateAnd(
      Builder.CreateICmpEQ(NodeId, ConstantInt::get(Int32, 0)),
      Builder.CreateICmpEQ(WorkerId, ConstantInt::get(Int32, 0)));
  BasicBlock *ThenBB = BasicBlock::Create(M.getContext(), "then", Fn);
  BasicBlock *ElseBB = BasicBlock::Create(M.getContext(), "else", Fn);
  Builder.CreateCondBr(IsMainWorker, ThenBB, ElseBB);

  /// then -> Create guid for main edt
  Builder.SetInsertPoint(ThenBB);
  EDT *MainEDT = Cache->getGraph().getEntryNode()->getEDT();
  auto MainEDTName = MainEDT->getName();
  EDTCodegen *MainECG = getOrCreateEDTCodegen(*MainEDT);
  Value *GuidAddress = getOrCreateEDTGuid(MainEDTName, MainEDT->getNode());
  MainECG->setGuidAddress(GuidAddress);

  /// Create the call to the main EDT
  AllocaInst *ParamC =
      Builder.CreateAlloca(Int32, nullptr, MainEDTName + "_paramc");
  Builder.CreateStore(ConstantInt::get(Int32, 0), ParamC);
  LoadInst *LoadedParamC = Builder.CreateLoad(Int32, ParamC);
  AllocaInst *ParamV =
      Builder.CreateAlloca(Int64, LoadedParamC, MainEDTName + "_paramv");
  Function *F = getOrCreateRuntimeFunctionPtr(ARTSRTL_artsEdtCreateWithGuid);
  Value *Args[] = {Builder.CreateBitCast(MainECG->getFn(), EdtFunctionPtr),
                   Builder.CreateBitCast(MainECG->getGuidAddress(), Int32Ptr),
                   LoadedParamC, ParamV, ConstantInt::get(Int32, 0)};
  Builder.CreateCall(F, Args);
  Builder.CreateRetVoid();

  /// else -> return void
  Builder.SetInsertPoint(ElseBB);
  Builder.CreateRetVoid();
}

void ARTSCodegen::createMainFn() {
  FunctionType *MainFnTy = FunctionType::get(Int32, {Int32, Int8PtrPtr}, false);
  Function *OldMainFn = Cache->getGraph().getEntryNode()->getEDT()->getFn();
  Function *MainFn = Function::Create(MainFnTy, GlobalValue::ExternalLinkage,
                                      OldMainFn->getAddressSpace(), "main", &M);
  MainFn->arg_begin()->setName("argc");
  (MainFn->arg_begin() + 1)->setName("argv");
  MainFn->takeName(OldMainFn);
  utils::removeValue(OldMainFn);
  /// Insert the entry block
  BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "entry", MainFn);
  Builder.SetInsertPoint(EntryBB);
  /// Call the artsInit function
  FunctionCallee RTFn =
      getOrCreateRuntimeFunction(types::RuntimeFunction::ARTSRTL_artsRT);
  Value *RTFnArgs[] = {MainFn->arg_begin(), MainFn->arg_begin() + 1};
  Builder.CreateCall(RTFn, RTFnArgs);
  Builder.CreateRet(ConstantInt::get(Int32, 0));
}

void ARTSCodegen::insertInitFunctions() {
  LLVM_DEBUG(dbgs() << TAG << "Inserting Init Functions\n");
  createInitPerNodeFn();
  createInitPerWorkerFn();
  createMainFn();
}

void ARTSCodegen::insertARTSShutdownFn() {
  LLVM_DEBUG(dbgs() << TAG << "Inserting ARTS Shutdown Function\n");
  EDTGraphNode *ExitNode = Cache->getGraph().getExitNode();
  EDT *ExitEDT = ExitNode->getEDT();
  EDTCodegen *ExitECG = getOrCreateEDTCodegen(*ExitEDT);
  setInsertionPointInBB(*ExitECG->getExit());
  /// Call the artsShutdown function
  FunctionCallee RTFn =
      getOrCreateRuntimeFunction(types::RuntimeFunction::ARTSRTL_artsShutdown);
  Builder.CreateCall(RTFn);
}

/// ---------------------------- Utils ---------------------------- ///
void ARTSCodegen::setInsertionPointInBB(BasicBlock &InsertionBB) {
  Instruction *EntryBBTerm = InsertionBB.getTerminator();
  if (EntryBBTerm)
    Builder.SetInsertPoint(EntryBBTerm);
  else
    Builder.SetInsertPoint(&InsertionBB);
}

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
void ARTSCodegen::setInsertPoint(BasicBlock *BB) { Builder.SetInsertPoint(BB); }

void ARTSCodegen::setInsertPoint(Instruction *I) { Builder.SetInsertPoint(I); }

void ARTSCodegen::insertRewiredValue(Value *OriginalVal, Value *CastedVal) {
  if (!getRewiredValue(OriginalVal))
    RewiredValues[OriginalVal] = CastedVal;
}

Value *ARTSCodegen::getRewiredValue(Value *OriginalVal) {
  if (RewiredValues.find(OriginalVal) != RewiredValues.end())
    return RewiredValues[OriginalVal];
  return nullptr;
}

/// ---------------------------- Private ---------------------------- ///
EDTCodegen *ARTSCodegen::getOrCreateEDTCodegen(EDT &E) {
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