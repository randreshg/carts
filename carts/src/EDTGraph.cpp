
#include "EDTGraph.h"
#include "llvm/Support/Debug.h"

#include "ARTS.h"
#include "ARTSUtils.h"

using namespace llvm;
using namespace arts;
using namespace arts_utils;
using namespace arts_utils::omp;
using namespace arcana::noelle;

/// DEBUG
#define DEBUG_TYPE "edt-graph"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// EDTGraphEdge
EDTGraphEdge::EDTGraphEdge() {}
EDTGraphEdge::~EDTGraphEdge() {}
bool EDTGraphEdge::isDataDep(void) { return IsDataDep; }
bool EDTGraphEdge::isControlDep(void) { return IsControlDep; }

/// EDTGraphNode
EDTGraphNode::EDTGraphNode(EDT &E) : E(E) {}
EDTGraphNode::~EDTGraphNode() {}

void EDTGraphNode::print(void) {
  LLVM_DEBUG(dbgs() << "EDT NODE:\n" << E << "\n");
}

/// EDTGraph
EDTGraph::EDTGraph(EDTCache &Cache) : Cache(Cache) {
  LLVM_DEBUG(dbgs() << TAG << "Building the EDT Graph\n");
  auto &M = Cache.getModule();
  /// Create MainEDT
  auto &NM = Cache.getNoelle();
  // auto FM = NM.getFunctionsManager();
  // auto MainFunction = FM->getEntryFunction();

  for (auto &F : M) {
    if (F.isDeclaration() && !F.hasLocalLinkage())
      continue;

    LLVM_DEBUG(dbgs() << TAG << "Processing function: " << F.getName() << "\n");
    removeLifetimeMarkers(F);
    for (auto &Inst : instructions(&F)) {
      auto *CB = dyn_cast<CallBase>(&Inst);
      if (!CB)
        continue;
      /// Get the callee
      omp::Type RTF = omp::getRTFunction(*CB);
      switch (RTF) {
      case omp::PARALLEL: {
        LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Parallel Region Found:\n" << *CB << "\n");
        insertNode(new ParallelEDT(Cache, CB));
      } break;
      case omp::TASKALLOC: {
        LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Task Region Found:\n" << *CB << "\n");
        insertNode(new TaskEDT(Cache, CB));
      } break;
      case omp::TASKWAIT: {
        assert(false && "Taskwait not implemented yet");
      } break;
      case omp::OTHER: {
        LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Other Function Found:\n" << *CB << "\n");
      } break;
      default:
        continue;
        break;
      }
    }
  }
  LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - -\n\n");
}

EDTGraphNode *EDTGraph::getEntryNode(void) const {
  // LLVM_DEBUG(dbgs() << TAG << "Fetching the entry node\n");

  return nullptr;
}

EDTGraphNode *EDTGraph::getNode(EDT *E) const {
  // LLVM_DEBUG(dbgs() << TAG << "Fetching the EDT node\n");
  auto it = EDTs.find(E->getOMPOutlinedFn());
  if (it != EDTs.end())
    return it->second;
  return nullptr;
}

void EDTGraph::insertNode(EDT *E) {
  // LLVM_DEBUG(dbgs() << TAG << "Inserting the EDT node\n");
  EDTGraphNode *EDTNode = new EDTGraphNode(*E);
  EDTs[E->getOMPOutlinedFn()] = EDTNode;
}

void EDTGraph::addEdge(EDTGraphNode *Src, EDTGraphNode *Dst, EDTGraphEdge *E) {
  // LLVM_DEBUG(dbgs() << TAG << "Adding an edge\n");
}

void EDTGraph::removeEdge(EDTGraphNode *Src, EDTGraphNode *Dst) {
  // LLVM_DEBUG(dbgs() << TAG << "Removing an edge\n");
}

void EDTGraph::print(void) {
  LLVM_DEBUG(dbgs() << TAG << "Printing the EDT Graph\n");
  for (auto &E : EDTs) {
    E.second->print();
  }
}