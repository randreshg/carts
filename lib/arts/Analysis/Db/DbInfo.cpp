///==========================================================================
/// File: DbInfo.cpp
///
/// Base implementation of DbInfo class providing common functionality
/// for datablock allocation and access nodes.
///==========================================================================

#include "arts/Analysis/Db/DbInfo.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "db-info"

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// DbInfo Base Implementation
///===----------------------------------------------------------------------===///

// Static member initialization
unsigned DbInfo::nextGlobalId = 0;

// // Default implementations for
// void DbInfo::analyzeIndexValue(mlir::Value index,
//                                std::set<mlir::arts::ValueOrInt> &mins,
//                                std::set<mlir::arts::ValueOrInt> &maxs) {
//   if (!index)
//     return;

//   // Basic analysis - try to extract constants
//   IntegerAttr constAttr;
//   if (matchPattern(index, m_Constant(&constAttr))) {
//     int64_t value = constAttr.getInt();
//     mins.insert(ValueOrInt(value));
//     maxs.insert(ValueOrInt(value));
//     return;
//   }

//   // For block arguments (like loop induction variables)
//   if (auto bbArg = index.dyn_cast<BlockArgument>()) {
//     Operation *parentOp = bbArg.getOwner()->getParentOp();
//     if (auto forOp = dyn_cast<scf::ForOp>(parentOp)) {
//       if (bbArg == forOp.getInductionVar()) {
//         // Try to get loop bounds
//         IntegerAttr lowerAttr, upperAttr;
//         if (matchPattern(forOp.getLowerBound(), m_Constant(&lowerAttr))) {
//           mins.insert(ValueOrInt(lowerAttr.getInt()));
//         }
//         if (matchPattern(forOp.getUpperBound(), m_Constant(&upperAttr))) {
//           maxs.insert(
//               ValueOrInt(upperAttr.getInt() - 1)); // Exclusive upper bound
//         }
//         return;
//       }
//     }
//   }

//   // Default: add the value itself (symbolic)
//   mins.insert(ValueOrInt(index));
//   maxs.insert(ValueOrInt(index));
// }

// Default implementation for updating region bounds
// void DbInfo::updateRegionBounds(
//     const SmallVector<mlir::arts::ValueOrInt> &newMins,
//     const SmallVector<mlir::arts::ValueOrInt> &newMaxs) {
//   // Resize if necessary
//   if (dimMin.size() < newMins.size()) {
//     dimMin.resize(newMins.size());
//     dimMax.resize(newMins.size());
//   }

//   // Update bounds
//   for (size_t i = 0; i < newMins.size() && i < dimMin.size(); ++i) {
//     dimMin[i] = newMins[i];
//     dimMax[i] = (i < newMaxs.size()) ? newMaxs[i] : newMins[i];
//   }
// }

// Default implementation for collecting uses
void DbInfo::collectUses() {
  /// Walk through users of the operation's results
  for (auto result : op->getResults()) {
    for (auto *user : result.getUsers()) {
      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        loads.push_back(loadOp);
      } else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        stores.push_back(storeOp);
      }
    }
  }
}

void DbInfo::setAccessType() {
  bool hasReads = !loads.empty();
  bool hasWrites = !stores.empty();

  if (hasReads && hasWrites)
    accessType = AccessType::ReadWrite;
  else if (hasReads)
    accessType = AccessType::Read;
  else if (hasWrites)
    accessType = AccessType::Write;
  else
    accessType = AccessType::Unknown;
}

void DbInfo::setMemoryLayout() {
  if (isAllocFlag) {
    /// For allocation nodes, the layout is the full buffer
    layout.offset = 0;
    layout.stride = 1;
    auto memrefType = getResultType().cast<MemRefType>();
    if (memrefType.hasStaticShape()) {
      uint64_t elementSize = getElementTypeSize() / 8;
      if (elementSize == 0)
        elementSize = 1; // Avoid div by zero
      layout.size = memrefType.getNumElements() * elementSize;
      layout.isStatic = true;
    } else {
      layout.size = -1;
      layout.isStatic = false;
    }
    return;
  }

  /// For access nodes, extract layout from the operation
  if (auto subviewOp = dyn_cast<memref::SubViewOp>(op)) {
    auto getConstant = [&](Value v) -> std::optional<int64_t> {
      if (auto c = v.getDefiningOp<arith::ConstantOp>()) {
        if (auto i = c.getValue().dyn_cast<IntegerAttr>()) {
          return i.getInt();
        }
      }
      return std::nullopt;
    };

    bool allStatic = true;
    if (!subviewOp.getOffsets().empty()) {
      offsets.push_back(subviewOp.getOffsets()[0]);
      if (auto val = getConstant(offsets[0]))
        layout.offset = *val;
      else
        allStatic = false;
    }
    if (!subviewOp.getSizes().empty()) {
      sizes.push_back(subviewOp.getSizes()[0]);
      if (auto val = getConstant(sizes[0]))
        layout.size = *val;
      else
        allStatic = false;
    }
    if (!subviewOp.getStrides().empty()) {
      strides.push_back(subviewOp.getStrides()[0]);
      if (auto val = getConstant(strides[0]))
        layout.stride = *val;
      else
        allStatic = false;
    }
    layout.isStatic = allStatic;
  }
}

// Default print implementation
void DbInfo::print(llvm::raw_ostream &os) const {
  // os << "DbInfo #" << id << " (GUID: " << guid << ")";
  if (isAllocFlag) {
    os << " [Allocation]";
  } else {
    os << " [Access]";
  }
  os << "\n";

  if (!loads.empty()) {
    os << "  Loads: " << loads.size() << "\n";
  }
  if (!stores.empty()) {
    os << "  Stores: " << stores.size() << "\n";
  }
}