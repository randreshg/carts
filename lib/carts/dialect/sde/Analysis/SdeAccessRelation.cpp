///==========================================================================///
/// File: SdeAccessRelation.cpp
///
/// Phase B substrate: access relations on affine-form SDE IR.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SdeAccessRelation.h"

#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/IR/BuiltinOps.h"

namespace mlir::carts::sde {

SdeAccessRelation::SdeAccessRelation(Operation *op) : operation(op) {
  compute();
}

void SdeAccessRelation::compute() {
  relationsByRoot.clear();
  if (!operation)
    return;

  auto walkSu = [&](SdeSuIterateOp su) {
    std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(su);
    if (!summary)
      return;

    auto record = [&](const MemrefAccessEntry &entry,
                      LayoutGraphAccessKind kind) {
      Value root = ValueAnalysis::stripMemrefViewOps(entry.memref);
      relationsByRoot[root].push_back(makeAccessRelation(root, entry, kind));
    };

    for (const MemrefAccessEntry &entry : summary->reads)
      record(entry, LayoutGraphAccessKind::read);
    for (const MemrefAccessEntry &entry : summary->writes)
      record(entry, LayoutGraphAccessKind::write);
  };

  if (auto module = dyn_cast<ModuleOp>(operation)) {
    module.walk(walkSu);
    return;
  }
  operation->walk(walkSu);
}

bool SdeAccessRelation::isInvalidated(
    const AnalysisManager::PreservedAnalyses &pa) {
  return !pa.isPreserved<SdeAccessRelation>();
}

ArrayRef<AccessRelation>
SdeAccessRelation::getRelationsForRoot(Value root) const {
  Value stripped = ValueAnalysis::stripMemrefViewOps(root);
  auto it = relationsByRoot.find(stripped);
  if (it == relationsByRoot.end())
    return {};
  return it->second;
}

} // namespace mlir::carts::sde
