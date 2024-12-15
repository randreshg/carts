//===- ARTSCodegen.cpp - Builder for LLVM-IR for ARTS directives ----===//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file implements the ARTSCodegen class, which is used as a
/// convenient way to create LLVM instructions for ARTS directives.
///
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

#include "carts/analysis/graph/ARTSEdge.h"
#include "carts/analysis/graph/ARTSGraph.h"
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

/// -------------------------- EDTSlotCodegen --------------------------- ///
EDTSlotCodegen::EDTSlotCodegen(Value *GuidAddress, Value *Ptr)
    : GuidAddress(GuidAddress), Ptr(Ptr) {}
Value *EDTSlotCodegen::getGuidAddress() { return GuidAddress; }
void EDTSlotCodegen::setGuidAddress(Value *V) { GuidAddress = V; }
Value *EDTSlotCodegen::getPtr() { return Ptr; }
void EDTSlotCodegen::setPtr(Value *V) { Ptr = V; }

/// ---------------------------- EDTCodegen ---------------------------- ///
EDTCodegen::EDTCodegen(EDT *E) : E(E) {}
EDTCodegen::EDTCodegen(EDT *E, Function *Fn) : E(E), Fn(Fn) {}
EDTCodegen::~EDTCodegen() {
  for (auto &Dep : Dependencies)
    delete Dep.second;
  Dependencies.clear();
}
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

Value *EDTCodegen::getParameter(uint32_t Slot) {
  auto It = Parameters.find(Slot);
  if (It != Parameters.end())
    return It->second;
  return nullptr;
}

bool EDTCodegen::insertParameter(uint32_t Slot, Value *V) {
  if (getParameter(Slot))
    return false;
  Parameters[Slot] = V;
  return true;
}

EDTSlotCodegen *EDTCodegen::getDependency(uint32_t Slot) {
  auto It = Dependencies.find(Slot);
  if (It != Dependencies.end())
    return It->second;
  return nullptr;
}

Value *EDTCodegen::getDependencyGuid(uint32_t Slot) {
  if (EDTSlotCodegen *Dep = getDependency(Slot))
    return Dep->getGuidAddress();
  return nullptr;
}

Value *EDTCodegen::getDependencyPtr(uint32_t Slot) {
  if (EDTSlotCodegen *Dep = getDependency(Slot))
    return Dep->getPtr();
  return nullptr;
}

bool EDTCodegen::insertDependency(uint32_t Slot, Value *GuidAddress,
                                  Value *Ptr) {
  if (getDependency(Slot))
    return false;
  Dependencies[Slot] = new EDTSlotCodegen(GuidAddress, Ptr);
  return true;
}

Value *EDTCodegen::getGuid(EDT *E) {
  auto It = Guids.find(E);
  if (It != Guids.end())
    return It->second;
  return nullptr;
}

bool EDTCodegen::insertGuid(EDT *E, Value *V) {
  if (getGuid(E))
    return false;
  Guids[E] = V;
  return true;
}

/// ---------------------------- DBCodegen ---------------------------- ///
DBCodegen::DBCodegen(DataBlock *DB) : DB(DB) {}
Value *DBCodegen::getGuidAddress() { return GuidAddress; }
void DBCodegen::setGuidAddress(Value *V) { GuidAddress = V; }
Value *DBCodegen::getPtr() { return Ptr; }
void DBCodegen::setPtr(Value *V) { Ptr = V; }

/// ---------------------------- ARTSCodegen ---------------------------- ///
ARTSCodegen::ARTSCodegen(ARTSCache *Cache)
    : Cache(Cache), M(Cache->getModule()),
      Builder(Cache->getModule().getContext()) {
  initialize();
}

ARTSCodegen::~ARTSCodegen() {
  /// Delete all the EDTs and DBs
  for (auto &ECG : EDTs)
    delete ECG.second;

  for (auto &DCG : DBs)
    delete DCG.second;
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
  }

  assert(Fn && "Failed to create ARTS runtime function");

  return {FnTy, Fn};
}

