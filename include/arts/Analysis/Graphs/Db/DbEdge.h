///==========================================================================
/// File: DbEdge.h
/// Defines Db-specific edges for graph analysis.
///==========================================================================

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBEDGE_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBEDGE_H

#include "arts/Analysis/Graphs/Base/EdgeBase.h"
#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {

class DbAllocEdge : public EdgeBase {
public:
  DbAllocEdge(NodeBase *from, NodeBase *to);

  NodeBase *getFrom() const override { return from; }
  NodeBase *getTo() const override { return to; }
  EdgeKind getKind() const override { return EdgeKind::Alloc; }
  llvm::StringRef getType() const override { return "Alloc"; }
  void print(llvm::raw_ostream &os) const override;

  static bool classof(const EdgeBase *E) {
    return E->getKind() == EdgeKind::Alloc;
  }

private:
  NodeBase *from;
  NodeBase *to;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBEDGE_H