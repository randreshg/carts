//===----------------------------------------------------------------------===//
// Db/DbGraph.cpp - Implementation of DbGraph
//===----------------------------------------------------------------------===//
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbEdge.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/LoopAnalysis.h"
#include "mlir/Analysis/DataFlowFramework.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <string>
using namespace mlir;
using namespace mlir::arts;

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_graph);

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

std::string DbGraph::sanitizeForDot(StringRef s) const {
  std::string id = s.str();
  std::replace(id.begin(), id.end(), '.', '_');
  std::replace(id.begin(), id.end(), '-', '_');
  return id;
}

std::string DbGraph::getFunctionName() const {
  return const_cast<func::FuncOp &>(func).getNameAttr().getValue().str();
}

DbGraph::DbGraph(func::FuncOp func, DbAnalysis *analysis)
    : func(func), analysis(analysis) {
  ARTS_INFO("Creating db graph for function: " << func.getName().str());
}

DbGraph::~DbGraph() = default;

void DbGraph::build() {
  if (isBuilt && !needsRebuild)
    return;
  ARTS_INFO("Building db graph");
  invalidate();
  collectNodes();
  buildDependencies();
  computeOpOrder();
  computeMetrics();
  computeReuseColoring();
  isBuilt = true;
  needsRebuild = false;
}

void DbGraph::invalidate() {
  allocNodes.clear();
  acquireNodeMap.clear();
  releaseNodeMap.clear();
  edges.clear();
  nodes.clear();
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
  if (auto allocOp = dyn_cast<DbAllocOp>(op)) {
    auto it = allocNodes.find(allocOp);
    if (it != allocNodes.end())
      return it->second.get();
    auto newNode = std::make_unique<DbAllocNode>(allocOp, analysis);
    newNode->setHierId(generateAllocId(nextAllocId++));
    NodeBase *ptr = newNode.get();
    allocNodes[allocOp] = std::move(newNode);
    nodes.push_back(ptr);
    return ptr;
  }
  if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
    DbAllocOp root = findRootAllocOp(acquireOp.getOperation());
    if (!root)
      return nullptr;
    DbAllocNode *allocNode =
        static_cast<DbAllocNode *>(getOrCreateNode(root.getOperation()));
    DbAcquireNode *acquireNode = allocNode->getOrCreateAcquireNode(acquireOp);
    acquireNodeMap[acquireOp] = acquireNode;
    nodes.push_back(acquireNode);
    return acquireNode;
  }
  if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
    DbAllocOp root = findRootAllocOp(releaseOp.getOperation());
    if (!root)
      return nullptr;
    DbAllocNode *allocNode =
        static_cast<DbAllocNode *>(getOrCreateNode(root.getOperation()));
    DbReleaseNode *releaseNode = allocNode->getOrCreateReleaseNode(releaseOp);
    releaseNodeMap[releaseOp] = releaseNode;
    nodes.push_back(releaseNode);
    return releaseNode;
  }
  return nullptr;
}

NodeBase *DbGraph::getNode(Operation *op) const {
  if (auto allocOp = dyn_cast<DbAllocOp>(op)) {
    auto it = allocNodes.find(allocOp);
    return it != allocNodes.end() ? it->second.get() : nullptr;
  }
  if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
    auto it = acquireNodeMap.find(acquireOp);
    return it != acquireNodeMap.end() ? it->second : nullptr;
  }
  if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
    auto it = releaseNodeMap.find(releaseOp);
    return it != releaseNodeMap.end() ? it->second : nullptr;
  }
  return nullptr;
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

bool DbGraph::isAllocReachable(DbAllocOp fromOp, DbAllocOp toOp) {
  DbAllocNode *from =
      static_cast<DbAllocNode *>(getNode(fromOp.getOperation()));
  DbAllocNode *to = static_cast<DbAllocNode *>(getNode(toOp.getOperation()));
  if (!from || !to)
    return false;
  for (auto *edge : from->getOutEdges()) {
    if (edge->getTo() == to && edge->getKind() == EdgeBase::EdgeKind::Alloc)
      return true;
  }
  return false;
}

DbAllocOp DbGraph::getParentAlloc(Operation *op) {
  if (auto allocOp = dyn_cast<DbAllocOp>(op))
    return allocOp;
  if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
    DbAcquireNode *node = static_cast<DbAcquireNode *>(getNode(op));
    if (!node)
      return nullptr;
    return dyn_cast<DbAllocOp>(node->getParent()->getOp());
  }
  if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
    DbReleaseNode *node = static_cast<DbReleaseNode *>(getNode(op));
    if (!node)
      return nullptr;
    return dyn_cast<DbAllocOp>(node->getParent()->getOp());
  }
  return nullptr;
}

