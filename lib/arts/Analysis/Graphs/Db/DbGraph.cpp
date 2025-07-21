//===----------------------------------------------------------------------===//
// Db/DbGraph.cpp - Implementation of DbGraph
//===----------------------------------------------------------------------===//
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbEdge.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

using namespace mlir::arts;

#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

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
  /// TODO: Add data flow analysis
  // dataFlowAnalysis = std::make_unique<DbDataFlowAnalysis>(analysis, this);
  LLVM_DEBUG(DBGS() << "Creating db graph for function: "
                    << func.getName().str() << "\n");
}

void DbGraph::build() {
  if (isBuilt && !needsRebuild)
    return;
  LLVM_DEBUG(DBGS() << "Building db graph\n");
  invalidate();
  collectNodes();
  buildDependencies();
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
    if (edge->getTo() == to && isa<DbAllocEdge>(edge))
      return true;
  }
  return false;
}

DbAllocOp DbGraph::getParentAlloc(Operation *op) {
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
  // return analysis->dbAliasAnalysis->mayAlias(*n1, *n2);
  /// TODO: Add mayAlias implementation
  return false;
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
  LLVM_DEBUG(DBGS() << "Phase 1 - Collecting nodes\n");

  func.walk([&](Operation *op) {
    if (isa<DbAllocOp, DbAcquireOp, DbReleaseOp>(op)) {
      getOrCreateNode(op);
    }
  });

  LLVM_DEBUG(DBGS() << "Collected " << allocNodes.size() << " allocations, "
                    << acquireNodeMap.size() << " acquires, "
                    << releaseNodeMap.size() << " releases\n");
}

DbAllocOp DbGraph::findRootAllocOp(Operation *op) {
  Value source;
  if (auto acquireOp = dyn_cast<DbAcquireOp>(op))
    source = acquireOp.getSource();
  else if (auto releaseOp = dyn_cast<DbReleaseOp>(op))
    source = releaseOp.getSources()[0];
  else
    return nullptr;

  const int maxChainDepth = 10;
  int depth = 0;
  while (depth < maxChainDepth) {
    if (auto allocOp = source.getDefiningOp<DbAllocOp>())
      return allocOp;
    if (auto chainedAcquireOp = source.getDefiningOp<DbAcquireOp>()) {
      source = chainedAcquireOp.getSource();
    } else if (auto chainedReleaseOp = source.getDefiningOp<DbReleaseOp>()) {
      source = chainedReleaseOp.getSources()[0];
    } else {
      break;
    }
    depth++;
  }
  return nullptr;
}

void DbGraph::buildDependencies() {
  LLVM_DEBUG(DBGS() << "Phase 2 - Building dependencies\n");
  // dataFlowAnalysis->analyze();
  LLVM_DEBUG(DBGS() << "Phase 2 - Data flow analysis finished\n");
}

GraphBase::ChildIterator DbGraph::childBegin(NodeBase *node) {
  // For now, return empty iterator
  // TODO: Implement proper child iteration
  static std::vector<NodeBase *> emptyChildren;
  return emptyChildren.begin();
}

GraphBase::ChildIterator DbGraph::childEnd(NodeBase *node) {
  // For now, return empty iterator
  // TODO: Implement proper child iteration
  static std::vector<NodeBase *> emptyChildren;
  return emptyChildren.end();
}
