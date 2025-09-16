///==========================================================================
/// File: DbGraph.cpp
/// Implementation of DbGraph for database operation analysis.
///==========================================================================
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Db/DbInfo.h"
#include "arts/Analysis/Graphs/Db/DbEdge.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/LoopAnalysis.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Analysis/DataFlowFramework.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <string>
using namespace mlir;
using namespace mlir::arts;

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_analysis);

std::string DbGraph::generateAllocId(unsigned id) {
  std::string s;
  if (id == 0)
    return "A";
  unsigned n = id;
  while (n > 0) {
    unsigned rem = (n - 1) % 26;
    s = char('A' + rem) + s;
    n = (n - 1) / 26;
  }
  return s;
}

std::string DbGraph::getFunctionName() const {
  return const_cast<func::FuncOp &>(func).getNameAttr().getValue().str();
}

DbGraph::DbGraph(func::FuncOp func, DbAnalysis *analysis)
    : func(func), analysis(analysis) {
  assert(analysis && "Analysis cannot be null");
  ARTS_INFO("Creating db graph for function: " << func.getName().str());
}

DbGraph::~DbGraph() = default;

/// Build the complete database graph by collecting nodes, building
/// dependencies, computing operation order, and calculating metrics.
void DbGraph::build() {
  if (isBuilt && !needsRebuild)
    return;
  ARTS_INFO("Building db graph");
  invalidate();
  collectNodes();
  buildDependencies();
  computeOpOrder();
  computeMetrics();
  /// computeReuseColoring();
  isBuilt = true;
  needsRebuild = false;
}

void DbGraph::buildNodesOnly() {
  ARTS_INFO("Building db graph (nodes only)");
  invalidate();
  collectNodes();
  isBuilt = true;
  needsRebuild = false;
}

void DbGraph::invalidate() {
  allocNodes.clear();
  acquireNodeMap.clear();
  releaseNodeMap.clear();
  edges.clear();
  nodes.clear();
  childrenCache.clear();
  nextAllocId = 1;
  isBuilt = false;
  needsRebuild = true;
}

NodeBase *DbGraph::getEntryNode() const {
  if (!allocNodes.empty())
    return allocNodes.begin()->second.get();
  return nullptr;
}

NodeBase *DbGraph::getOrCreateNode(Operation *op) {
  if (auto allocOp = dyn_cast<DbAllocOp>(op))
    return getOrCreateAllocNode(allocOp);
  if (auto acquireOp = dyn_cast<DbAcquireOp>(op))
    return getOrCreateAcquireNode(acquireOp);
  if (auto releaseOp = dyn_cast<DbReleaseOp>(op))
    return getOrCreateReleaseNode(releaseOp);
  return nullptr;
}

NodeBase *DbGraph::getNode(Operation *op) const {
  if (auto allocOp = dyn_cast<DbAllocOp>(op))
    return getDbAllocNode(allocOp);
  if (auto acquireOp = dyn_cast<DbAcquireOp>(op))
    return getDbAcquireNode(acquireOp);
  if (auto releaseOp = dyn_cast<DbReleaseOp>(op))
    return getDbReleaseNode(releaseOp);
  return nullptr;
}

DbAllocNode *DbGraph::getDbAllocNode(DbAllocOp op) const {
  auto it = allocNodes.find(op);
  return it != allocNodes.end() ? it->second.get() : nullptr;
}

DbAcquireNode *DbGraph::getDbAcquireNode(DbAcquireOp op) const {
  auto it = acquireNodeMap.find(op);
  return it != acquireNodeMap.end() ? it->second : nullptr;
}

DbReleaseNode *DbGraph::getDbReleaseNode(DbReleaseOp op) const {
  auto it = releaseNodeMap.find(op);
  return it != releaseNodeMap.end() ? it->second : nullptr;
}

