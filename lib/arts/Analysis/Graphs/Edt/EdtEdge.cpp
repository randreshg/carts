///==========================================================================///
/// File: EdtEdge.cpp
/// Implementation of EDT edges for graph analysis.
///==========================================================================///

#include "arts/Analysis/Graphs/Edt/EdtEdge.h"
#include "arts/Analysis/Graphs/Base/NodeBase.h"

using namespace mlir::arts;

EdtDepEdge::EdtDepEdge(NodeBase *from, NodeBase *to, const DbEdgeSlice &slice)
    : from(from), to(to) {
  assert(from && "Source node cannot be null");
  assert(to && "Destination node cannot be null");
  dbSlices.push_back(slice);
  if (!slice.description.empty())
    typeLabel = slice.description;
  else if (!slice.producer.label.empty())
    typeLabel = slice.producer.label;
  else
    typeLabel = "db";
}

void EdtDepEdge::print(llvm::raw_ostream &os) const {
  os << "EDT DEPENDENCY EDGE:\n"
     << "  - From: " << from->getHierId() << "\n"
     << "  - To: " << to->getHierId() << "\n";
  for (const auto &slice : dbSlices) {
    os << "  - Dep: " << slice.description << " [";
    switch (slice.depType) {
    case DbDepType::RAW:
      os << "RAW";
      break;
    case DbDepType::WAR:
      os << "WAR";
      break;
    case DbDepType::WAW:
      os << "WAW";
      break;
    case DbDepType::RAR:
      os << "RAR";
      break;
    }
    os << "]\n";
  }
}
