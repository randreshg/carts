
#include "llvm/Support/Debug.h"

#include "carts/analysis/ARTS.h"
#include "carts/utils/ARTSUtils.h"
#include "carts/utils/ARTSMetadata.h"

using namespace llvm;

/// DEBUG
#define DEBUG_TYPE "arts-metadata"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// Metadata
namespace arts {
namespace metadata {

/// EDTMetadata
EDTMetadata::EDTMetadata(Function &F) {
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT Metadata for function: " << F.getName() << "\n");
  /// Get CARTS Metadata for the function
  
}

EDTMetadata::~EDTMetadata() {
  LLVM_DEBUG(dbgs() << TAG << "Destroying EDT Metadata\n");
}



} // namespace metadata
} // namespace arts