DbAllocNode *DbGraph::getOrCreateAllocNode(DbAllocOp op) {
  if (auto node = getDbAllocNode(op))
    return node;

  auto newNode = std::make_unique<DbAllocNode>(op, analysis);
  newNode->setHierId(generateAllocId(nextAllocId++));
  DbAllocNode *ptr = newNode.get();
  allocNodes[op] = std::move(newNode);
  nodes.push_back(ptr);
  return ptr;
}

DbAcquireNode *DbGraph::getOrCreateAcquireNode(DbAcquireOp op) {
  if (auto node = getDbAcquireNode(op))
    return node;

  DbAllocOp allocOp =
      dyn_cast_or_null<DbAllocOp>(getUnderlyingOperation(op.getPtr()));
  assert(allocOp && "Expected DbAllocOp for acquire");

  DbAllocNode *allocNode = getOrCreateAllocNode(allocOp);
  DbAcquireNode *acquireNode = allocNode->getOrCreateAcquireNode(op);
  acquireNodeMap[op] = acquireNode;
  nodes.push_back(acquireNode);
  return acquireNode;
}

DbReleaseNode *DbGraph::getOrCreateReleaseNode(DbReleaseOp op) {
  auto it = releaseNodeMap.find(op);
  if (it != releaseNodeMap.end())
    return it->second;

  DbAllocOp allocOp =
      dyn_cast_or_null<DbAllocOp>(getUnderlyingOperation(op.getSources()[0]));
  assert(allocOp && "Expected DbAllocOp for release");

  DbAllocNode *allocNode = getOrCreateAllocNode(allocOp);
  DbReleaseNode *releaseNode = allocNode->getOrCreateReleaseNode(op);
  releaseNodeMap[op] = releaseNode;
  nodes.push_back(releaseNode);
  return releaseNode;
}

void DbGraph::forEachNode(const std::function<void(NodeBase *)> &fn) const {
  for (auto *node : nodes)
    fn(node);
}

bool DbGraph::addEdge(NodeBase *from, NodeBase *to, EdgeBase *edge) {
  auto key = std::make_pair(from, to);
  if (edges.count(key))
    return false;
  edges[key] = std::unique_ptr<EdgeBase>(edge);
  from->addOutEdge(edge);
  to->addInEdge(edge);
  return true;
}

DbAllocOp DbGraph::getParentAlloc(Operation *op) {
  if (auto allocOp = dyn_cast<DbAllocOp>(op))
    return allocOp;
  if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
    DbAllocOp allocOp = dyn_cast_or_null<DbAllocOp>(
        getUnderlyingOperation(acquireOp.getSourcePtr()));
    assert(allocOp && "Expected DbAllocOp for acquire");
    return allocOp;
  }
  if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
    DbAllocOp allocOp = dyn_cast_or_null<DbAllocOp>(
        getUnderlyingOperation(releaseOp.getSources()[0]));
    assert(allocOp && "Expected DbAllocOp for release");
    return allocOp;
  }
  return nullptr;
}

bool DbGraph::mayAlias(DbAllocOp alloc1, DbAllocOp alloc2) {
  DbAllocNode *n1 = getOrCreateAllocNode(alloc1);
  DbAllocNode *n2 = getOrCreateAllocNode(alloc2);
  if (!n1 || !n2)
    return false;
  return analysis->getAliasAnalysis()->mayAlias(*n1, *n2);
}

bool DbGraph::hasAcquireReleasePair(DbAllocOp alloc) {
  DbAllocNode *node = getOrCreateAllocNode(alloc);
  if (!node)
    return false;
  return node->getAcquireNodesSize() == node->getReleaseNodesSize();
}

void DbGraph::print(llvm::raw_ostream &os) const {
  os << "===============================================\n";
  os << "DbGraph for function: " << getFunctionName() << "\n";
  os << "===============================================\n";
  os << "Summary:\n";
  os << "  Allocations: " << allocNodes.size() << "\n";
  os << "  Acquire nodes: " << acquireNodeMap.size() << "\n";
  os << "  Release nodes: " << releaseNodeMap.size() << "\n";
  os << "  Edges: " << edges.size() << "\n\n";

  os << "Allocation Hierarchy:\n";
  for (const auto &pair : allocNodes) {
    DbAllocNode *alloc = pair.second.get();
    os << "Allocation [" << alloc->getHierId() << "]: ";
    alloc->print(os);
    os << "\n";

    alloc->forEachChildNode([&](NodeBase *child) {
      child->print(os);
      os << "\n";
    });
  }
}

