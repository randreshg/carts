///==========================================================================///
/// File: CodeletDataflowGraph.h
///
/// In-memory codelet dependence graph over isolated `codir.codelet` nodes.
///
/// Nodes are codelets; an edge pairs a writer dependency and a reader
/// dependency that resolve to the same memref root (view ops stripped). Edges
/// are order-agnostic existence relations (writer x reader for a shared root),
/// not a program-order single-writer chain: codelets can sit in timestep loops
/// where the last writer feeds the first reader of the next iteration, so a
/// "nearest prior writer" model would be wrong. Each edge carries the producer
/// and consumer dependency slots (with their access modes); the movement family
/// already committed on an edge is the consumer dependency's `dep_collectives`
/// entry, so a CODIR isolation optimization reads movement off the edge's
/// consumer slot instead of re-deriving it.
///
/// Pure analysis: it reads the IR and never mutates it.
///==========================================================================///

#ifndef CARTS_DIALECT_CODIR_UTILS_CODELETDATAFLOWGRAPH_H
#define CARTS_DIALECT_CODIR_UTILS_CODELETDATAFLOWGRAPH_H

#include "carts/dialect/codir/IR/CodirDialect.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir::carts::codir {

class CodeletDataflowGraph {
public:
  /// A single dependency slot of a codelet, with its access mode.
  struct DepRef {
    CodeletOp codelet;
    unsigned depIndex;
    CodirAccessMode mode;
  };

  /// A producer(write) -> consumer(read) edge over a shared memref root. The
  /// edge's committed movement family, when any, is `consumer`'s
  /// `dep_collectives` entry.
  struct Edge {
    DepRef producer;
    DepRef consumer;
    Value root;
  };

  /// Build the graph over every `codir.codelet` nested in `scope`.
  explicit CodeletDataflowGraph(Operation *scope);

  ArrayRef<CodeletOp> nodes() const { return nodes_; }
  ArrayRef<Edge> edges() const { return edges_; }

  /// All dependency slots (writers and readers) whose stripped root is `root`,
  /// or null when no codelet dependency resolves to it.
  const SmallVector<DepRef> *depsForRoot(Value root) const;

private:
  SmallVector<CodeletOp> nodes_;
  llvm::MapVector<Value, SmallVector<DepRef>> usesByRoot_;
  SmallVector<Edge> edges_;
};

} // namespace mlir::carts::codir

#endif // CARTS_DIALECT_CODIR_UTILS_CODELETDATAFLOWGRAPH_H
