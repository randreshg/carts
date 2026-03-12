///==========================================================================///
/// File: EdtAnalysis.cpp
///==========================================================================///

#include "arts/analysis/edt/EdtAnalysis.h"
#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/graphs/edt/EdtGraph.h"
#include "arts/analysis/graphs/edt/EdtNode.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"

#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using namespace arts;

///==========================================================================///
/// EdtAnalysis
///==========================================================================///

EdtAnalysis::EdtAnalysis(AnalysisManager &AM) : ArtsAnalysis(AM) {
  assert(AM.getModule() && "Module is required");
}

void EdtAnalysis::analyze() {
  edtPatternByOp.clear();
  allocPatternByOp.clear();
  edtOrderIndex.clear();

  auto &AM = getAnalysisManager();
  for (auto func : AM.getModule().getOps<func::FuncOp>())
    analyzeFunc(func);
  analyzed = true;
}

void EdtAnalysis::analyzeFunc(func::FuncOp func) {
  unsigned edtIndex = 0;
  auto &analysisManager = getAnalysisManager();
  EdtGraph &edtGraph = getOrCreateEdtGraph(func);
  func.walk([&](EdtOp edt) {
    auto edtNode = edtGraph.getEdtNode(edt);
    if (!edtNode)
      return;
    auto &info = edtNode->getInfo();
    info.orderIndex = edtIndex;
    edtOrderIndex[edt] = edtIndex++;

    info.loopDistributionPatterns.clear();
    info.dominantDistributionPattern = EdtDistributionPattern::unknown;

    bool sawLoop = false;
    bool mixed = false;
    EdtDistributionPattern summary = EdtDistributionPattern::unknown;

    edt.walk([&](arts::ForOp forOp) {
      if (forOp->getParentOfType<EdtOp>() != edt)
        return;
      DbAnalysis::LoopDbAccessSummary loopAccessSummary;
      if (auto summary =
              analysisManager.getLoopDbAccessSummary(forOp.getOperation()))
        loopAccessSummary = *summary;
      for (auto &[allocOp, pattern] : loopAccessSummary.allocPatterns) {
        auto it = allocPatternByOp.find(allocOp);
        if (it == allocPatternByOp.end()) {
          allocPatternByOp[allocOp] = pattern;
        } else {
          it->second = mergeDbAccessPattern(it->second, pattern);
        }
      }

      EdtDistributionPattern pattern = loopAccessSummary.distributionPattern;
      info.loopDistributionPatterns[forOp.getOperation()] = pattern;
      if (!sawLoop) {
        summary = pattern;
        sawLoop = true;
      } else if (summary != pattern) {
        mixed = true;
      }
    });

    if (sawLoop && !mixed)
      info.dominantDistributionPattern = summary;
    edtPatternByOp[edt.getOperation()] = info.dominantDistributionPattern;
  });
}

void EdtAnalysis::print(func::FuncOp func, llvm::raw_ostream &os) {
  os << "EdtAnalysis for function: " << func.getName() << "\n";

  EdtGraph &edtGraph = getOrCreateEdtGraph(func);
  func.walk([&](EdtOp edt) {
    auto edtNode = edtGraph.getEdtNode(edt);
    if (!edtNode)
      return;
    const EdtInfo &info = edtNode->getInfo();
    os << "  EDT #" << info.orderIndex << ":\n";
    os << "    Total ops: " << info.totalOps << "\n";
    os << "    Loads: " << info.numLoads << ", Stores: " << info.numStores
       << "\n";
    os << "    Calls: " << info.numCalls << "\n";
    os << "    Max loop depth: " << info.maxLoopDepth << "\n";
    os << "    Compute/Memory ratio: " << info.computeToMemRatio << "\n";
    os << "    Bases read: " << info.basesRead.size()
       << ", written: " << info.basesWritten.size() << "\n";
    os << "    Bytes read: " << info.bytesRead
       << ", written: " << info.bytesWritten << "\n";
  });
}

