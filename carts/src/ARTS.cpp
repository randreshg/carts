
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Local.h"

#include "ARTS.h"
#include "ARTSUtils.h"

using namespace llvm;
using namespace arts;
using namespace arts_utils;

/// DEBUG
#define DEBUG_TYPE "arts"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// EDT Environment
void EDTEnvironment::insertParamV(Value *V) { ParamV.insert(V); }
void EDTEnvironment::insertDepV(Value *V) { DepV.insert(V); }
uint32_t EDTEnvironment::getParamC() { return ParamV.size(); }
uint32_t EDTEnvironment::getDepC() { return DepV.size(); }

/// EDT
void EDT::removeDeadInstructions() {
  LLVM_DEBUG(dbgs() << TAG << "Deleting dead instructions in EDT: "
                    << F->getName() << "\n");
  SmallVector<Value *, 16> ValuesToRemove;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      /// If the instruction is trivially dead, remove it
      if (isInstructionTriviallyDead(&I)) {
        LLVM_DEBUG(dbgs() << TAG << "- Removing: " << I << "\n");
        ValuesToRemove.push_back(&I);
        continue;
      }
      /// However, if the instruction is not trivially dead, check its operands
      /// If any of them is undef, remove the instruction
      for (auto &Op : I.operands()) {
        if (isa<UndefValue>(Op.get())) {
          LLVM_DEBUG(dbgs() << TAG << "- Removing: " << I << "\n");
          ValuesToRemove.push_back(&I);
          break;
        }
      }
    }
  }
  removeValues(ValuesToRemove);
}

void EDT::insertValueToEnv(Value *Val) {
  /// Pointer is a depv, else, it is a paramv
  if (PointerType *PT = dyn_cast<PointerType>(Val->getType()))
    Env.insertDepV(Val);
  else
    Env.insertParamV(Val);

  /// TODO: Add extra info to OpenMP Frontend to have info about
  /// Data Sharing attributes
}

void EDT::insertValueToEnv(Value *Val, bool IsDepV) {
  /// Pointer is a depv, else, it is a paramv
  if (IsDepV)
    Env.insertDepV(Val);
  else
    Env.insertParamV(Val);
}

void EDT::cloneAndAddBasicBlocks(Function *F) {
  assert((Ty == Type::TASK) || (Ty == Type::PARALLEL) ||
         (Ty == Type::LOOP) && "Only tasks, parallel and loop can be cloned");
  for (auto &BB : *F)
    cloneAndAddBasicBlock(&BB);
}

void EDT::cloneAndAddBasicBlocks(BlockSequence &BBs) {
  for (auto BB : BBs)
    cloneAndAddBasicBlock(BB);
}

/// EDT Graph
EDTGraph::EDTGraph(Module &M) : M(M) {}

EDTGraphNode *CallGraph::getEntryNode(void) const {
  Function *f = M.getFunction("main");
  return this->getFunctionNode(f);
}

EDTGraphNode *CallGraph::getFunctionNode(Function *f) const {
  if (this->functions.find(f) == this->functions.end()) {
    return nullptr;
  }
  auto n = this->functions.at(f);

  return n;
}
