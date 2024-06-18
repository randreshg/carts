
#include "carts/utils/ARTSMetadata.h"
#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "llvm/Support/Debug.h"
#include <string>

/// DEBUG
#define DEBUG_TYPE "arts-metadata"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// Metadata
namespace arts {
using namespace llvm;
using namespace arts::types;

EDTMetadata *EDTMetadata::getEDT(Function &Fn) {
  // LLVMContext &C = Fn.getContext();
  // if (Fn.hasFnAttribute(ARTS::Parallel))
  //   return new ParallelEDTMetadata(Fn);
  // SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
  // Fn.getAllMetadata(MDs);
  // for (auto &MD : MDs) {
  //   if (MDNode *N = MD.second) {
  //     Constant *val = dyn_cast<ConstantAsMetadata>(
  //                         dyn_cast<MDNode>(N->getOperand(0))->getOperand(0))
  //                         ->getValue();
  //     errs() << "Total instructions in function " << Fn.getName() << " - "
  //            << cast<ConstantInt>(val)->getSExtValue() << "\n";
  //   }
  // }
  // for (auto I = inst_begin(Fn), E = inst_end(Fn); I != E; ++I) {
  //   // showing different way of accessing metadata
  //   if (MDNode *N = (*I).getMetadata("stats.instNumber")) {
  //     errs() << cast<MDString>(N->getOperand(0))->getString() << "\n";
  //   }
  // }
  return nullptr;
}

/// ------------------------------------------------------------------- ///
///                           EDT ARG METADATA                          ///
/// ------------------------------------------------------------------- ///
EDTArgMetadata::EDTArgMetadata(Value &V) {
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT Arg Metadata object for value: "
                    << V.getName() << "\n");
  /// Get CARTS Metadata for the value
}

EDTArgMetadata::~EDTArgMetadata() {}

string EDTArgMetadata::getName(EDTArgType Ty) {
  switch (Ty) {
  case EDTArgType::Dep:
    return EDTDepArgMetadata::getName();
  case EDTArgType::Param:
    return EDTParamArgMetadata::getName();
  default:
    return "";
  }
}

/// EDTDepsArgMetadata
EDTDepArgMetadata::EDTDepArgMetadata(Value &V) : EDTArgMetadata(V) {
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT Dep Arg Metadata object for value: "
                    << V.getName() << "\n");
}
EDTDepArgMetadata::~EDTDepArgMetadata() {}

string EDTDepArgMetadata::getName() {
  return ("edt.arg." + toString(EDTArgType::Dep)).str();
}

/// EDTParamArgMetadata
EDTParamArgMetadata::EDTParamArgMetadata(Value &V) : EDTArgMetadata(V) {
  LLVM_DEBUG(dbgs() << TAG
                    << "Creating EDT Param Arg Metadata object for value: "
                    << V.getName() << "\n");
}

EDTParamArgMetadata::~EDTParamArgMetadata() {}

string EDTParamArgMetadata::getName() {
  return ("edt.arg." + toString(EDTArgType::Param)).str();
}

/// ------------------------------------------------------------------- ///
///                             EDT METADATA                            ///
/// ------------------------------------------------------------------- ///
EDTMetadata::EDTMetadata(Function &Fn) : Fn(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT Metadata object for function: "
                    << Fn.getName() << "\n");
  /// Get CARTS Metadata for the function
}

EDTMetadata::~EDTMetadata() {}

string EDTMetadata::getName(EDTType Ty) {
  switch (Ty) {
  case EDTType::Parallel:
    return ParallelEDTMetadata::getName();
  case EDTType::Task:
    return TaskEDTMetadata::getName();
  case EDTType::Main:
    return MainEDTMetadata::getName();
  default:
    return "";
  }
}

void EDTMetadata::setMetadata(Function &Fn, EDTIRBuilder &Builder) {
  LLVMContext &Ctx = Fn.getContext();
  /// Metadata Nodes for Argument Values
  SmallVector<Metadata *, 16> ArgMDs;
  for (auto *CallArg : Builder.CallArgs) {
    EDTArgType ArgTy = Builder.CallArgTypeMap[CallArg];
    std::string MDStr = EDTArgMetadata::getName(ArgTy);
    ArgMDs.push_back(MDString::get(Ctx, MDStr));
  }
  MDNode *ArgNode = MDNode::get(Ctx, ArgMDs);
  /// Metadata Node for EDT
  SmallVector<Metadata *, 16> EDTMDs;
  string EDTTyStr = EDTMetadata::getName(Builder.Ty);
  EDTMDs.push_back(MDString::get(Ctx, EDTTyStr));
  EDTMDs.push_back(ArgNode);
  /// Set specific metadata for the function
  /// TODO: If it is parallel, add the number of threads...
  Fn.setMetadata("carts.metadata", MDNode::get(Ctx, EDTMDs));
}

/// ParallelEDTMetadata
string ParallelEDTMetadata::getName() {
  return ("edt." + toString(EDTType::Parallel)).str();
}

/// TaskEDTMetadata
string TaskEDTMetadata::getName() {
  return ("edt." + toString(EDTType::Task)).str();
}

/// MainEDTMetadata
string MainEDTMetadata::getName() {
  return ("edt." + toString(EDTType::Main)).str();
}

} // namespace arts