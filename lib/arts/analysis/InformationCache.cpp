///==========================================================================///
/// File: InformationCache.cpp
///
/// Pre-scanned module-level fact cache to eliminate redundant walks.
///==========================================================================///

#include "arts/analysis/InformationCache.h"

using namespace mlir;
using namespace mlir::arts;

const SmallVector<EdtOp> InformationCache::emptyEdts;
const SmallVector<DbAllocOp> InformationCache::emptyAllocs;
const SmallVector<ForOp> InformationCache::emptyLoops;
const SmallVector<DbAcquireOp> InformationCache::emptyAcquires;

void InformationCache::build(ModuleOp module) {
  invalidate();

  module.walk([&](func::FuncOp func) {
    functions.push_back(func);
    auto &edts = edtsPerFunction[func];
    auto &allocs = allocsPerFunction[func];
    auto &loops = loopsPerFunction[func];
    auto &acquires = acquiresPerFunction[func];

    func.walk([&](Operation *op) {
      if (auto edt = dyn_cast<EdtOp>(op))
        edts.push_back(edt);
      else if (auto alloc = dyn_cast<DbAllocOp>(op))
        allocs.push_back(alloc);
      else if (auto loop = dyn_cast<ForOp>(op))
        loops.push_back(loop);
      else if (auto acquire = dyn_cast<DbAcquireOp>(op))
        acquires.push_back(acquire);
    });
  });

  built = true;
}

void InformationCache::invalidate() {
  built = false;
  functions.clear();
  edtsPerFunction.clear();
  allocsPerFunction.clear();
  loopsPerFunction.clear();
  acquiresPerFunction.clear();
}

ArrayRef<EdtOp>
InformationCache::getEdtsInFunction(func::FuncOp func) const {
  auto it = edtsPerFunction.find(func);
  if (it != edtsPerFunction.end())
    return it->second;
  return emptyEdts;
}

ArrayRef<DbAllocOp>
InformationCache::getAllocsInFunction(func::FuncOp func) const {
  auto it = allocsPerFunction.find(func);
  if (it != allocsPerFunction.end())
    return it->second;
  return emptyAllocs;
}

ArrayRef<ForOp>
InformationCache::getLoopsInFunction(func::FuncOp func) const {
  auto it = loopsPerFunction.find(func);
  if (it != loopsPerFunction.end())
    return it->second;
  return emptyLoops;
}

ArrayRef<DbAcquireOp>
InformationCache::getAcquiresInFunction(func::FuncOp func) const {
  auto it = acquiresPerFunction.find(func);
  if (it != acquiresPerFunction.end())
    return it->second;
  return emptyAcquires;
}

size_t InformationCache::getTotalEdtCount() const {
  size_t total = 0;
  for (const auto &entry : edtsPerFunction)
    total += entry.second.size();
  return total;
}

size_t InformationCache::getTotalAllocCount() const {
  size_t total = 0;
  for (const auto &entry : allocsPerFunction)
    total += entry.second.size();
  return total;
}
