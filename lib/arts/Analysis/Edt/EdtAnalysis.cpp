///==========================================================================
/// File: EdtAnalysis.cpp
///==========================================================================

#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/ArtsDialect.h"
/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"

/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

// #include "arts/Utils/ArtsDebug.h"
// ARTS_DEBUG_SETUP(edt_analysis)

using namespace mlir;
using namespace arts;

///==========================================================================
/// EdtAnalysis
///==========================================================================

EdtAnalysis::EdtAnalysis(ArtsAnalysisManager &AM) : AM(AM) {
  assert(AM.getModule() && "Module is required");
}

void EdtAnalysis::analyze() {
  for (auto func : AM.getModule().getOps<func::FuncOp>())
    analyzeFunc(func);
}

void EdtAnalysis::analyzeFunc(func::FuncOp func) {
  unsigned edtIndex = 0;
  EdtGraph &edtGraph = getOrCreateEdtGraph(func);
  func.walk([&](EdtOp edt) {
    auto edtNode = edtGraph.getEdtNode(edt);
    auto &info = edtNode->getInfo();
    info.orderIndex = edtIndex;
    edtOrderIndex[edt] = edtIndex++;
  });
}

EdtPairAffinity EdtAnalysis::affinity(EdtOp from, EdtOp to) {
  /// TODO: Fix this
  EdtPairAffinity result;
  auto parentFunc = from->getParentOfType<func::FuncOp>();
  EdtGraph &edtGraph = getOrCreateEdtGraph(parentFunc);
  auto edtFrom = edtGraph.getEdtNode(from);
  auto edtTo = edtGraph.getEdtNode(to);
  const EdtInfo *edtInfoFrom = &edtFrom->getInfo();
  const EdtInfo *edtInfoTo = &edtTo->getInfo();

  if (!edtInfoFrom || !edtInfoTo) {
    return result; /// Return zero affinity
  }

  /// Calculate data overlap (Jaccard similarity)
  DenseSet<Value> unionBases = edtInfoFrom->basesRead;
  unionBases.insert(edtInfoFrom->basesWritten.begin(),
                    edtInfoFrom->basesWritten.end());
  unionBases.insert(edtInfoTo->basesRead.begin(), edtInfoTo->basesRead.end());
  unionBases.insert(edtInfoTo->basesWritten.begin(),
                    edtInfoTo->basesWritten.end());

  DenseSet<Value> intersectionBases;
  for (Value base : edtInfoFrom->basesRead) {
    if (edtInfoTo->basesRead.count(base) ||
        edtInfoTo->basesWritten.count(base)) {
      intersectionBases.insert(base);
    }
  }
  for (Value base : edtInfoFrom->basesWritten) {
    if (edtInfoTo->basesRead.count(base) ||
        edtInfoTo->basesWritten.count(base)) {
      intersectionBases.insert(base);
    }
  }

  if (!unionBases.empty()) {
    result.dataOverlap =
        static_cast<double>(intersectionBases.size()) / unionBases.size();
  }

  /// Calculate hazard score (write conflicts)
  unsigned conflicts = 0;
  for (Value base : intersectionBases) {
    if ((edtInfoFrom->basesWritten.count(base) &&
         edtInfoTo->basesRead.count(base)) ||
        (edtInfoFrom->basesRead.count(base) &&
         edtInfoTo->basesWritten.count(base)) ||
        (edtInfoFrom->basesWritten.count(base) &&
         edtInfoTo->basesWritten.count(base))) {
      conflicts++;
    }
  }

  if (!intersectionBases.empty()) {
    result.hazardScore =
        static_cast<double>(conflicts) / intersectionBases.size();
    result.mayConflict = conflicts > 0;
  }

  /// Simple program order proximity
  /// TODO: could be enhanced with actual distance
  auto orderA = edtInfoFrom->orderIndex;
  auto orderB = edtInfoTo->orderIndex;
  if (orderA != orderB) {
    result.reuseProximity =
        1.0 /
        (1.0 + std::abs(static_cast<int>(orderA) - static_cast<int>(orderB)));
  }

  result.localityScore = result.dataOverlap * result.reuseProximity;
  result.concurrencyRisk = result.hazardScore * result.dataOverlap;

  return result;
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
  // Build using DbGraph from DbAnalysis for consistency
  DbGraph &db = AM.getDbAnalysis().getOrCreateGraph(func);
  auto eg = std::make_unique<EdtGraph>(func, &db);
  eg->build();
  auto *ptr = eg.get();
  const_cast<EdtAnalysis *>(this)->edtGraphs[func] = std::move(eg);
  return *ptr;
}

bool EdtAnalysis::invalidateEdtGraph(func::FuncOp func) {
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
}
