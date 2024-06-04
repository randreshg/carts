// //===- ARTSIRBuilder.cpp - Builder for LLVM-IR for ARTS directives ----===//
// //===----------------------------------------------------------------------===//
// /// \file
// ///
// /// This file implements the ARTSIRBuilder class, which is used as a
// /// convenient way to create LLVM instructions for ARTS directives.
// ///
// //===----------------------------------------------------------------------===//

// #include "ARTSIRBuilder.h"
// #include "ARTS.h"
// #include "llvm/IR/BasicBlock.h"
// #include "llvm/IR/Type.h"
// #include "llvm/Support/Debug.h"

// #include <cassert>

// // DEBUG
// #define DEBUG_TYPE "arts-ir-builder"
// #if !defined(NDEBUG)
// static constexpr auto TAG = "[" DEBUG_TYPE "] ";
// #endif

// // using namespace llvm;
// using namespace arts;
// using namespace arts::types;

// void ARTSIRBuilder::initialize() {
//   LLVM_DEBUG(dbgs() << TAG << "Initializing ARTSIRBuilder\n");
//   // Initialize the module with the runtime functions.
//   initializeTypes();
//   LLVM_DEBUG(dbgs() << TAG << "ARTSIRBuilder initialized\n");
// }

// void ARTSIRBuilder::finalize() {
//   LLVM_DEBUG(dbgs() << TAG << "Finalizing ARTSIRBuilder\n");
//   // Finalize the module with the runtime functions.
//   // finalizeModule(M);
//   LLVM_DEBUG(dbgs() << TAG << "ARTSIRBuilder finalized\n");
// }

// FunctionCallee ARTSIRBuilder::getOrCreateRuntimeFunction(Module &M,
//                                                          RuntimeFunction FnID) {
//   FunctionType *FnTy = nullptr;
//   Function *Fn = nullptr;

//   // Try to find the declaration in the module first.
//   switch (FnID) {
// #define ARTS_RTL(Enum, Str, IsVarArg, ReturnType, ...)                         \
//   case Enum:                                                                   \
//     FnTy = FunctionType::get(ReturnType, ArrayRef<Type *>{__VA_ARGS__},        \
//                              IsVarArg);                                        \
//     Fn = M.getFunction(Str);                                                   \
//     break;
// #include "ARTSKinds.def"
//   }

//   if (!Fn) {
//     // Create a new declaration if we need one.
//     switch (FnID) {
// #define ARTS_RTL(Enum, Str, ...)                                               \
//   case Enum:                                                                   \
//     Fn = Function::Create(FnTy, GlobalValue::ExternalLinkage, Str, M);         \
//     break;
// #include "ARTSKinds.def"
//     }

//     LLVM_DEBUG(dbgs() << TAG << "Created ARTS runtime function "
//                       << Fn->getName() << " with type "
//                       << *Fn->getFunctionType() << "\n");
//     // addAttributes(FnID, *Fn);

//   } else {
//     LLVM_DEBUG(dbgs() << TAG << "Found ARTS runtime function " << Fn->getName()
//                       << " with type " << *Fn->getFunctionType() << "\n");
//   }

//   assert(Fn && "Failed to create ARTS runtime function");

//   return {FnTy, Fn};
// }

// Function *ARTSIRBuilder::getOrCreateRuntimeFunctionPtr(RuntimeFunction FnID) {
//   FunctionCallee RTLFn = getOrCreateRuntimeFunction(M, FnID);
//   auto *Fn = dyn_cast<llvm::Function>(RTLFn.getCallee());
//   assert(Fn && "Failed to create ARTS runtime function pointer");
//   return Fn;
// }

// void ARTSIRBuilder::initializeEDT(EDT &E) {
//   LLVM_DEBUG(dbgs() << TAG << "Initializing EDT\n");
//   auto *EntryBB = E.getEntry();
//   EntryBB->setName("edt.entry");
//   auto *ExitBB = E.getExit();
//   ExitBB->setName("edt.exit");
//   /// Change EDT Task body function signature
//   auto *EdtBody = E.getTaskBody();
//   (EdtBody->arg_begin())->setName("paramc");
//   (EdtBody->arg_begin() + 1)->setName("paramv");
//   (EdtBody->arg_begin() + 2)->setName("depc");
//   (EdtBody->arg_begin() + 3)->setName("depv");
// }

