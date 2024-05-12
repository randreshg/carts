
#include "ARTS.h"
#include "ARTSAnalyzer.h"
#include "ARTSIRBuilder.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
using namespace arts;

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
void EDT::insertValueToEnv(Value *Val) {
  /// Pointer is a depv, else, it is a paramv
  if (PointerType *PT = dyn_cast<PointerType>(Val->getType()))
    Env.insertDepV(Val);
  else
    Env.insertParamV(Val);

  /// TODO: Add extra info to OpenMP Frontend to have info about
  /// Data Sharing attributes
}

void EDT::cloneAndAddBasicBlocks(Function *F) {
  assert((Ty == Type::TASK) || (Ty == Type::PARALLEL) ||
         (Ty == Type::LOOP) && "Only tasks, parallel and loop can be cloned");
  for (auto &BB : *F)
    cloneAndAddBasicBlock(&BB);
}