#ifndef ARTS_ANALYSIS_DB_GRAPH_DBEDGE_H
#define ARTS_ANALYSIS_DB_GRAPH_DBEDGE_H

#include "mlir/IR/BuiltinOps.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>



namespace mlir {
namespace arts {

/// Forward declarations
class DbAccessNode;
class DbAllocNode;
enum class DbDepType { Read, Write, ReadWrite };
StringRef toString(DbDepType type);

/// DbDepEdge
class DbDepEdge {
public:
  DbDepEdge(DbAccessNode *from, DbAccessNode *to, DbDepType type);
  void print();

  DbAccessNode *getFrom() const { return from; }
  DbAccessNode *getTo() const { return to; }
  DbDepType getType() const { return type; }

  DbAccessNode *from;
  DbAccessNode *to;
  DbDepType type;
};

/// DbAllocEdge
class DbAllocEdge {
public:
  DbAllocEdge(DbAllocNode *from, DbAllocNode *to);
  void print();

  DbAllocNode *getFrom() const { return from; }
  DbAllocNode *getTo() const { return to; }

  DbAllocNode *from;
  DbAllocNode *to;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_GRAPH_DBEDGE_H