// void ARTSIRBuilder::insertEDTEntry(EDT &E) {
//   LLVM_DEBUG(dbgs() << TAG << "Inserting EDT Entry\n");
//   auto &Env = E.getEnv();
//   auto *EntryBB = E.getEntry();
//   auto *EdtBody = EntryBB->getParent();
//   assert((EdtBody == E.getTaskBody()) &&
//          "Entry BB parent must be the EDT Func");
//   auto OldInsertPoint = Builder.saveIP();
//   if (EntryBB->getTerminator())
//     Builder.SetInsertPoint(EntryBB->getTerminator());
//   else
//     Builder.SetInsertPoint(EntryBB);
//   /// Insert ParamV
//   auto ParamVArg = EdtBody->arg_begin() + 1;
//   for (auto En : enumerate(Env.ParamV)) {
//     unsigned Index = En.index();
//     Value *OriginalVal = En.value();
//     auto ParamVName = (OriginalVal->getName() == "")
//                           ? ("paramv." + std::to_string(Index))
//                           : ("paramv." + OriginalVal->getName());
//     Value *ParamVElemPtr =
//         Builder.CreateConstInBoundsGEP1_64(Int64, ParamVArg, Index, ParamVName);
//     /// Load the value from the array
//     Value *LoadedVal = Builder.CreateLoad(Int64, ParamVElemPtr);
//     /// Cast the value to the original type
//     Value *CastedVal = LoadedVal;
//     auto *OriginalType = OriginalVal->getType();
//     switch (OriginalType->getTypeID()) {
//     /// Integer
//     case llvm::Type::IntegerTyID: {
//       if (OriginalType != Int64)
//         CastedVal = Builder.CreateTrunc(LoadedVal, OriginalType);
//     } break;
//     /// Float
//     case llvm::Type::FloatTyID: {
//       assert(false && "Float type not supported yet");
//     } break;
//     /// Pointer
//     case llvm::Type::PointerTyID: {
//       assert(false && "Pointer type not supported yet");
//       break;
//     }
//     default:
//       assert(false && "Type not supported yet");
//       break;
//     }

//     E.addLiveIn(OriginalVal, CastedVal);
//   }

//   /// Insert DepV
//   auto DepVArg = EdtBody->arg_begin() + 3;
//   for (auto En : enumerate(Env.DepV)) {
//     unsigned Index = En.index();
//     Value *OriginalVal = En.value();
//     auto DepVName = (OriginalVal->getName() == "")
//                         ? ("depv." + std::to_string(Index))
//                         : ("depv." + std::string(OriginalVal->getName()));
//     Value *DepVArrayElemPtr =
//         Builder.CreateConstInBoundsGEP2_32(EdtDep, DepVArg, Index, 2, DepVName);
//     Value *CastedVal =
//         Builder.CreateBitCast(DepVArrayElemPtr, OriginalVal->getType());
//     E.addLiveIn(OriginalVal, CastedVal);
//     /// Cast to Instruction
//     auto *CastedInst = dyn_cast<Instruction>(CastedVal);
//     auto *OriginalInst = dyn_cast<Instruction>(OriginalVal);
//     if (CastedInst && OriginalInst)
//       E.addLiveOut(OriginalInst, CastedInst);
//   }

//   redirectTo(EntryBB, E.getExit());
//   Builder.restoreIP(OldInsertPoint);
// }

// CallInst *ARTSIRBuilder::insertEDTCall(EDT &E) {
//   LLVM_DEBUG(dbgs() << TAG << "Inserting EDT Call\n");
//   auto *EdtBody = E.getTaskBody();
//   auto &EdtEnv = E.getEnv();
//   const auto EdtName = E.getName();
//   /// Guid
//   reserveEDTGuid(E);
//   /// ParamC
//   AllocaInst *ParamC =
//       Builder.CreateAlloca(Int32, nullptr, EdtName + "_paramc");
//   Builder.CreateStore(ConstantInt::get(Int32, EdtEnv.getParamC()), ParamC);
//   auto *LoadedParamC = Builder.CreateLoad(Int32, ParamC);
//   /// ParamV
//   AllocaInst *ParamV =
//       Builder.CreateAlloca(Int64, LoadedParamC, EdtName + "_paramv");
//   for (auto En : enumerate(EdtEnv.ParamV)) {
//     unsigned Index = En.index();
//     Value *Val = En.value();
//     auto ParamVName = (Val->getName() == "")
//                           ? (EdtName + "_paramv." + std::to_string(Index))
//                           : (EdtName + "_paramv." + Val->getName());
//     /// Create the GEP to store the value in the ParamV array
//     Value *ParamVElemPtr =
//         Builder.CreateConstInBoundsGEP1_64(Int64, ParamV, Index, ParamVName);
//     /// Cast the value to int64
//     Value *CastedVal = nullptr;
//     auto *ValType = Val->getType();
//     switch (ValType->getTypeID()) {
//     /// Integer
//     case llvm::Type::IntegerTyID: {
//       if (ValType != Int64)
//         CastedVal = Builder.CreateSExtOrTrunc(Val, Int64);
//       else
//         CastedVal = Val;
//     } break;
//     /// Default
//     default:
//       assert(false && "Type not supported yet");
//       break;
//     }
//     Builder.CreateStore(CastedVal, ParamVElemPtr);
//   }
//   /// Depc
//   AllocaInst *DepC = Builder.CreateAlloca(Int32, nullptr);
//   Builder.CreateStore(ConstantInt::get(Int32, EdtEnv.getDepC()), DepC);
//   Value *Args[] = {Builder.CreateBitCast(EdtBody, EdtFunctionPtr),
//                    Builder.CreateBitCast(E.getGuidAddr(), Int32Ptr),
//                    LoadedParamC, ParamV,
//                    //  Builder.CreateBitCast(ParamV, Int64Ptr),
//                    Builder.CreateLoad(Int32, DepC)};
//   Function *F = getOrCreateRuntimeFunctionPtr(ARTSRTL_artsEdtCreateWithGuid);
//   return Builder.CreateCall(F, Args);
// }

