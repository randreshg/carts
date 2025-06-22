//===----------------------------------------------------------------------===//
// DbEdge.cpp - Implementation of db graph edge classes
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Db/Graph/DbEdge.h"
#include "arts/Analysis/Db/Graph/DbNode.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "db-graph"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

namespace mlir {
namespace arts {
//===----------------------------------------------------------------------===//
// DbDepType Implementation
//===----------------------------------------------------------------------===//
StringRef toString(DbDepType type) {
  switch (type) {
  case DbDepType::Read:
    return "Read";
  case DbDepType::Write:
    return "Write";
  case DbDepType::ReadWrite:
    return "ReadWrite";
  }
}

//===----------------------------------------------------------------------===//
// DbDepEdge Implementation
//===----------------------------------------------------------------------===//
DbDepEdge::DbDepEdge(DbDepNode *from, DbDepNode *to, DbDepType type)
    : from(from), to(to), type(type) {
  assert(from && "Source node cannot be null");
  assert(to && "Destination node cannot be null");
}

void DbDepEdge::print() {
  LLVM_DEBUG({
    dbgs() << "DB DEPENDENCE EDGE:\n"
           << "  - From: Dep #" << from->getHierId() << "  - To: Dep #"
           << to->getHierId() << "  - Type: " << toString(type) << "\n";
  });
}

//===----------------------------------------------------------------------===//
// DbAllocEdge Implementation
//===----------------------------------------------------------------------===//
DbAllocEdge::DbAllocEdge(DbAllocNode *from, DbAllocNode *to)
    : from(from), to(to) {
  assert(from && "Source node cannot be null");
  assert(to && "Destination node cannot be null");
}

void DbAllocEdge::print() {
  LLVM_DEBUG(dbgs() << "DB ALLOCATION EDGE:\n"
                    << "  - From: Alloc #" << from->getHierId() << "\n"
                    << "  - To: Alloc #" << to->getHierId() << "\n"
                    << "\n");
}

} // namespace arts
} // namespace mlir