/// Walk through the function and collect all database operations
/// (alloc/acquire/release) into graph nodes, creating the basic structure of
/// the graph.
void DbGraph::collectNodes() {
  ARTS_INFO("Phase 1 - Collecting nodes");

  func.walk([&](Operation *op) {
    if (isa<DbAllocOp, DbAcquireOp, DbReleaseOp>(op)) {
      getOrCreateNode(op);
    }
  });

  ARTS_INFO("Collected " << allocNodes.size() << " allocations, "
                         << acquireNodeMap.size() << " acquires, "
                         << releaseNodeMap.size() << " releases");
}

/// Build dependency edges between nodes using dataflow analysis to determine
/// lifetime relationships and data dependencies between database operations.
void DbGraph::buildDependencies() {
  ARTS_INFO("Phase 2 - Building dependencies");
  auto *dataflowAnalysis = analysis->getDataFlowAnalysis();
  assert(dataflowAnalysis &&
         "Dataflow analysis is required to build dependencies");

  dataflowAnalysis->initialize(this, analysis);
  (void)analysis->getSolver().initializeAndRun(func);
  ARTS_INFO("Phase 2 - Data flow analysis finished");
}

void DbGraph::computeOpOrder() {
  unsigned idx = 0;
  func.walk([&](Operation *op) { opOrder[op] = idx++; });
}

/// Compute per-allocation metrics and global summary metrics.
void DbGraph::computeMetrics() {
  peakLiveDbs = 0;
  peakBytes = 0;

  /// Process each allocation to compute its metrics
  for (const auto &pair : allocNodes) {
    computeAllocationMetrics(pair.first, pair.second.get());
  }

  computePeakMetrics();
  computeReuseCandidates();
}

/// Compute metrics for a single allocation
void DbGraph::computeAllocationMetrics(DbAllocOp alloc,
                                       DbAllocNode *allocNode) {
  DbAllocInfo &info = allocNode->getInfoRef();

  /// Initialize basic allocation info
  info.isAlloc = true;
  info.allocIndex = getOrder(alloc.getOperation());
  info.numAcquires = 0;
  info.numReleases = 0;
  info.inCount = 0;
  info.outCount = 0;
  info.totalAccessBytes = 0;
  info.intervals.clear();

  /// Process child nodes to build intervals
  allocNode->forEachChildNode([&](NodeBase *child) {
    if (auto *acq = dyn_cast<DbAcquireNode>(child)) {
      processAcquireNode(acq, info);
    } else if (auto *rel = dyn_cast<DbReleaseNode>(child)) {
      processReleaseNode(rel, info);
    }
  });

  /// Sort intervals and compute derived metrics
  llvm::sort(info.intervals,
             [](const DbAcquireInfo &a, const DbAcquireInfo &b) {
               return a.beginIndex < b.beginIndex;
             });

  computeAllocationDerivedMetrics(alloc, info);
}

