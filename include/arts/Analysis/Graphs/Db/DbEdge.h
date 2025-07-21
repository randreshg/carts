//===----------------------------------------------------------------------===//
// Db/DbEdge.h - Db-specific edges
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBEDGE_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBEDGE_H

#include "arts/Analysis/Graphs/Base/EdgeBase.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace mlir {
namespace arts {

class DbAllocEdge : public EdgeBase {
public:
  DbAllocEdge(NodeBase *from, NodeBase *to);

  NodeBase *getFrom() const override { return from; }
  NodeBase *getTo() const override { return to; }
  StringRef getType() const override { return "Alloc"; }
  void print(llvm::raw_ostream &os) const override;

private:
  NodeBase *from;
  NodeBase *to;
};

class DbLifetimeEdge : public EdgeBase {
public:
  DbLifetimeEdge(NodeBase *from, NodeBase *to, StringRef lifetimeType);

  NodeBase *getFrom() const override { return from; }
  NodeBase *getTo() const override { return to; }
  StringRef getType() const override { return lifetimeTypeStr; }
  void print(llvm::raw_ostream &os) const override;

private:
  NodeBase *from;
  NodeBase *to;
  std::string lifetimeTypeStr;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBEDGE_H