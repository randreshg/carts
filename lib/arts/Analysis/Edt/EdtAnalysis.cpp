///==========================================================================///
/// File: EdtAnalysis.cpp
///==========================================================================///

#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"

#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using namespace arts;

namespace {

static unsigned dbPatternRank(DbAccessPattern pattern) {
  switch (pattern) {
  case DbAccessPattern::stencil:
    return 3;
  case DbAccessPattern::indexed:
    return 2;
  case DbAccessPattern::uniform:
    return 1;
  case DbAccessPattern::unknown:
    return 0;
  }
  return 0;
}

static DbAccessPattern mergeDbAccessPattern(DbAccessPattern lhs,
                                            DbAccessPattern rhs) {
  return dbPatternRank(rhs) > dbPatternRank(lhs) ? rhs : lhs;
}

} // namespace

///==========================================================================///
/// EdtAnalysis
///==========================================================================///

EdtAnalysis::EdtAnalysis(ArtsAnalysisManager &AM) : ArtsAnalysis(AM) {
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
    const EdtInfo *edtInfo = &edtNode->getInfo();
    if (edtInfo) {
      unsigned edtIndex = edtInfo->orderIndex;
      os << "  EDT #" << edtIndex << ":\n";
      os << "    Total ops: " << edtInfo->totalOps << "\n";
      os << "    Loads: " << edtInfo->numLoads
         << ", Stores: " << edtInfo->numStores << "\n";
      os << "    Calls: " << edtInfo->numCalls << "\n";
      os << "    Max loop depth: " << edtInfo->maxLoopDepth << "\n";
      os << "    Compute/Memory ratio: " << edtInfo->computeToMemRatio << "\n";
      os << "    Bases read: " << edtInfo->basesRead.size()
         << ", written: " << edtInfo->basesWritten.size() << "\n";
      os << "    Bytes read: " << edtInfo->bytesRead
         << ", written: " << edtInfo->bytesWritten << "\n";
    }
  });
}

void EdtAnalysis::toJson(func::FuncOp func, llvm::raw_ostream &os) {
  os << "{\n";
  os << "  \"function\": \"" << func.getName() << "\",\n";
  os << "  \"edts\": [\n";

  bool first = true;
  EdtGraph &edtGraph = getOrCreateEdtGraph(func);
  func.walk([&](Operation *op) {
    if (auto edt = dyn_cast<EdtOp>(op)) {
      auto edtNode = static_cast<EdtNode *>(edtGraph.getNode(op));
      const EdtInfo *edtInfo = &edtNode->getInfo();

      if (edtInfo) {
        if (!first)
          os << ",\n";
        first = false;

        unsigned edtIndex = edtInfo->orderIndex;
        os << "    {\n";
        os << "      \"edtId\": " << edtIndex << ",\n";
        os << "      \"totalOps\": " << edtInfo->totalOps << ",\n";
        os << "      \"numLoads\": " << edtInfo->numLoads << ",\n";
        os << "      \"numStores\": " << edtInfo->numStores << ",\n";
        os << "      \"bytesRead\": " << edtInfo->bytesRead << ",\n";
        os << "      \"bytesWritten\": " << edtInfo->bytesWritten << ",\n";
        os << "      \"maxLoopDepth\": " << edtInfo->maxLoopDepth << ",\n";
        os << "      \"computeToMemRatio\": " << edtInfo->computeToMemRatio
           << "\n";
        os << "    }";
      }
    }
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
  auto eg = std::make_unique<EdtGraph>(func, &db, &getAnalysisManager());
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