/// Process an acquire node and add its interval
void DbGraph::processAcquireNode(DbAcquireNode *acq, DbAllocInfo &info) {
  /// Find matching release node
  DbReleaseNode *matchedRelease = findMatchingRelease(acq);

  /// Create interval
  info.intervals.emplace_back();
  DbAcquireInfo &interval = info.intervals.back();
  interval.acquire = cast<DbAcquireOp>(acq->getOp());
  interval.beginIndex = getOrder(interval.acquire.getOperation());

  if (matchedRelease) {
    interval.release = cast<DbReleaseOp>(matchedRelease->getOp());
    interval.endIndex = getOrder(interval.release.getOperation());
  } else {
    interval.release = nullptr;
    interval.endIndex = interval.beginIndex; /// Unknown; treat as minimal
  }

  /// Copy information from acquire node
  const auto &nodeInfo = acq->getInfoRef();
  acq->getInfoRef().beginIndex = interval.beginIndex;
  acq->getInfoRef().endIndex = interval.endIndex;

  interval.offsets = nodeInfo.offsets;
  interval.sizes = nodeInfo.sizes;
  interval.pinned = nodeInfo.pinned;
  interval.constOffsets = nodeInfo.constOffsets;
  interval.constSizes = nodeInfo.constSizes;
  interval.estimatedBytes = nodeInfo.estimatedBytes;
  interval.inCount = nodeInfo.inCount;
  interval.outCount = nodeInfo.outCount;

  /// Update allocation totals
  ++info.numAcquires;
  info.inCount += interval.inCount;
  info.outCount += interval.outCount;
  info.totalAccessBytes += interval.estimatedBytes;
}

/// Process a release node
void DbGraph::processReleaseNode(DbReleaseNode *rel, DbAllocInfo &info) {
  ++info.numReleases;
  auto releaseOp = cast<DbReleaseOp>(rel->getOp());
  rel->getInfoRef().releaseIndex = getOrder(releaseOp.getOperation());
}

/// Find matching release node for an acquire node
DbReleaseNode *DbGraph::findMatchingRelease(DbAcquireNode *acq) {
  for (auto *edge : acq->getOutEdges()) {
    if (auto *life = dyn_cast<DbLifetimeEdge>(edge)) {
      return dyn_cast<DbReleaseNode>(life->getTo());
    }
  }
  return nullptr;
}

/// Compute derived metrics for an allocation
void DbGraph::computeAllocationDerivedMetrics(DbAllocOp alloc,
                                              DbAllocInfo &info) {
  /// Compute end index (last release)
  unsigned lastEnd = info.allocIndex;
  for (auto &interval : info.intervals) {
    lastEnd = std::max(lastEnd, interval.endIndex);
  }
  info.endIndex = lastEnd;

  /// Compute critical span
  computeCriticalSpan(info);

  /// Compute critical path
  computeCriticalPath(info);

  /// Compute loop depth
  computeLoopDepth(info);

  /// Determine if long-lived
  computeLongLivedFlag(info);

  /// Check if escaping
  computeEscapingFlag(alloc, info);
}

/// Compute critical span for an allocation
void DbGraph::computeCriticalSpan(DbAllocInfo &info) {
  info.criticalSpan = 0;
  if (info.intervals.empty())
    return;

  unsigned minBegin = info.intervals.front().beginIndex;
  unsigned maxEnd = info.intervals.front().endIndex;

  for (auto &interval : info.intervals) {
    minBegin = std::min(minBegin, interval.beginIndex);
    maxEnd = std::max(maxEnd, interval.endIndex);
  }

  info.criticalSpan = maxEnd >= minBegin ? (maxEnd - minBegin + 1) : 0;
}

/// Compute critical path for an allocation
void DbGraph::computeCriticalPath(DbAllocInfo &info) {
  SmallVector<std::pair<unsigned, unsigned>, 8> segments;
  for (auto &interval : info.intervals) {
    segments.emplace_back(interval.beginIndex, interval.endIndex);
  }

  llvm::sort(segments, [](auto a, auto b) {
    return a.first < b.first || (a.first == b.first && a.second < b.second);
  });

  unsigned currentStart = 0, currentEnd = 0;
  bool initialized = false;
  info.criticalPath = 0;

  for (auto [start, end] : segments) {
    if (!initialized) {
      currentStart = start;
      currentEnd = end;
      initialized = true;
      continue;
    }

    if (start <= currentEnd) {
      currentEnd = std::max(currentEnd, end);
    } else {
      info.criticalPath += (currentEnd - currentStart + 1);
      currentStart = start;
      currentEnd = end;
    }
  }

  if (initialized) {
    info.criticalPath += (currentEnd - currentStart + 1);
  }
}