Function *ARTSCodegen::getOrCreateRuntimeFunctionPtr(RuntimeFunction FnID) {
  FunctionCallee RTLFn = getOrCreateRuntimeFunction(FnID);
  Function *Fn = dyn_cast<llvm::Function>(RTLFn.getCallee());
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

Value *ARTSCodegen::createEDTGuid(string EDTName, uint32_t EDTNode) {
  /// Create the call to reserve the GUID
  ConstantInt *EDTType = ConstantInt::get(Builder.getContext(), APInt(32, 1));
  Value *Args[] = {
      EDTType,
      Builder.CreateIntCast(ConstantInt::get(Int32, EDTNode), Int32, false)};
  CallInst *ReserveGuidCall = Builder.CreateCall(
      getOrCreateRuntimeFunctionPtr(ARTSRTL_artsReserveGuidRoute), Args);
  ReserveGuidCall->setTailCall(true);
  /// Create allocation of the GUID
  AllocaInst *GuidAddress =
      Builder.CreateAlloca(artsGuid_t, nullptr, EDTName + "_guid.addr");
  Builder.CreateStore(ReserveGuidCall, GuidAddress);
  LoadInst *LoadedGuidAddress = Builder.CreateLoad(artsGuid_t, GuidAddress);
  /// Print the new GUID
  // SmallVector<Value *, 8> PrintArgs;
  // PrintArgs = {LoadedGuidAddress};
  // string PrintMsg = "Guid for " + EDTName + ": %u\n";
  // insertPrint(PrintMsg, PrintArgs);
  return LoadedGuidAddress;
}

Value *ARTSCodegen::getOrCreateEDTGuid(EDT &E, BasicBlock *InsertionBB) {
  /// Check if the EDT already has a GUID
  EDTCodegen *ECG = getOrCreateEDTCodegen(E);
  if (ECG->getGuidAddress())
    return ECG->getGuidAddress();

  LLVM_DEBUG(dbgs() << TAG << "Reserving GUID for EDT #" << E.getID() << "\n");
  EDT *EDTParent = E.getParent();
  if (!InsertionBB) {
    Function *ParentFn = nullptr;
    /// If no insertion BB provided, try to get the parent EDT
    if (!EDTParent) {
      LLVM_DEBUG(
          dbgs() << "     EDT #" << E.getID()
                 << " doesn't have a parent EDT, using parent function\n");
      assert(E.isMain() && "A non-main EDT should have a parent EDT");
      ParentFn = E.getCall()->getFunction();
      BasicBlock *OldEntryBB = &ParentFn->getEntryBlock();
      OldEntryBB->setName("body");
      /// Insert EntryBB to the main function
      BasicBlock *EntryBB =
          BasicBlock::Create(M.getContext(), "entry", ParentFn, OldEntryBB);
      redirectTo(EntryBB, OldEntryBB);
    } else {
      /// If it has parent EDT set insertion BB to the parent EDT entry block
      ParentFn = getOrCreateEDTFunction(*EDTParent);
    }
    /// Set the insertion BB to the entry block of the parent function
    InsertionBB = &ParentFn->getEntryBlock();
  }

  auto OldInsertPoint = Builder.saveIP();
  if (InsertionBB->getTerminator())
    Builder.SetInsertPoint(InsertionBB->getTerminator());
  else
    Builder.SetInsertPoint(InsertionBB);

  /// Create the GUID
  Value *GuidAddress = createEDTGuid(E.getName(), E.getNode());
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
  LLVM_DEBUG(dbgs() << TAG << "Inserting Entry for EDT #" << E.getID() << "\n");
  auto OldInsertPoint = Builder.saveIP();
  /// Set the insertion point to the entry block of the EDT function
  Function *EDTFn = getOrCreateEDTFunction(E);
  setInsertionPointInBB(EDTFn->getEntryBlock());

  /// It has the information of all the values that are going to be rewired
  DenseMap<Value *, Value *> RewiringMap;

  /// Get the information we know about the Module
  ARTSGraph &Graph = Cache->getGraph();
  EDTCodegen *ECG = getOrCreateEDTCodegen(E);
  EDTGraphNode *EDTNode = Graph.getOrCreateNode(&E);
  string EDTTagName = E.getTag();

  /// Parameters
  LLVM_DEBUG(dbgs() << " - Inserting ParamV\n");
  auto handleParameters = [&]() -> void {
    /// The input parameters and Guids always come from the parent EDT
    EDT *Parent = E.getParent();
    if (!Parent) {
      LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                        << " doesn't have a parent EDT\n");
      return;
    }
    CreationGraphEdge *InputParentEdge = Graph.getEdge(Parent, &E);
    const auto ParamVArg = EDTFn->arg_begin() + 1;

    /// Input parameters
    uint32_t ParameterIndex = 0;
    InputParentEdge->forEachParameter([&](EDTValue *ParameterValue) {
      Value *OriginalVal = Cache->getEDTArg(&E, ParameterValue);
      LLVM_DEBUG(dbgs() << "   - ParamV[" << ParameterIndex
                        << "]: " << *OriginalVal);
      string ParamVName =
          EDTTagName + ".paramv_" + std::to_string(ParameterIndex);
      Value *ParamVElemPtr = Builder.CreateConstInBoundsGEP1_64(
          Int64, ParamVArg, ParameterIndex, ParamVName);
      /// Load the value from the array
      Value *LoadedVal = Builder.CreateLoad(Int64, ParamVElemPtr);
      /// Cast the value to the original type
      Value *CastedVal = LoadedVal;
      Type *OriginalType = OriginalVal->getType();
      switch (OriginalType->getTypeID()) {
      /// Integer
      case llvm::Type::IntegerTyID: {
        if (OriginalType != Int64)
          CastedVal = Builder.CreateTrunc(LoadedVal, OriginalType);
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
      LLVM_DEBUG(dbgs() << " -> " << *CastedVal << "\n");
      /// Add the value to the rewiring map
      RewiringMap[OriginalVal] = CastedVal;
      insertRewiredValue(OriginalVal, CastedVal);
      ECG->insertParameter(ParameterIndex, CastedVal);

      /// Increment the parameter index
      ParameterIndex++;
    });

    /// Input Guids
    uint32_t GuidIndex = ParameterIndex;
    InputParentEdge->forEachGuid([&](EDT *Guid) {
      LLVM_DEBUG(dbgs() << "   - ParamVGuid[" << GuidIndex << "]: EDT"
                        << Guid->getID() << "\n");
      auto ParamVName = EDTTagName + ".paramv_" + std::to_string(GuidIndex) +
                        ".guid.edt_" + std::to_string(Guid->getID());
      Value *ParamVElemPtr = Builder.CreateConstInBoundsGEP1_64(
          artsGuid_t, ParamVArg, GuidIndex, ParamVName);
      LoadInst *LoadedGuid = Builder.CreateLoad(artsGuid_t, ParamVElemPtr);
      ECG->insertGuid(Guid, LoadedGuid);
      /// Increment the GUID index
      GuidIndex++;
    });
  };

  /// Dependencies
  auto handleDependencies = [&]() -> void {
    LLVM_DEBUG(dbgs() << " - Inserting DepV\n");

    if (EDTNode->getIncomingSlotNodesSize() == 0) {
      LLVM_DEBUG(dbgs() << "     EDT #" << E.getID()
                        << " doesn't have incoming slot nodes\n");
      return;
    }

    auto DepVArg = EDTFn->arg_begin() + 3;
    EDTNode->forEachIncomingSlotNode([&](EDTGraphSlotNode *IncomingSlotNode) {
      uint32_t Slot = IncomingSlotNode->getSlot();
      Value *OriginalVal = E.getDepArg(Slot);
      LLVM_DEBUG(dbgs() << "   - DepV[" << Slot << "]: " << *OriginalVal);

      const string DepVName = EDTTagName + ".depv_" + std::to_string(Slot);
      /// Guid
      string DepVGuidName = DepVName + ".guid";
      Value *DepVGuid = Builder.CreateConstInBoundsGEP2_32(
          EdtDep, DepVArg, Slot, 0, DepVGuidName);
      LoadInst *LoadedGuid =
          Builder.CreateLoad(artsGuid_t, DepVGuid, DepVGuidName + ".load");
      /// Ptr
      string DepVPtrName = DepVName + ".ptr";
      Value *DepVPtr = Builder.CreateConstInBoundsGEP2_32(EdtDep, DepVArg, Slot,
                                                          2, DepVPtrName);
      LoadInst *LoadedPtr =
          Builder.CreateLoad(VoidPtr, DepVPtr, DepVPtrName + ".load");

      LLVM_DEBUG(dbgs() << " -> " << *LoadedPtr << "\n");
      /// Debug info
      // SmallVector<Value *, 8> PrintArgs;
      // PrintArgs = {LoadedGuid, LoadedPtr};
      // string PrintMsg = "EDT#" + std::to_string(E.getID()) + " - DepV[" +
      //                   std::to_string(Slot) + "]: Guid: %u, Ptr: %p\n";
      // insertPrint(PrintMsg, PrintArgs);

      /// Add the value to the rewiring map
      RewiringMap[OriginalVal] = LoadedPtr;
      insertRewiredValue(OriginalVal, LoadedPtr);
      ECG->insertDependency(Slot, LoadedGuid, LoadedPtr);
    });
  };

  handleParameters();
  handleDependencies();
  utils::rewireValues(RewiringMap);

  /// Create DataBlocks
  LLVM_DEBUG(dbgs() << " - Inserting DataBlocks\n");
  Graph.forEachOutgoingDataBlockEdge(EDTNode, [&](DataBlockGraphEdge *Edge) {
    DataBlock *DB = Edge->getDataBlock();
    DBCodegen *DCG = getOrCreateDBCodegen(*DB);
    if (!DCG)
      return;
    Value *DBValue = DB->getValue();
    RewiringMap[DBValue] = DCG->getPtr();
    insertRewiredValue(DBValue, DCG->getPtr());
  });
  utils::rewireValues(RewiringMap);
  Builder.restoreIP(OldInsertPoint);
}

void ARTSCodegen::insertEDTCall(EDT &E) {
  auto OldInsertPoint = Builder.saveIP();
  string EDTTagName = E.getTag();

  LLVM_DEBUG(dbgs() << TAG << "Inserting Call for EDT #" << E.getID() << "\n");
  EDT *ParentEDT = E.getParent();
  if (!ParentEDT)
    assert(E.isMain() && "A non-main EDT should have a parent EDT");

  /// Parent variables in case there is parent
  CreationGraphEdge *InputParentEdge = nullptr;

  /// Get graph information
  ARTSGraph &Graph = Cache->getGraph();
  EDTGraphNode *EDTNode = Graph.getOrCreateNode(&E);

  /// Set insertion point to the EDT Call
  Builder.SetInsertPoint(E.getCall());

  /// Insert ParamC
  uint32_t ParamSize = 0;
  uint32_t GuidSize = 0;
  if (ParentEDT) {
    /// Get input parent edge
    InputParentEdge = Graph.getEdge(ParentEDT, &E);
    assert(InputParentEdge && "Creation edge not found");
    ParamSize = InputParentEdge->getParametersSize();
    GuidSize = InputParentEdge->getGuidsSize();
  }

  /// ParamC
  AllocaInst *ParamC =
      Builder.CreateAlloca(Int32, nullptr, EDTTagName + "_paramc");
  Builder.CreateStore(ConstantInt::get(Int32, ParamSize + GuidSize), ParamC);
  LoadInst *LoadedParamC = Builder.CreateLoad(Int32, ParamC);
  /// ParamV
  AllocaInst *ParamV =
      Builder.CreateAlloca(Int64, LoadedParamC, EDTTagName + "_paramv");
  if (ParentEDT) {
    /// Insert the parameters
    uint32_t ParameterIndex = 0;
    InputParentEdge->forEachParameter([&](EDTValue *ParameterValue) {
      LLVM_DEBUG(dbgs() << " - ParamV[" << ParameterIndex
                        << "]: " << *ParameterValue << "\n");
      auto ParamVName =
          EDTTagName + ".paramv_" + std::to_string(ParameterIndex);
      /// Create the GEP to store the value in the ParamV array
      Value *ParamVElemPtr = Builder.CreateConstInBoundsGEP1_64(
          Int64, ParamV, ParameterIndex, ParamVName);
      /// Cast the value to int64
      Value *CastedVal = nullptr;
      auto *ValType = ParameterValue->getType();
      switch (ValType->getTypeID()) {
      /// Integer
      case llvm::Type::IntegerTyID: {
        if (ValType != Int64)
          CastedVal = Builder.CreateSExtOrTrunc(ParameterValue, Int64);
        else
          CastedVal = ParameterValue;
      } break;
      /// Default
      default:
        assert(false && "Type not supported yet");
        break;
      }
      Builder.CreateStore(CastedVal, ParamVElemPtr);
      /// Increment the parameter index
      ParameterIndex++;
    });

    /// Insert the guids
    uint32_t GuidIndex = ParameterIndex;
    EDTCodegen *ParentECG = getOrCreateEDTCodegen(*ParentEDT);
    InputParentEdge->forEachGuid([&](EDT *Guid) {
      LLVM_DEBUG(dbgs() << " - ParamVGuid[" << GuidIndex << "]: EDT"
                        << Guid->getID() << "\n");
      /// Get the GUID value
      Value *GuidValue = ParentECG->getGuid(Guid);
      if (!GuidValue) {
        LLVM_DEBUG(dbgs() << "     Parent EDT #" << ParentEDT->getID()
                          << " doesn't have a GUID for EDT #" << Guid->getID()
                          << "\n");
        llvm_unreachable("Parent EDT doesn't have a GUID for the child EDT");
      }
      /// Create the GEP to store the Guid value in the ParamV array
      string ParamVGuidName = EDTTagName + ".paramv_" +
                              std::to_string(GuidIndex) + ".guid.edt_" +
                              std::to_string(Guid->getID());
      Value *ParamVGuiElemPtr = Builder.CreateConstInBoundsGEP1_64(
          artsGuid_t, ParamV, GuidIndex, ParamVGuidName);
      Builder.CreateStore(GuidValue, ParamVGuiElemPtr);
      /// Increment the GUID index
      GuidIndex++;
    });
  }

  /// DepC
  AllocaInst *DepC = Builder.CreateAlloca(Int32, nullptr);
  Builder.CreateStore(
      ConstantInt::get(Int32, EDTNode->getIncomingSlotNodesSize()), DepC);

  /// Debug info
  // LLVM_DEBUG(dbgs() << " - Inserting EDT Call\n");
  // SmallVector<Value *, 8> PrintArgs;
  // string PrintMsg = "Creating EDT #" + std::to_string(E.getID()) + "\n";
  // insertPrint(PrintMsg, PrintArgs);

  /// Insert EDT Call
  EDTCodegen *ECG = getOrCreateEDTCodegen(E);
  Function *F = getOrCreateRuntimeFunctionPtr(ARTSRTL_artsEdtCreateWithGuid);
  Value *Args[] = {Builder.CreateBitCast(ECG->getFn(), EdtFunctionPtr),
                   ECG->getGuidAddress(), LoadedParamC, ParamV,
                   Builder.CreateLoad(Int32, DepC)};
  ECG->setCB(Builder.CreateCall(F, Args));
  LLVM_DEBUG(dbgs() << " - EDT Call: " << *ECG->getCB() << "\n");

  /// We can remove both the function and the call
  utils::insertValueToRemove(E.getCall());
  utils::insertValueToRemove(E.getFn());

  /// Restore the insertion point
  Builder.restoreIP(OldInsertPoint);
}

void ARTSCodegen::insertEDTSignals(EDT &E) {
  LLVM_DEBUG(dbgs() << TAG << "Inserting Signals from EDT #" << E.getID()
                    << "\n");
  Function *SignalFn = getOrCreateRuntimeFunctionPtr(ARTSRTL_artsSignalEdt);
  /// Analyze all the data outgoing edges from the EDT
  EDTCodegen *ECG = getOrCreateEDTCodegen(E);
  ARTSGraph &Graph = Cache->getGraph();
  /// Set insertion point to the exit block of the EDT function
  setInsertionPointInBB(*ECG->getExit());

  EDTGraphNode *EDTNode = Graph.getOrCreateNode(&E);
  EDT *FromEDT = EDTNode->getEDT();
  EDTCodegen *FromECG = getOrCreateEDTCodegen(*FromEDT);
  // LLVM_DEBUG(dbgs() << *FromECG->getFn() << "\n");
  Graph.forEachOutgoingDataBlockEdge(EDTNode, [&](DataBlockGraphEdge *Edge) {
    DataBlock *DB = Edge->getDataBlock();
    EDT *ContextEDT = DB->getContextEDT();
    LLVM_DEBUG(dbgs() << "\nSignal DB " << *DB->getValue() << " from EDT #"
                      << E.getID() << " to EDT #"
                      << Edge->getTo()->getParent()->getEDT()->getID() << "\n");
    LLVM_DEBUG(dbgs() << " - ContextEDT: " << ContextEDT->getID() << "\n");

    /// To EDT
    EDT *ToEDT = Edge->getTo()->getParent()->getEDT();
    uint32_t ToSlot = Edge->getTo()->getSlot();

    /// DataBlock Information
    Value *DBPtr = nullptr;
    Value *DBGuid = nullptr;

    /// Since the DB is always relative to its use in the EDTCall, we need to
    /// consider different cases: 
    /// - If the Context EDT of the DataBlock is the same as the that we have
    ///   to signal to, it means that the DB is sent from the
    ///   parent EDT to the EDT child. There are two cases here:
    ///   - If the DB doesn't have a parent EDT, we created it.
    ///   - Otherwise, the DB comes from a EDTDepSlot
    /// - Otherwise, the value of the DB is the EDTDepSlot of the FromEDT
    if (ContextEDT == ToEDT) {
      if (DataBlock *ParentDB = DB->getParent()) {
        LLVM_DEBUG(dbgs() << " - We have a parent\n");
        EDTSlotCodegen *FromSlotECG =
            FromECG->getDependency(ParentDB->getSlot());
        assert(FromSlotECG && "FromSlotECG of Parent not found");
        DBPtr = FromSlotECG->getPtr();
        DBGuid = FromSlotECG->getGuidAddress();
      } else {
        LLVM_DEBUG(dbgs() << " - We created the EDT\n");
        DBCodegen *DBCG = getOrCreateDBCodegen(*DB);
        assert(DBCG && "DBCG not found");
        DBPtr = DBCG->getPtr();
        DBGuid = DBCG->getGuidAddress();
      }
    }
    else {
      LLVM_DEBUG(dbgs() << " - Using DepSlot of FromEDT - Slot: "
                        << DB->getSlot());
      EDTSlotCodegen *FromSlotECG = FromECG->getDependency(DB->getSlot());
      assert(FromSlotECG && "FromSlotECG not found");
      DBPtr = FromSlotECG->getPtr();
      DBGuid = FromSlotECG->getGuidAddress();
    }
    if (DBPtr)
      LLVM_DEBUG(dbgs() << " - DBPtr: " << *DBPtr << "\n");
    if (DBGuid)
      LLVM_DEBUG(dbgs() << " - DBGuid: " << *DBGuid << "\n");

    /// Create the Call arguments
    /// artsSignalEdt(artsGuid_t edtGuid, uint32_t slot, artsGuid_t dataGuid)
    AllocaInst *ToEDTSlotAlloca =
        Builder.CreateAlloca(Int32, nullptr,
                             "toedt." + std::to_string(ToEDT->getID()) +
                                 ".slot." + std::to_string(ToSlot));
    Builder.CreateStore(ConstantInt::get(Int32, ToSlot), ToEDTSlotAlloca);
    Value *Args[] = {ECG->getGuid(ToEDT),
                     Builder.CreateLoad(Int32, ToEDTSlotAlloca), DBGuid};

    /// Debug info
    // SmallVector<Value *, 8> PrintArgs;
    // LoadInst *LoadedVal = Builder.CreateLoad(Int32, DBPtr);
    // string PrintMsg = "Signaling " + DB->getName() +
    //                   " with guid: %u and Value %d, and Address %p from EDT #" +
    //                   std::to_string(E.getID()) + " to EDT #" +
    //                   std::to_string(ToEDT->getID()) + " with guid: %u\n";
    // PrintArgs = {DBGuid, LoadedVal, DBPtr, ECG->getGuid(ToEDT)};
    // insertPrint(PrintMsg, PrintArgs);

    /// Create the Call
    Builder.CreateCall(SignalFn, Args);
  });
}

void ARTSCodegen::createInitPerNodeFn() {
  auto *FnTy = FunctionType::get(Void, {Int32, Int32, Int8PtrPtr}, false);
  auto *Fn =
      Function::Create(FnTy, GlobalValue::ExternalLinkage, "initPerNode", &M);
  Fn->setDSOLocal(true);
  Fn->arg_begin()->setName("nodeId");
  (Fn->arg_begin() + 1)->setName("argc");
  (Fn->arg_begin() + 2)->setName("argv");
  /// Create the entry block
  BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "entry", Fn);
  Builder.SetInsertPoint(EntryBB);
  Builder.CreateRetVoid();
}

void ARTSCodegen::createInitPerWorkerFn() {
  FunctionType *FnTy =
      FunctionType::get(Void, {Int32, Int32, Int32, Int8PtrPtr}, false);
  Function *Fn =
      Function::Create(FnTy, GlobalValue::ExternalLinkage, "initPerWorker", &M);
  Fn->setDSOLocal(true);
  Fn->arg_begin()->setName("nodeId");
  (Fn->arg_begin() + 1)->setName("workerId");
  (Fn->arg_begin() + 2)->setName("argc");
  (Fn->arg_begin() + 3)->setName("argv");

  /// Splice the body of the main function right into the new function
  Function *MainFn = M.getFunction("main");
  Fn->splice(Fn->begin(), MainFn);
  utils::removeValue(MainFn);

  /// Then
  BasicBlock *ThenBB = &Fn->getEntryBlock();
  ThenBB->setName("then");

  /// Create the entry block
  BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "entry", Fn, ThenBB);
  Builder.SetInsertPoint(EntryBB);

  /// if nodeId == 0 && workerId == 0
  Value *NodeId = Fn->arg_begin();
  Value *WorkerId = Fn->arg_begin() + 1;
  Value *IsMainWorker = Builder.CreateAnd(
      Builder.CreateICmpEQ(NodeId, ConstantInt::get(Int32, 0)),
      Builder.CreateICmpEQ(WorkerId, ConstantInt::get(Int32, 0)));

  /// else
  BasicBlock *ExitBB = BasicBlock::Create(M.getContext(), "exit", Fn);
  Builder.SetInsertPoint(ExitBB);
  Builder.CreateRetVoid();

  /// Insert the conditional branch
  Builder.SetInsertPoint(EntryBB);
  Builder.CreateCondBr(IsMainWorker, ThenBB, ExitBB);

  /// Redirect the exit blocks to ExitBB
  redirectExitsTo(Fn, ExitBB);
}

