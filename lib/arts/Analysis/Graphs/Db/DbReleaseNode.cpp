///==========================================================================
/// DbReleaseNode.cpp - Implementation of DbRelease node
///==========================================================================

#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Db/DbAnalysis.h"

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// DbReleaseNode
//===----------------------------------------------------------------------===//
DbReleaseNode::DbReleaseNode(DbReleaseOp op, DbAllocNode *parent,
                             DbAnalysis *analysis)
    : dbReleaseOp(op), op(op.getOperation()), parent(parent),
      analysis(analysis) {
  info.release = op;
}

void DbReleaseNode::print(llvm::raw_ostream &os) const {
  os << "DbReleaseNode (" << getHierId() << ")\n";
}