/// Compute maximum loop depth for an allocation
void DbGraph::computeLoopDepth(DbAllocInfo &info) {
  if (auto *loopAnalysis = analysis->getLoopAnalysis()) {
    unsigned maxDepth = 0;
    for (auto &interval : info.intervals) {
      if (!interval.acquire)
        continue;

      SmallVector<LoopInfo *, 4> enclosingLoops;
      loopAnalysis->collectEnclosingLoops(interval.acquire.getOperation(),
                                          enclosingLoops);
      maxDepth = std::max<unsigned>(maxDepth, enclosingLoops.size());
    }
    info.maxLoopDepth = maxDepth;
  }
}

/// Determine if allocation is long-lived
void DbGraph::computeLongLivedFlag(DbAllocInfo &info) {
  unsigned firstIdx = info.allocIndex;
  unsigned lastIdx = opOrder.empty() ? 0u : (unsigned)opOrder.size() - 1;

  if (lastIdx > 0) {
    double coverage =
        (double)(info.endIndex - firstIdx + 1) / (double)(lastIdx + 1);
    info.isLongLived = coverage > 0.7;
  }
}

/// Determine if allocation may be escaping
void DbGraph::computeEscapingFlag(DbAllocOp alloc, DbAllocInfo &info) {
  info.maybeEscaping = false;
  for (Operation *user : alloc.getPtr().getUsers()) {
    if (!isa<DbAcquireOp>(user) && !isa<DbReleaseOp>(user)) {
      info.maybeEscaping = true;
      break;
    }
  }
}

/// Compute peak live databases and bytes using sweep-line algorithm
void DbGraph::computePeakMetrics() {
  struct Event {
    unsigned idx;
    int delta;
    uint64_t bytes;
  };

  SmallVector<Event, 32> events;
  for (auto &kv : allocNodes) {
    const DbAllocInfo &info = kv.second->getInfoRef();
    for (auto &interval : info.intervals) {
      events.push_back({interval.beginIndex, +1, info.staticBytes});
      events.push_back({interval.endIndex + 1, -1, info.staticBytes});
    }
  }

  llvm::sort(events,
             [](const Event &a, const Event &b) { return a.idx < b.idx; });

  int liveCount = 0;
  uint64_t liveBytes = 0;
  for (auto &event : events) {
    liveCount += event.delta;
    if (event.delta > 0) {
      liveBytes += event.bytes;
    } else {
      liveBytes -= event.bytes;
    }
    peakLiveDbs = std::max<uint64_t>(peakLiveDbs, (uint64_t)liveCount);
    peakBytes = std::max<uint64_t>(peakBytes, liveBytes);
  }
}

/// Compute reuse candidates for allocations
void DbGraph::computeReuseCandidates() {
  SmallVector<DbAllocOp, 16> allocs;
  for (auto &kv : allocNodes) {
    allocs.push_back(kv.first);
  }

  llvm::sort(allocs, [&](DbAllocOp a, DbAllocOp b) {
    return getOrder(a.getOperation()) < getOrder(b.getOperation());
  });

  for (size_t i = 0; i < allocs.size(); ++i) {
    for (size_t j = i + 1; j < allocs.size(); ++j) {
      const auto &infoA = allocNodes[allocs[i]]->getInfoRef();
      const auto &infoB = allocNodes[allocs[j]]->getInfoRef();

      /// Check if lifetimes don't overlap and sizes match
      bool noOverlap = (infoA.endIndex <= infoB.allocIndex ||
                        infoB.endIndex <= infoA.allocIndex);
      bool sameSize =
          (infoA.staticBytes != 0 && infoA.staticBytes == infoB.staticBytes);

      if (noOverlap && sameSize) {
        allocNodes[allocs[i]]->getInfoRef().reuseMatches.push_back(allocs[j]);
        allocNodes[allocs[j]]->getInfoRef().reuseMatches.push_back(allocs[i]);
      }
    }
  }
}