void ARTSCodegen::createMainFn() {
  FunctionType *MainFnTy = FunctionType::get(Int32, {Int32, Int8PtrPtr}, false);
  Function *MainFn =
      Function::Create(MainFnTy, GlobalValue::ExternalLinkage, "main", &M);
  MainFn->setDSOLocal(true);
  MainFn->arg_begin()->setName("argc");
  (MainFn->arg_begin() + 1)->setName("argv");
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
void ARTSCodegen::insertPrint(string FormatStr, SmallVector<Value *, 8> &Args) {
  Constant *FormatStrVal = Builder.CreateGlobalStringPtr(FormatStr);
  FunctionType *PrintfTy = FunctionType::get(Int32, Int8Ptr, true);
  FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);
  SmallVector<Value *, 8> ArgsV;
  ArgsV.push_back(FormatStrVal);
  ArgsV.append(Args.begin(), Args.end());
  Builder.CreateCall(Printf, ArgsV);
}

void ARTSCodegen::setInsertionPointInBB(BasicBlock &InsertionBB) {
  Instruction *EntryBBTerm = InsertionBB.getTerminator();
  if (EntryBBTerm)
    Builder.SetInsertPoint(EntryBBTerm);
  else
    Builder.SetInsertPoint(&InsertionBB);
}