// void ARTSIRBuilder::reserveEDTGuid(EDT &E) {
//   const auto Node = 0;
//   /// Create the call to reserve the GUID
//   ConstantInt *ARTS_EDT_Enum =
//       ConstantInt::get(Builder.getContext(), APInt(32, 1));
//   Value *Args[] = {
//       ARTS_EDT_Enum,
//       Builder.CreateIntCast(ConstantInt::get(Int32, Node), Int32, false)};
//   CallInst *ReserveGuidCall = Builder.CreateCall(
//       getOrCreateRuntimeFunctionPtr(ARTSRTL_artsReserveGuidRoute), Args);
//   /// Create allocation of the GUID
//   E.GuidAddr =
//       Builder.CreateAlloca(Int32Ptr, nullptr, E.getName() + "_guid.addr");
//   /// Store the GUID
//   Builder.CreateStore(ReserveGuidCall, E.GuidAddr);
// }

// /// ---------------------------- Utils ---------------------------- ///
// void ARTSIRBuilder::redirectTo(BasicBlock *Source, BasicBlock *Target) {
//   if (Instruction *Term = Source->getTerminator()) {
//     auto *Br = cast<BranchInst>(Term);
//     assert(!Br->isConditional() &&
//            "BB's terminator must be an unconditional branch (or degenerate)");
//     BasicBlock *Succ = Br->getSuccessor(0);
//     Succ->removePredecessor(Source, /*KeepOneInputPHIs=*/true);
//     Br->setSuccessor(0, Target);
//     return;
//   }
//   /// Create unconditional branch
//   BranchInst::Create(Target, Source);
// }

// void ARTSIRBuilder::redirectTo(Function *Source, BasicBlock *Target) {
//   for (auto &SourceBB : *Source)
//     redirectTo(&SourceBB, Target);
// }

// void ARTSIRBuilder::redirectEntryAndExit(EDT &E, BasicBlock *OriginalEntry) {
//   /// Redirect Entry
//   auto *ClonedEntry = E.getCloneOfOriginalBasicBlock(OriginalEntry);
//   ClonedEntry->setName("edt.body");
//   E.setBody(ClonedEntry);
//   redirectTo(E.getEntry(), ClonedEntry);
//   /// Redirect Exit
//   auto OriginalParent = OriginalEntry->getParent();
//   for (auto &BB : *OriginalParent) {
//     auto *Terminator = BB.getTerminator();
//     if (Terminator->getNumSuccessors() == 0) {
//       auto *ClonedBB = E.getCloneOfOriginalBasicBlock(&BB);
//       ClonedBB->getTerminator()->eraseFromParent();
//       redirectTo(ClonedBB, E.getExit());
//     }
//   }
// }

// void ARTSIRBuilder::setInsertPoint(BasicBlock *BB) {
//   Builder.SetInsertPoint(BB);
// }

// void ARTSIRBuilder::setInsertPoint(Instruction *I) {
//   Builder.SetInsertPoint(I);
// }

// /// ---------------------------- Private ---------------------------- ///
// void ARTSIRBuilder::initializeTypes() {
//   LLVMContext &Ctx = M.getContext();
//   StructType *T;
// #define ARTS_TYPE(VarName, InitValue) VarName = InitValue;
// #define ARTS_ARRAY_TYPE(VarName, ElemTy, ArraySize)                            \
//   VarName##Ty = ArrayType::get(ElemTy, ArraySize);                             \
//   VarName##PtrTy = PointerType::getUnqual(VarName##Ty);
// #define ARTS_FUNCTION_TYPE(VarName, IsVarArg, ReturnType, ...)                 \
//   VarName = FunctionType::get(ReturnType, {__VA_ARGS__}, IsVarArg);            \
//   VarName##Ptr = PointerType::getUnqual(VarName);
// #define ARTS_STRUCT_TYPE(VarName, StructName, Packed, ...)                     \
//   T = StructType::getTypeByName(Ctx, StructName);                              \
//   if (!T)                                                                      \
//     T = StructType::create(Ctx, {__VA_ARGS__}, StructName, Packed);            \
//   VarName = T;                                                                 \
//   VarName##Ptr = PointerType::getUnqual(T);
// #include "ARTSKinds.def"
// }