void DbGraph::computeReuseColoring() {
  SmallVector<DbAllocOp, 16> allocs;
  for (auto &kv : allocNodes)
    allocs.push_back(kv.first);

  /// Sort by descending weight: bigger and longer-lived first
  llvm::sort(allocs, [&](DbAllocOp allocA, DbAllocOp allocB) {
    const auto &infoA = allocNodes[allocA]->getInfoRef();
    const auto &infoB = allocNodes[allocB]->getInfoRef();
    uint64_t weightA =
        std::max<uint64_t>(infoA.staticBytes, infoA.totalAccessBytes) *
        (1 + infoA.criticalPath);
    uint64_t weightB =
        std::max<uint64_t>(infoB.staticBytes, infoB.totalAccessBytes) *
        (1 + infoB.criticalPath);
    if (weightA != weightB)
      return weightA > weightB;
    return infoA.allocIndex < infoB.allocIndex;
  });

  auto interferes = [&](DbAllocOp allocX, DbAllocOp allocY) -> bool {
    const auto &infoX = allocNodes[allocX]->getInfoRef();
    const auto &infoY = allocNodes[allocY]->getInfoRef();
    bool lifetimeOverlap = !(infoX.endIndex < infoY.allocIndex ||
                             infoY.endIndex < infoX.allocIndex);
    if (lifetimeOverlap)
      return true;
    return mayAlias(allocX, allocY);
  };

  llvm::SmallDenseMap<DbAllocOp, unsigned> colorMap;
  for (DbAllocOp alloc : allocs) {
    llvm::SmallDenseSet<unsigned> usedColors;
    for (auto &entry : colorMap) {
      DbAllocOp otherAlloc = entry.first;
      if (interferes(alloc, otherAlloc))
        usedColors.insert(entry.second);
    }

    unsigned color = 0;
    while (usedColors.count(color))
      ++color;

    colorMap[alloc] = color;
    allocNodes[alloc]->getInfoRef().reuseColor = color;
  }
}

