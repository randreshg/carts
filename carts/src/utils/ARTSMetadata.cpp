
#include "carts/utils/ARTSMetadata.h"
#include "carts/analysis/ARTS.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

/// DEBUG
#define DEBUG_TYPE "arts-metadata"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// Metadata
namespace arts {
namespace metadata {
using namespace arts::types;

EDTMetadata *EDTMetadata::getEDT(Function &F) {
  LLVMContext &C = F.getContext();
  // if (F.hasFnAttribute(ARTS::Parallel))
  //   return new ParallelEDTMetadata(F);
  // SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
  // F.getAllMetadata(MDs);
  // for (auto &MD : MDs) {
  //   if (MDNode *N = MD.second) {
  //     Constant *val = dyn_cast<ConstantAsMetadata>(
  //                         dyn_cast<MDNode>(N->getOperand(0))->getOperand(0))
  //                         ->getValue();
  //     errs() << "Total instructions in function " << F.getName() << " - "
  //            << cast<ConstantInt>(val)->getSExtValue() << "\n";
  //   }
  // }
  // for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
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

EDTArgMetadata::~EDTArgMetadata() {
  // LLVM_DEBUG(dbgs() << TAG << "Destroying EDT Arg Metadata\n");
}

StringRef EDTArgMetadata::getName(EDTArgType Ty) {
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

StringRef EDTDepArgMetadata::getName() { return "edt." + toString(EDTArgType::Dep); }

/// EDTParamArgMetadata
EDTParamArgMetadata::EDTParamArgMetadata(Value &V) : EDTArgMetadata(V) {
  LLVM_DEBUG(dbgs() << TAG
                    << "Creating EDT Param Arg Metadata object for value: "
                    << V.getName() << "\n");
}

EDTParamArgMetadata::~EDTParamArgMetadata() {}

StringRef EDTParamArgMetadata::getName() { return toString(EDTArgType::Param); }

/// ------------------------------------------------------------------- ///
///                             EDT METADATA                            ///
/// ------------------------------------------------------------------- ///
EDTMetadata::EDTMetadata(Function &F) {
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT Metadata object for function: "
                    << F.getName() << "\n");
  /// Get CARTS Metadata for the function
}

EDTMetadata::~EDTMetadata() {}

} // namespace metadata
} // namespace arts