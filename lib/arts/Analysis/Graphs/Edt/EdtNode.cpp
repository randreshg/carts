//===----------------------------------------------------------------------===//
// Edt/EdtNode.cpp - Implementation of EDT nodes
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Graphs/Edt/EdtNode.h"

using namespace mlir::arts;

EdtTaskNode::EdtTaskNode(EdtOp op) : edtOp(op) {}

void EdtTaskNode::print(llvm::raw_ostream &os) const {
  os << "EdtTaskNode (" << getHierId() << ")\n";
}