void EdtAnalysis::toJson(func::FuncOp func, llvm::raw_ostream &os) {
  os << "{\n";
  os << "  \"function\": \"" << func.getName() << "\",\n";
  os << "  \"edts\": [\n";

  bool first = true;
  EdtGraph &edtGraph = getOrCreateEdtGraph(func);
  func.walk([&](EdtOp edt) {
    auto edtNode = edtGraph.getEdtNode(edt);
    if (!edtNode)
      return;
    const EdtInfo &info = edtNode->getInfo();

    if (!first)
      os << ",\n";
    first = false;

    os << "    {\n";
    os << "      \"edtId\": " << info.orderIndex << ",\n";
    os << "      \"totalOps\": " << info.totalOps << ",\n";
    os << "      \"numLoads\": " << info.numLoads << ",\n";
    os << "      \"numStores\": " << info.numStores << ",\n";
    os << "      \"bytesRead\": " << info.bytesRead << ",\n";
    os << "      \"bytesWritten\": " << info.bytesWritten << ",\n";
    os << "      \"maxLoopDepth\": " << info.maxLoopDepth << ",\n";
    os << "      \"computeToMemRatio\": " << info.computeToMemRatio << "\n";
    os << "    }";
  });

  os << "\n  ]\n";
  os << "}\n";
}

EdtGraph &EdtAnalysis::getOrCreateEdtGraph(func::FuncOp func) {
  auto it = edtGraphs.find(func);
  if (it != edtGraphs.end())
    return *it->second.get();

  /// Build using DbGraph from DbAnalysis for consistency
  DbGraph &db = getAnalysisManager().getDbAnalysis().getOrCreateGraph(func);
  auto eg = std::make_unique<EdtGraph>(func, &db, this);
  eg->build();

  auto *ptr = eg.get();
  const_cast<EdtAnalysis *>(this)->edtGraphs[func] = std::move(eg);
  return *ptr;
}

std::optional<EdtDistributionPattern>
EdtAnalysis::getEdtDistributionPattern(EdtOp edt) {
  if (!edt)
    return std::nullopt;
  if (!analyzed)
    analyze();
  auto it = edtPatternByOp.find(edt.getOperation());
  if (it == edtPatternByOp.end())
    return std::nullopt;
  return it->second;
}

std::optional<DbAccessPattern>
EdtAnalysis::getAllocAccessPattern(Operation *allocOp) {
  if (!allocOp)
    return std::nullopt;
  if (!analyzed)
    analyze();
  auto it = allocPatternByOp.find(allocOp);
  if (it == allocPatternByOp.end())
    return std::nullopt;
  return it->second;
}

void EdtAnalysis::forEachAllocAccessPattern(
    llvm::function_ref<void(Operation *, DbAccessPattern)> fn) {
  if (!analyzed)
    analyze();
  for (const auto &entry : allocPatternByOp)
    fn(entry.first, entry.second);
}

bool EdtAnalysis::invalidateGraph(func::FuncOp func) {
  auto it = edtGraphs.find(func);
  if (it != edtGraphs.end()) {
    if (it->second)
      it->second->invalidate();
    edtGraphs.erase(it);
    analyzed = false;
    return true;
  }
  return false;
}

void EdtAnalysis::invalidate() {
  for (auto &kv : edtGraphs)
    if (kv.second)
      kv.second->invalidate();
  edtGraphs.clear();
  edtPatternByOp.clear();
  allocPatternByOp.clear();
  edtOrderIndex.clear();
  analyzed = false;
}

EdtNode *EdtAnalysis::getEdtNode(EdtOp op) {
  auto func = op->getParentOfType<func::FuncOp>();
  if (!func)
    return nullptr;
  return getOrCreateEdtGraph(func).getEdtNode(op);
}

MetadataManager &EdtAnalysis::getMetadataManager() {
  return getAnalysisManager().getMetadataManager();
}

LoopAnalysis &EdtAnalysis::getLoopAnalysis() {
  return getAnalysisManager().getLoopAnalysis();
}
