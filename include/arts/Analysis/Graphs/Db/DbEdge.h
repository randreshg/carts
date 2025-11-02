///==========================================================================
/// File: DbEdge.h
/// Defines Db-specific edges for graph analysis.
///==========================================================================

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBEDGE_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBEDGE_H

#include "arts/Analysis/Graphs/Base/EdgeBase.h"
#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {

/// Child edge: parent-child relationship in DB hierarchy
class DbChildEdge : public EdgeBase {
public:
  DbChildEdge(NodeBase *from, NodeBase *to);

  NodeBase *getFrom() const override { return from; }
  NodeBase *getTo() const override { return to; }
  EdgeKind getKind() const override { return EdgeKind::Child; }
  llvm::StringRef getType() const override { return "Child"; }
  void print(llvm::raw_ostream &os) const override;

  static bool classof(const EdgeBase *E) {
    return E->getKind() == EdgeKind::Child;
  }

private:
  NodeBase *from;
  NodeBase *to;
};

/// Dependency edge between DB acquires
enum class DbDepType { RAW, WAR, WAW, RAR };

class DbDepEdge : public EdgeBase {
public:
  /// Access pattern information for this dependency
  struct AccessInfo {
    llvm::SmallVector<int64_t> sourceOffsets;
    llvm::SmallVector<int64_t> sourceSizes;
    llvm::SmallVector<int64_t> sinkOffsets;
    llvm::SmallVector<int64_t> sinkSizes;
    /// Proven disjoint by alias analysis
    bool isDisjoint = false;
  };

  DbDepEdge(NodeBase *from, NodeBase *to, DbDepType depType);

  NodeBase *getFrom() const override { return from; }
  NodeBase *getTo() const override { return to; }
  EdgeKind getKind() const override { return EdgeKind::Dep; }
  llvm::StringRef getType() const override { return typeStr; }
  void print(llvm::raw_ostream &os) const override;

  /// Get the dependency type
  DbDepType getDepType() const { return depType; }

  /// Check dependency classification
  bool isAntiDep() const { return depType == DbDepType::WAR; }
  bool isFlowDep() const { return depType == DbDepType::RAW; }
  bool isOutputDep() const { return depType == DbDepType::WAW; }
  bool isAffinityDep() const { return depType == DbDepType::RAR; }

  /// Access to pattern information
  AccessInfo &getAccessInfo() { return accessInfo; }
  const AccessInfo &getAccessInfo() const { return accessInfo; }

  static bool classof(const EdgeBase *E) {
    return E->getKind() == EdgeKind::Dep;
  }

private:
  static llvm::StringRef toString(DbDepType t);
  NodeBase *from;
  NodeBase *to;
  DbDepType depType;
  llvm::StringRef typeStr;
  AccessInfo accessInfo;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBEDGE_H