///==========================================================================
/// File: DbEdge.cpp
/// Implementation of Db edges for graph analysis.
///==========================================================================

#include "arts/Analysis/Graphs/Db/DbEdge.h"

using namespace mlir::arts;

DbAllocEdge::DbAllocEdge(NodeBase *from, NodeBase *to) : from(from), to(to) {
  assert(from && "Source node cannot be null");
  assert(to && "Destination node cannot be null");
}

void DbAllocEdge::print(llvm::raw_ostream &os) const {
  os << "DB ALLOCATION EDGE:\n"
     << "  - From: " << from->getHierId() << "\n"
     << "  - To: " << to->getHierId() << "\n";
}

DbLifetimeEdge::DbLifetimeEdge(NodeBase *from, NodeBase *to)
    : from(from), to(to) {
  assert(from && "Source node cannot be null");
  assert(to && "Destination node cannot be null");
}

void DbLifetimeEdge::print(llvm::raw_ostream &os) const {
  os << "DB LIFETIME EDGE:\n"
     << "  - From: " << from->getHierId() << "  - To: " << to->getHierId()
     << "  - Type: " << getType() << "\n";
}