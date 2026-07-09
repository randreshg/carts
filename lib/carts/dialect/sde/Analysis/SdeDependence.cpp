///==========================================================================///
/// File: SdeDependence.cpp
///
/// Phase B substrate: memref dependence distance vectors.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SdeDependence.h"

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/utils/ValueAnalysis.h"
#include "llvm/ADT/STLExtras.h"

namespace mlir::carts::sde {
namespace {

static Value stripRoot(Value value) {
  return ::mlir::carts::ValueAnalysis::stripMemrefViewOps(value);
}

static bool sameLoopDim(const AffineDimOffset &lhs,
                        const AffineDimOffset &rhs) {
  if (!lhs.dim || !rhs.dim)
    return !lhs.dim && !rhs.dim;
  return *lhs.dim == *rhs.dim;
}

static std::optional<SdeSelfDependence>
deriveSelfDependence(Value root, const AccessRelation &read,
                     const AccessRelation &write) {
  if (read.subscripts.empty() || read.subscripts.size() != write.subscripts.size())
    return std::nullopt;

  SdeSelfDependence dep;
  dep.root = root;
  dep.minDistance.reserve(read.subscripts.size());
  dep.maxDistance.reserve(read.subscripts.size());

  bool sawCarried = false;
  for (auto [readSubscript, writeSubscript] :
       llvm::zip_equal(read.subscripts, write.subscripts)) {
    if (!sameLoopDim(readSubscript, writeSubscript))
      return std::nullopt;
    int64_t distance = readSubscript.offset - writeSubscript.offset;
    dep.minDistance.push_back(distance);
    dep.maxDistance.push_back(distance);
    sawCarried |= distance != 0;
  }
  if (!sawCarried)
    return std::nullopt;
  return dep;
}

} // namespace

bool SdeSelfDependence::isLoopCarried() const {
  for (auto [minValue, maxValue] : llvm::zip_equal(minDistance, maxDistance))
    if (minValue != 0 || maxValue != 0)
      return true;
  return false;
}

SdeDependence::SdeDependence(Operation *op) : operation(op) {
  compute();
}

void SdeDependence::compute() {
  selfDependencesBySu.clear();
  if (!operation)
    return;

  operation->walk([&](SdeSuIterateOp su) {
    std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(su);
    if (!summary)
      return;

    SmallVector<AccessRelation, 4> reads;
    SmallVector<AccessRelation, 4> writes;
    for (const MemrefAccessEntry &entry : summary->reads) {
      Value root = stripRoot(entry.memref);
      reads.push_back(makeAccessRelation(root, entry, LayoutGraphAccessKind::read));
    }
    for (const MemrefAccessEntry &entry : summary->writes) {
      Value root = stripRoot(entry.memref);
      writes.push_back(
          makeAccessRelation(root, entry, LayoutGraphAccessKind::write));
    }

    SmallVector<SdeSelfDependence, 2> deps;
    for (const AccessRelation &read : reads) {
      for (const AccessRelation &write : writes) {
        if (read.root != write.root)
          continue;
        if (std::optional<SdeSelfDependence> dep =
                deriveSelfDependence(read.root, read, write))
          deps.push_back(std::move(*dep));
      }
    }
    if (!deps.empty())
      selfDependencesBySu[su.getOperation()] = std::move(deps);
  });
}

bool SdeDependence::isInvalidated(
    const AnalysisManager::PreservedAnalyses &pa) {
  return !pa.isPreserved<SdeDependence>();
}

ArrayRef<SdeSelfDependence>
SdeDependence::getSelfDependences(SdeSuIterateOp op) const {
  auto it = selfDependencesBySu.find(op.getOperation());
  if (it == selfDependencesBySu.end())
    return {};
  return it->second;
}

bool SdeDependence::hasLoopCarriedSelfDependence(SdeSuIterateOp op) const {
  return llvm::any_of(getSelfDependences(op),
                      [](const SdeSelfDependence &dep) {
                        return dep.isLoopCarried();
                      });
}

} // namespace mlir::carts::sde