bool DbGraph::mayAlias(DbAllocOp alloc1, DbAllocOp alloc2) {
  DbAllocNode *n1 = static_cast<DbAllocNode *>(getNode(alloc1.getOperation()));
  DbAllocNode *n2 = static_cast<DbAllocNode *>(getNode(alloc2.getOperation()));
  if (!n1 || !n2)
    return false;
  return analysis->getAliasAnalysis()->mayAlias(*n1, *n2);
}

bool DbGraph::hasAcquireReleasePair(DbAllocOp alloc) {
  DbAllocNode *node = static_cast<DbAllocNode *>(getNode(alloc.getOperation()));
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

void DbGraph::exportToDot(llvm::raw_ostream &os) const {
  os << "digraph DbGraph {\n";
  os << "  compound=true;\n";
  os << "  node [shape=box];\n";
  os << "  graph [label=\"" << getFunctionName()
     << "\", labelloc=t, fontsize=20];\n\n";

  for (const auto &pair : allocNodes) {
    DbAllocNode *alloc = pair.second.get();
    os << "  subgraph cluster_" << sanitizeForDot(alloc->getHierId()) << " {\n";
    os << "    label = \"Alloc: " << alloc->getHierId() << "\";\n";
    os << "    style=filled;\n";
    os << "    color=lightgrey;\n\n";

    alloc->forEachChildNode([&](NodeBase *child) {
      os << "    " << sanitizeForDot(child->getHierId()) << " [label=\""
         << child->getHierId() << "\"];\n";
    });

    os << "  }\n\n";
  }

  for (const auto &pair : edges) {
    EdgeBase *edge = pair.second.get();
    os << "  " << sanitizeForDot(edge->getFrom()->getHierId()) << " -> "
       << sanitizeForDot(edge->getTo()->getHierId()) << " [label=\""
       << edge->getType() << "\"];\n";
  }

  os << "}\n";
}

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

DbAllocOp DbGraph::findRootAllocOp(Operation *op) {
  Value source;
  if (auto acquireOp = dyn_cast<DbAcquireOp>(op))
    source = acquireOp.getSourcePtr();
  else if (auto releaseOp = dyn_cast<DbReleaseOp>(op))
    source = releaseOp.getSources()[0];
  else
    return nullptr;

  const int maxChainDepth = 10;
  int depth = 0;
  while (depth < maxChainDepth) {
    if (auto allocOp = source.getDefiningOp<DbAllocOp>())
      return allocOp;
    if (auto chainedAcquireOp = source.getDefiningOp<DbAcquireOp>())
      source = chainedAcquireOp.getSourcePtr();
    else if (auto chainedReleaseOp = source.getDefiningOp<DbReleaseOp>())
      source = chainedReleaseOp.getSources()[0];
    else
      break;
    depth++;
  }
  return nullptr;
}

void DbGraph::buildDependencies() {
  ARTS_INFO("Phase 2 - Building dependencies");
  /// Use the dataflow analysis from the analysis instead of directly accessing
  /// the solver
  if (!dataFlowAnalysis) {
    /// Get the dataflow analysis from the analysis
    dataFlowAnalysis = analysis->getDataFlowAnalysis();
    if (dataFlowAnalysis) {
      dataFlowAnalysis->initialize(this, analysis);
    }
  }

  if (dataFlowAnalysis) {
    /// Execute the solver over this function to run the analysis
    (void)analysis->getSolver().initializeAndRun(func);
    ARTS_INFO("Phase 2 - Data flow analysis finished");
  } else {
    ARTS_WARN("No dataflow analysis available, skipping dependency building");
  }
}

void DbGraph::computeOpOrder() {
  unsigned idx = 0;
  func.walk([&](Operation *op) { opOrder[op] = idx++; });
}

uint64_t DbGraph::getElementTypeByteSize(Type elemTy) {
  if (auto intTy = dyn_cast<IntegerType>(elemTy))
    return intTy.getWidth() / 8u;
  if (auto fTy = dyn_cast<FloatType>(elemTy))
    return fTy.getWidth() / 8u;
  return 0; /// unknown
}

bool DbGraph::isConstantIndex(Value v, int64_t &cst) {
  if (auto c = v.getDefiningOp<mlir::arith::ConstantIndexOp>()) {
    cst = c.value();
    return true;
  }
  return false;
}

void DbGraph::computeMetrics() {
  allocMetrics.clear();
  peakLiveDbs = 0;
  peakBytes = 0;
  reuseCandidates.clear();

  /// Gather intervals per alloc
  for (const auto &pair : allocNodes) {
    DbAllocOp alloc = pair.first;
    DbAllocMetrics M;
    M.allocIndex = getOrder(alloc.getOperation());
    M.staticBytes = 0;
    /// Compute static bytes if memref shape is all-constant
    if (auto memTy = dyn_cast<MemRefType>(alloc.getPtr().getType())) {
      if (memTy.hasStaticShape()) {
        uint64_t bytes = getElementTypeByteSize(memTy.getElementType());
        for (int64_t d : memTy.getShape()) {
          uint64_t mul = (uint64_t)std::max<int64_t>(d, 1);
          if (bytes > std::numeric_limits<uint64_t>::max() / mul) {
            bytes = std::numeric_limits<uint64_t>::max();
            break;
          }
          bytes *= mul;
        }
        M.staticBytes = bytes;
      }
    }

    DbAllocNode *allocNode = pair.second.get();
    /// Enumerate children to find intervals
    SmallVector<AcquireInterval, 8> ivs;
    allocNode->forEachChildNode([&](NodeBase *child) {
      if (auto *acq = dyn_cast<DbAcquireNode>(child)) {
        /// Find matching release edge among out edges leading to a release node
        DbReleaseNode *matchedRelease = nullptr;
        for (auto *edge : acq->getOutEdges()) {
          if (auto *life = dyn_cast<DbLifetimeEdge>(edge)) {
            matchedRelease = dyn_cast<DbReleaseNode>(life->getTo());
            break;
          }
        }
        AcquireInterval I;
        I.acquire = cast<DbAcquireOp>(acq->getOp());
        I.beginIndex = getOrder(I.acquire.getOperation());
        if (matchedRelease) {
          I.release = cast<DbReleaseOp>(matchedRelease->getOp());
          I.endIndex = getOrder(I.release.getOperation());
        } else {
          I.release = nullptr;
          I.endIndex = I.beginIndex; /// unknown; treat as minimal
        }
        ivs.push_back(I);
        ++M.numAcquires;
      } else if (auto *rel = dyn_cast<DbReleaseNode>(child)) {
        (void)rel;
        ++M.numReleases;
      }
    });
    /// Sort intervals by begin
    llvm::sort(ivs, [&](const AcquireInterval &a, const AcquireInterval &b) {
      return a.beginIndex < b.beginIndex;
    });
    /// Compute alloc-level end index (last release)
    unsigned lastEnd = M.allocIndex;
    for (auto &iv : ivs)
      lastEnd = std::max(lastEnd, iv.endIndex);
    M.endIndex = lastEnd;
    M.intervals = std::move(ivs);
    /// Approximate per-alloc critical span and total access bytes using
    /// DbAnalysis slice info (best-effort)
    M.criticalSpan = 0;
    if (!M.intervals.empty()) {
      unsigned minB = M.intervals.front().beginIndex;
      unsigned maxE = M.intervals.front().endIndex;
      for (auto &iv : M.intervals) {
        minB = std::min(minB, iv.beginIndex);
        maxE = std::max(maxE, iv.endIndex);
      }
      M.criticalSpan = maxE >= minB ? (maxE - minB + 1) : 0;
    }
    /// Precise critical path (sum of disjoint durations after merging overlaps)
    SmallVector<std::pair<unsigned, unsigned>, 8> segs;
    for (auto &iv : M.intervals)
      segs.emplace_back(iv.beginIndex, iv.endIndex);
    llvm::sort(segs, [](auto a, auto b) {
      return a.first < b.first || (a.first == b.first && a.second < b.second);
    });
    unsigned curS = 0, curE = 0;
    bool initialized = false;
    M.criticalPath = 0;
    for (auto [s, e] : segs) {
      if (!initialized) {
        curS = s;
        curE = e;
        initialized = true;
        continue;
      }
      if (s <= curE) {
        curE = std::max(curE, e);
      } else {
        M.criticalPath += (curE - curS + 1);
        curS = s;
        curE = e;
      }
    }
    if (initialized)
      M.criticalPath += (curE - curS + 1);
    /// Sum estimated bytes per interval (unknown treated as 0)
    M.totalAccessBytes = 0;
    for (auto &iv : M.intervals) {
      if (iv.acquire) {
        auto info = analysis->computeSliceInfo(iv.acquire);
        M.totalAccessBytes += info.estimatedBytes;
      }
    }
    /// Loop-aware depth (best-effort; require LoopAnalysis to be present)
    if (auto *LA = analysis->getLoopAnalysis()) {
      unsigned depthMax = 0;
      for (auto &iv : M.intervals) {
        if (!iv.acquire)
          continue;
        SmallVector<LoopInfo *, 4> enc;
        LA->collectEnclosingLoops(iv.acquire.getOperation(), enc);
        depthMax = std::max<unsigned>(depthMax, enc.size());
      }
      M.maxLoopDepth = depthMax;
    }
    /// Long lived if covers > 70% of function order span
    unsigned firstIdx = M.allocIndex;
    unsigned lastIdx = opOrder.empty() ? 0u : (unsigned)opOrder.size() - 1;
    if (lastIdx > 0) {
      M.isLongLived =
          (double)(M.endIndex - firstIdx + 1) / (double)(lastIdx + 1) > 0.7;
    }
    /// Escaping if the alloc result is returned or passed to unknown calls
    /// (approx)
    M.maybeEscaping = false;
    for (Operation *user : alloc.getPtr().getUsers()) {
      if (!isa<DbAcquireOp>(user) && !isa<DbReleaseOp>(user)) {
        M.maybeEscaping = true;
        break;
      }
    }
    allocMetrics[alloc] = std::move(M);
  }

  /// Sweep-line over intervals for peak
  struct Event {
    unsigned idx;
    int delta;
    uint64_t bytes;
  };
  SmallVector<Event, 32> events;
  for (auto &kv : allocMetrics) {
    const DbAllocMetrics &M = kv.second;
    for (auto &iv : M.intervals) {
      events.push_back({iv.beginIndex, +1, M.staticBytes});
      events.push_back({iv.endIndex + 1, -1, M.staticBytes});
    }
  }
  llvm::sort(events,
             [](const Event &a, const Event &b) { return a.idx < b.idx; });
  int live = 0;
  uint64_t liveBytes = 0;
  for (auto &e : events) {
    live += e.delta;
    if (e.delta > 0)
      liveBytes += e.bytes;
    else
      liveBytes -= e.bytes;
    peakLiveDbs = std::max<uint64_t>(peakLiveDbs, (uint64_t)live);
    peakBytes = std::max<uint64_t>(peakBytes, liveBytes);
  }

  /// Simple reuse candidates: non-overlapping alloc lifetimes with same bytes
  SmallVector<DbAllocOp, 16> allocs;
  for (auto &kv : allocMetrics)
    allocs.push_back(kv.first);
  llvm::sort(allocs, [&](DbAllocOp a, DbAllocOp b) {
    return getOrder(a.getOperation()) < getOrder(b.getOperation());
  });
  for (size_t i = 0; i < allocs.size(); ++i) {
    for (size_t j = i + 1; j < allocs.size(); ++j) {
      auto &A = allocMetrics[allocs[i]];
      auto &B = allocMetrics[allocs[j]];
      if (A.endIndex <= B.allocIndex || B.endIndex <= A.allocIndex) {
        if (A.staticBytes != 0 && A.staticBytes == B.staticBytes) {
          reuseCandidates.emplace_back(allocs[i], allocs[j]);
        }
      }
    }
  }
}

void DbGraph::computeReuseColoring() {
  allocColor.clear();
  colorGroups.clear();
  SmallVector<DbAllocOp, 16> allocs;
  for (auto &kv : allocMetrics)
    allocs.push_back(kv.first);
  /// Sort by descending weight: bigger and longer-lived first
  llvm::sort(allocs, [&](DbAllocOp a, DbAllocOp b) {
    const auto &A = allocMetrics[a];
    const auto &B = allocMetrics[b];
    uint64_t wa = std::max<uint64_t>(A.staticBytes, A.totalAccessBytes) *
                  (1 + A.criticalPath);
    uint64_t wb = std::max<uint64_t>(B.staticBytes, B.totalAccessBytes) *
                  (1 + B.criticalPath);
    if (wa != wb)
      return wa > wb;
    return A.allocIndex < B.allocIndex;
  });

  auto interferes = [&](DbAllocOp x, DbAllocOp y) -> bool {
    const auto &X = allocMetrics[x];
    const auto &Y = allocMetrics[y];
    bool lifetimeOverlap =
        !(X.endIndex < Y.allocIndex || Y.endIndex < X.allocIndex);
    if (lifetimeOverlap)
      return true;
    return mayAlias(x, y);
  };

  for (DbAllocOp a : allocs) {
    llvm::SmallDenseSet<DbAllocOp> seen;
    for (auto &kv : allocColor) {
      DbAllocOp b = kv.first;
      if (interferes(a, b))
        seen.insert(b);
    }
    unsigned c = 0;
    while (seen.count(a))
      ++c;
    allocColor[a] = c;
    colorGroups[c].push_back(a);
  }
}

void DbGraph::exportToJson(llvm::raw_ostream &os) const {
  os << "{\n";
  /// Summary
  os << "  \"summary\": { \"peakLiveDbs\": " << peakLiveDbs
     << ", \"peakBytes\": " << peakBytes << " },\n";
  /// Allocs
  os << "  \"allocs\": [\n";
  bool first = true;
  for (const auto &kv : allocMetrics) {
    if (!first)
      os << ",\n";
    first = false;
    DbAllocOp alloc = kv.first;
    const DbAllocMetrics &M = kv.second;
    os << "    {\n";
    os << "      \"id\": \""
       << sanitizeForDot(getNode(alloc.getOperation())->getHierId()) << "\",\n";
    os << "      \"allocIndex\": " << M.allocIndex
       << ", \"endIndex\": " << M.endIndex << ",\n";
    os << "      \"numAcquires\": " << M.numAcquires
       << ", \"numReleases\": " << M.numReleases << ",\n";
    os << "      \"staticBytes\": " << M.staticBytes
       << ", \"criticalSpan\": " << M.criticalSpan
       << ", \"criticalPath\": " << M.criticalPath
       << ", \"totalAccessBytes\": " << M.totalAccessBytes << ",\n";
    os << "      \"isLongLived\": " << (M.isLongLived ? 1 : 0)
       << ", \"maybeEscaping\": " << (M.maybeEscaping ? 1 : 0) << ",\n";
    os << "      \"intervals\": [\n";
    bool firstIv = true;
    for (auto &iv : M.intervals) {
      if (!firstIv)
        os << ",\n";
      firstIv = false;
      os << "        { \"begin\": " << iv.beginIndex
         << ", \"end\": " << iv.endIndex << ", \"mode\": \""
         << const_cast<DbAcquireOp &>(iv.acquire).getMode() << "\" }";
    }
    os << "\n      ]\n";
    os << "    }";
  }
  os << "\n  ],\n";
  /// Reuse candidates
  os << "  \"reuseCandidates\": [\n";
  for (size_t i = 0; i < reuseCandidates.size(); ++i) {
    auto A = reuseCandidates[i].first;
    auto B = reuseCandidates[i].second;
    os << "    { \"a\": \""
       << sanitizeForDot(getNode(A.getOperation())->getHierId())
       << "\", \"b\": \""
       << sanitizeForDot(getNode(B.getOperation())->getHierId()) << "\" }"
       << (i + 1 < reuseCandidates.size() ? "," : "") << "\n";
  }
  os << "  ],\n";
  /// Coloring groups
  os << "  \"reuseColoring\": {\n";
  bool firstColor = true;
  for (auto &cg : colorGroups) {
    if (!firstColor)
      os << ",\n";
    firstColor = false;
    os << "    \"" << cg.first << "\": [";
    for (size_t i = 0; i < cg.second.size(); ++i) {
      auto A = cg.second[i];
      os << "\"" << sanitizeForDot(getNode(A.getOperation())->getHierId())
         << "\"";
      if (i + 1 < cg.second.size())
        os << ", ";
    }
    os << "]";
  }
  os << "\n  }\n";
  os << "}\n";
}

GraphBase::ChildIterator DbGraph::childBegin(NodeBase *node) {
  /// For now, return empty iterator
  /// TODO: Implement proper child iteration
  static std::vector<NodeBase *> emptyChildren;
  return emptyChildren.begin();
}

GraphBase::ChildIterator DbGraph::childEnd(NodeBase *node) {
  /// For now, return empty iterator
  /// TODO: Implement proper child iteration
  static std::vector<NodeBase *> emptyChildren;
  return emptyChildren.end();
}