void DbGraph::exportToJson(llvm::raw_ostream &os, bool includeAnalysis) const {
  using namespace llvm::json;

  auto edgeKindStr = [](EdgeBase::EdgeKind k) -> const char * {
    switch (k) {
    case EdgeBase::EdgeKind::Alloc:
      return "Alloc";
    case EdgeBase::EdgeKind::Lifetime:
      return "Lifetime";
    case EdgeBase::EdgeKind::Dep:
      return "Dep";
    }
    return "Unknown";
  };

  Object root;

  /// Always include structural information
  Array nodesArr;
  forEachNode([&](NodeBase *node) {
    const char *nkind = "DbNode";
    if (isa<DbAllocNode>(node))
      nkind = "DbAlloc";
    else if (isa<DbAcquireNode>(node))
      nkind = "DbAcquire";
    else if (isa<DbReleaseNode>(node))
      nkind = "DbRelease";
    nodesArr.push_back(Object{{"id", sanitizeString(node->getHierId())},
                              {"group", "db"},
                              {"nodeKind", nkind}});
  });

  Array edgesArr;
  forEachNode([&](NodeBase *from) {
    for (auto *edge : from->getOutEdges()) {
      edgesArr.push_back(
          Object{{"from", sanitizeString(edge->getFrom()->getHierId())},
                 {"to", sanitizeString(edge->getTo()->getHierId())},
                 {"edgeKind", edgeKindStr(edge->getKind())},
                 {"label", edge->getType().str()}});
    }
  });

  root["nodes"] = std::move(nodesArr);
  root["edges"] = std::move(edgesArr);

  /// Include analysis data if requested
  if (includeAnalysis) {
    auto modeToString = [](ArtsMode m) -> std::string {
      switch (m) {
      case ArtsMode::in:
        return "in";
      case ArtsMode::out:
        return "out";
      default:
        return "inout";
      }
    };

    /// Summary info
    root["info"] = Object{
        {"peakLiveDbs", (double)peakLiveDbs},
        {"peakBytes", (double)peakBytes},
    };

    /// Allocs
    Array allocs;
    for (const auto &kv : allocNodes) {
      DbAllocOp alloc = kv.first;
      const DbAllocInfo &M = kv.second->getInfoRef();
      Object A;
      A["id"] = sanitizeString(getNode(alloc.getOperation())->getHierId());
      A["allocIndex"] = (double)M.allocIndex;
      A["endIndex"] = (double)M.endIndex;
      A["numAcquires"] = (double)M.numAcquires;
      A["numReleases"] = (double)M.numReleases;
      A["staticBytes"] = (double)M.staticBytes;
      A["criticalSpan"] = (double)M.criticalSpan;
      A["criticalPath"] = (double)M.criticalPath;
      A["totalAccessBytes"] = (double)M.totalAccessBytes;
      /// Preserve numeric 0/1 booleans as in prior output
      A["isLongLived"] = (double)(M.isLongLived ? 1 : 0);
      A["maybeEscaping"] = (double)(M.maybeEscaping ? 1 : 0);

      Array intervals;
      for (const auto &iv : M.intervals) {
        Object I;
        I["begin"] = (double)iv.beginIndex;
        I["end"] = (double)iv.endIndex;
        I["mode"] =
            modeToString(const_cast<DbAcquireOp &>(iv.acquire).getMode());
        Array offs;
        for (auto v : iv.constOffsets)
          offs.push_back((double)v);
        I["constOffsets"] = std::move(offs);
        Array sizes;
        for (auto v : iv.constSizes)
          sizes.push_back((double)v);
        I["constSizes"] = std::move(sizes);
        I["estimatedBytes"] = (double)iv.estimatedBytes;
        intervals.push_back(std::move(I));
      }
      A["intervals"] = std::move(intervals);
      allocs.push_back(std::move(A));
    }
    root["allocs"] = std::move(allocs);

    /// Reuse candidates (deduplicated)
    Array reuse;
    for (const auto &kv : allocNodes) {
      DbAllocOp Aop = kv.first;
      const auto &AI = kv.second->getInfoRef();
      for (DbAllocOp Bop : AI.reuseMatches) {
        if (getOrder(Aop.getOperation()) < getOrder(Bop.getOperation())) {
          Object P;
          P["a"] = sanitizeString(getNode(Aop.getOperation())->getHierId());
          P["b"] = sanitizeString(getNode(Bop.getOperation())->getHierId());
          reuse.push_back(std::move(P));
        }
      }
    }
    root["reuseCandidates"] = std::move(reuse);

    /// Reuse coloring groups
    llvm::DenseMap<unsigned, SmallVector<DbAllocOp, 8>> groups;
    for (auto &kv : allocNodes)
      groups[kv.second->getInfoRef().reuseColor].push_back(kv.first);

    Object coloring;
    for (auto &cg : groups) {
      Array ids;
      for (auto Aop : cg.second)
        ids.push_back(sanitizeString(getNode(Aop.getOperation())->getHierId()));
      coloring[std::to_string(cg.first)] = std::move(ids);
    }
    root["reuseColoring"] = std::move(coloring);
  }

  os << llvm::json::Value(std::move(root)) << "\n";
}

/// Helper to populate and sort children cache for a node
void DbGraph::populateChildrenCache(NodeBase *node) {
  auto &vec = childrenCache[node];
  vec.clear();

  /// Collect children from the node's outgoing edges
  for (auto *edge : node->getOutEdges()) {
    vec.push_back(edge->getTo());
  }

  /// Sort by operation order
  llvm::sort(vec, [&](NodeBase *a, NodeBase *b) {
    Operation *opA = a ? a->getOp() : nullptr;
    Operation *opB = b ? b->getOp() : nullptr;
    return getOrder(opA) < getOrder(opB);
  });
}

GraphBase::ChildIterator DbGraph::childBegin(NodeBase *node) {
  populateChildrenCache(node);
  return childrenCache[node].begin();
}

GraphBase::ChildIterator DbGraph::childEnd(NodeBase *node) {
  auto it = childrenCache.find(node);
  if (it == childrenCache.end()) {
    populateChildrenCache(node);
    return childrenCache[node].end();
  }
  return it->second.end();
}