void ARTSCodegen::redirectTo(BasicBlock *Source, BasicBlock *Target) {
  if (Instruction *Term = Source->getTerminator()) {
    BranchInst *Br = cast<BranchInst>(Term);
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
    if (&BB == Target)
      continue;
    Instruction *Terminator = BB.getTerminator();
    if (Terminator->getNumSuccessors() > 0 || !isa<ReturnInst>(Terminator))
      continue;
    /// Remove the terminator and redirect the exit and
    Terminator->eraseFromParent();
    redirectTo(&BB, Target);
  }
}
void ARTSCodegen::setInsertPoint(BasicBlock *BB) { Builder.SetInsertPoint(BB); }

void ARTSCodegen::setInsertPoint(Instruction *I) { Builder.SetInsertPoint(I); }

void ARTSCodegen::rewireValues() { utils::rewireValues(RewiredValues); }

void ARTSCodegen::insertRewiredValue(Value *OriginalVal, Value *CastedVal) {
  if (!getRewiredValue(OriginalVal))
    RewiredValues[OriginalVal] = CastedVal;
}

Value *ARTSCodegen::getRewiredValue(Value *OriginalVal) {
  if (RewiredValues.find(OriginalVal) != RewiredValues.end())
    return RewiredValues[OriginalVal];
  return nullptr;
}

