//===----------------------------------------------------------------------===//
// Edt/EdtNode.h - EDT-specific nodes derived from NodeBase
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_EDT_EDTNODE_H
#define ARTS_ANALYSIS_GRAPHS_EDT_EDTNODE_H

#include "arts/Analysis/Graphs/Base/NodeBase.h"
#include "arts/ArtsDialect.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {

class EdtTaskNode : public NodeBase {
public:
  EdtTaskNode(EdtOp op);

  StringRef getHierId() const override { return hierId; }
  void setHierId(std::string id) { hierId = id; }
  void print(llvm::raw_ostream &os) const override;
  Operation *getOp() const override { return opPtr; }

  void addInEdge(EdgeBase *edge) override { inEdges.insert(edge); }
  void addOutEdge(EdgeBase *edge) override { outEdges.insert(edge); }
  const DenseSet<EdgeBase *> &getInEdges() const override { return inEdges; }
  const DenseSet<EdgeBase *> &getOutEdges() const override { return outEdges; }

  EdtOp getEdtOp() const { return edtOp; }

  NodeKind getKind() const override { return NodeKind::EdtTask; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::EdtTask;
  }

private:
  EdtOp edtOp;
  Operation *opPtr = nullptr;
  std::string hierId;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_EDT_EDTNODE_H