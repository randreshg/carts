///==========================================================================///
/// File: EdtEdge.h
/// Defines EDT-specific edges for graph analysis.
///==========================================================================///

#ifndef ARTS_ANALYSIS_GRAPHS_EDT_EDTEDGE_H
#define ARTS_ANALYSIS_GRAPHS_EDT_EDTEDGE_H

#include "arts/Analysis/Graphs/Base/EdgeBase.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/ArtsDialect.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <cstdint>

namespace mlir {
namespace arts {

class DbAllocNode;
class DbAcquireNode;

struct DbAcquireUsage {
  DbAllocOp alloc;
  DbAcquireOp acquire;
  DbAllocNode *allocNode = nullptr;
  DbAcquireNode *acquireNode = nullptr;
  ArtsMode mode = ArtsMode::in;
  uint64_t estimatedBytes = 0;
  std::string label;
  SmallVector<LoopNode *, 4> loops;
};

struct DbEdgeSlice {
  DbAcquireUsage producer;
  DbAcquireUsage consumer;
  DbDepType depType = DbDepType::RAW;
  std::string description;
};

class EdtDepEdge : public EdgeBase {
public:
  EdtDepEdge(NodeBase *from, NodeBase *to, const DbEdgeSlice &slice);

  NodeBase *getFrom() const override { return from; }
  NodeBase *getTo() const override { return to; }
  EdgeKind getKind() const override { return EdgeKind::Dep; }
  StringRef getType() const override { return typeLabel; }
  void print(llvm::raw_ostream &os) const override;

  ArrayRef<DbEdgeSlice> getSlices() const { return dbSlices; }
  void appendSlice(const DbEdgeSlice &slice) { dbSlices.push_back(slice); }

  static bool classof(const EdgeBase *E) {
    return E->getKind() == EdgeKind::Dep;
  }

private:
  NodeBase *from;
  NodeBase *to;
  SmallVector<DbEdgeSlice, 2> dbSlices;
  std::string typeLabel;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_EDT_EDTEDGE_H