Value *ARTSCodegen::getValue(Value *V) {
  if (Value *RewiredValue = getRewiredValue(V))
    return RewiredValue;
  return V;
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

DBCodegen *ARTSCodegen::getOrCreateDBCodegen(DataBlock &DB) {
  if (DBs.find(&DB) != DBs.end())
    return DBs[&DB];

  if (DB.getParent())
    return nullptr;

  /// If the DB doesn't have a parent, create the DB
  LLVM_DEBUG(dbgs() << TAG << "Creating DBCodegen " << *DB.getValue() << "\n");
  DBCodegen *DCG = new DBCodegen(&DB);
  DBs[&DB] = DCG;

  auto OldInsertPoint = Builder.saveIP();
  /// Set the insertion point to the entry block of the parent context EDT
  EDTCodegen *ParentECG =
      getOrCreateEDTCodegen(*DB.getContextEDT()->getParent());
  setInsertionPointInBB(*ParentECG->getEntry());

  string DBName = DB.getName();

  /// Addr
  AllocaInst *DBAddr =
      Builder.CreateAlloca(artsPtr_t, nullptr, DBName + ".addr");

  /// Size
  string DBSizeName = DBName + ".size";
  AllocaInst *DBSize = Builder.CreateAlloca(Int64, nullptr, DBSizeName);
  Builder.CreateStore(
      ConstantInt::get(Int64, M.getDataLayout().getTypeAllocSize(DB.getType())),
      DBSize);
  LoadInst *LoadedDBSize =
      Builder.CreateLoad(Int64, DBSize, DBSizeName + ".ld");
  /// Mode
  ConstantInt *DBMode =
      ConstantInt::get(Builder.getContext(), APInt(32, 7)); /// ARTS_DB_READ
  CallInst *CreateDBCall =
      Builder.CreateCall(getOrCreateRuntimeFunctionPtr(ARTSRTL_artsDbCreatePtr),
                         {DBAddr, LoadedDBSize, DBMode});
  /// inttoptr
  LoadInst *LoadedDBAddr =
      Builder.CreateLoad(artsPtr_t, DBAddr, DBName + ".addr.ld");
  Value *DBPtr = Builder.CreateIntToPtr(LoadedDBAddr, VoidPtr, DBName + ".ptr");

  DCG->setGuidAddress(CreateDBCall);
  DCG->setPtr(DBPtr);

  /// Debug info
  // LLVM_DEBUG(dbgs() << " - Inserting EDT Call\n");
  // SmallVector<Value *, 8> PrintArgs;
  // string PrintMsg = "Creating DB \"" + DBName + "\" - Guid: %u / Pointer:
  // %p\n"; PrintArgs = {CreateDBCall, DBPtr}; insertPrint(PrintMsg, PrintArgs);

  /// Restore the insertion point
  Builder.restoreIP(OldInsertPoint);
  return DCG;
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