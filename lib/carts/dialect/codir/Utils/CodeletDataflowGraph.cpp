///==========================================================================///
/// File: CodeletDataflowGraph.cpp
///==========================================================================///
#include "carts/dialect/codir/Utils/CodeletDataflowGraph.h"
#include "carts/dialect/codir/Utils/CodeletABIUtils.h"
#include "carts/utils/ValueAnalysis.h"

namespace mlir::carts::codir {

static bool modeWrites(CodirAccessMode mode) {
  return mode == CodirAccessMode::write || mode == CodirAccessMode::readwrite;
}
static bool modeReads(CodirAccessMode mode) {
  return mode == CodirAccessMode::read || mode == CodirAccessMode::readwrite;
}

CodeletDataflowGraph::CodeletDataflowGraph(Operation *scope) {
  // Index every codelet dependency by the memref root it resolves to.
  scope->walk([&](CodeletOp codelet) {
    nodes_.push_back(codelet);
    for (auto [index, dep] : llvm::enumerate(codelet.getDeps())) {
      auto depIndex = static_cast<unsigned>(index);
      std::optional<CodirAccessMode> mode = getDepAccessMode(codelet, depIndex);
      if (!mode)
        continue;
      Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep);
      usesByRoot_[root].push_back(DepRef{codelet, depIndex, *mode});
    }
  });

  // Form writer x reader edges for each shared root.
  for (auto &entry : usesByRoot_) {
    for (const DepRef &producer : entry.second) {
      if (!modeWrites(producer.mode))
        continue;
      for (const DepRef &consumer : entry.second) {
        if (!modeReads(consumer.mode) || consumer.codelet == producer.codelet)
          continue;
        edges_.push_back(Edge{producer, consumer, entry.first});
      }
    }
  }
}

const SmallVector<CodeletDataflowGraph::DepRef> *
CodeletDataflowGraph::depsForRoot(Value root) const {
  auto it = usesByRoot_.find(root);
  return it == usesByRoot_.end() ? nullptr : &it->second;
}

} // namespace mlir::carts::codir
