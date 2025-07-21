//===----------------------------------------------------------------------===//
// Edt/EdtEdge.h - EDT-specific edges
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_EDT_EDTEDGE_H
#define ARTS_ANALYSIS_GRAPHS_EDT_EDTEDGE_H

#include "arts/Analysis/Graphs/Base/EdgeBase.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {

class EdtDepEdge : public EdgeBase {
public:
  EdtDepEdge(NodeBase *from, NodeBase *to, NodeBase *dbNode);

  NodeBase *getFrom() const override { return from; }
  NodeBase *getTo() const override { return to; }
  StringRef getType() const override { return dbNode->getHierId(); }
  void print(llvm::raw_ostream &os) const override;

private:
  NodeBase *from;
  NodeBase *to;
  NodeBase *dbNode;  // DbGraph node causing dependency
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_EDT_EDTEDGE_H