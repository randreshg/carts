
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <cassert>

// #include "llvm/Analysis/MemorySSA.h"
// #include "llvm/Analysis/MemorySSAUpdater.h"

#include "carts/analysis/graph/EDTEdge.h"
#include "carts/analysis/graph/EDTGraph.h"
#include "carts/utils/ARTS.h"

using namespace llvm;
using namespace arts;
using namespace std;

/// DEBUG
#define DEBUG_TYPE "edt-graph"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

namespace arts {
/// EDTGraph
EDTGraph::EDTGraph() {}
// EDTGraph::EDTGraph(EDTGraphCache &Cache) : Cache(Cache) {
//   LLVM_DEBUG(dbgs() <<
//   "-------------------------------------------------\n"); LLVM_DEBUG(dbgs()
//   << TAG << "Building the EDT Graph\n"); FM =
//   Cache.getNoelle().getFunctionsManager(); CG = FM->getProgramCallGraph();
//   /// Create nodes
//   createNodes();
//   setCreationDeps();
//   /// Debug Cache
//   LLVM_DEBUG(dbgs() <<
//   "-------------------------------------------------\n"); LLVM_DEBUG(dbgs()
//   << TAG << "Printing the Cache\n"); for (auto &ValItr : Cache.getValues()) {
//     auto *V = ValItr.first;
//     auto &EDTs = ValItr.second;
//     /// Get EDT Call parent function
//     auto *ParentEDT = getNode(*EDTs[0]->getCall()->getFunction());
//     LLVM_DEBUG(dbgs() << "  Value: " << *V << " in EDT #"
//                       << ParentEDT->getEDT()->getID() << "\n");
//     for (auto *E : EDTs) {
//       LLVM_DEBUG(dbgs() << "    - EDT #" << E->getID() << "\n");
//     }
//   }
//   LLVM_DEBUG(dbgs() <<
//   "-------------------------------------------------\n"); analyzeDeps();
//   /// Debug module
//   // LLVM_DEBUG(dbgs() << Cache.getModule());
//   LLVM_DEBUG(dbgs() <<
//   "-------------------------------------------------\n");
// }

EDTGraph::~EDTGraph() {
  LLVM_DEBUG(dbgs() << TAG << "Destroying the EDT Graph\n");
  for (auto &E : EDTs)
    delete E.second;
}

/// Analysis
bool EDTGraph::isReachable(EDTGraphNode *From, EDTGraphNode *To) {
  for (auto *Edge : getOutgoingEdges(From)) {
    if (Edge->getTo() == To) {
      return true;
    } else if (isReachable(Edge->getTo(), To)) {
      return true;
    }
  }
  return false;
}

bool EDTGraph::isCreationReachable(EDTGraphNode *From, EDTGraphNode *To) {
  for (auto *Edge : getOutgoingEdges(From)) {
    if (Edge->hasCreationDep() && Edge->getTo() == To) {
      return true;
    } else if (isCreationReachable(Edge->getTo(), To)) {
      return true;
    }
  }
  return false;
}

bool EDTGraph::isDataDependent(EDTGraphNode *From, EDTGraphNode *To) {
  for (auto *Edge : getOutgoingEdges(From)) {
    if (Edge->isDataEdge() && Edge->getTo() == To) {
      return true;
    }
  }
  return false;
}

EDT *EDTGraph::getParentSyncEDT(EDTGraphNode *Node, uint32_t Depth) {
  for (auto *Edge : getIncomingEdges(Node)) {
    /// We are only concerned with creation edges
    if (!Edge->hasCreationDep())
      continue;

    auto *ParentEDT = Edge->getFrom()->getEDT();
    if (ParentEDT->isAsync())
      return getParentSyncEDT(Edge->getFrom(), Depth + 1);

    SyncEDT *ParentSyncEDT = dyn_cast<SyncEDT>(ParentEDT);
    if (ParentSyncEDT->mustSyncChilds() && Depth == 0)
      return ParentSyncEDT;
    if (ParentSyncEDT->mustSyncDescendants() && Depth > 0)
      return ParentSyncEDT;
    return nullptr;
  }
  return nullptr;
}

/// Nodes
void EDTGraph::createNode(Function &Fn) {
  if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
    return;
  LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << TAG << " Processing function: " << Fn.getName() << "\n");

  auto *E = EDT::get(&Fn);
  assert(E != nullptr && "The EDT is null");
  insertNode(E);
}

void EDTGraph::createNodes() {
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Creating the EDT Nodes\n");
  // for (auto &Fn : M)
  //   createNode(Fn);

  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << EDTs.size() << " EDT Nodes were created\n");
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
}

unordered_set<EDTGraphNode *> EDTGraph::getNodes() {
  unordered_set<EDTGraphNode *> Aux;
  for (auto Pair : EDTs) {
    Aux.insert(Pair.second);
  }
  return Aux;
}

EDTGraphNode *EDTGraph::getEntryNode(void) const {
  // const auto it =
  //     EDTs.find(Cache.getNoelle().getFunctionsManager()->getEntryFunction());
  // if (it != EDTs.end())
  //   return it->second;
  return nullptr;
}

EDTGraphNode *EDTGraph::getNode(Function &Fn) const {
  auto it = EDTs.find(&Fn);
  if (it != EDTs.end())
    return it->second;
  return nullptr;
}

EDTGraphNode *EDTGraph::getNode(EDT *E) const { return getNode(*E->getFn()); }

EDTGraphNode *EDTGraph::insertNode(EDT *E) {
  EDTGraphNode *EDTNode = new EDTGraphNode(*E);
  EDTs[E->getFn()] = EDTNode;
  return EDTNode;
}

EDTGraphNode *EDTGraph::insertNode(EDT *E, EDTGraphNode *ParentNode,
                                   Function *Fn) {
  EDTGraphNode *EDTNode = (Fn == nullptr) ? insertNode(E) : insertNode(E, *Fn);
  addCreationEdge(ParentNode, EDTNode);
  return EDTNode;
}

EDTGraphNode *EDTGraph::insertNode(EDT *E, Function &Fn) {
  EDTGraphNode *EDTNode = new EDTGraphNode(*E);
  EDTs[&Fn] = EDTNode;
  return EDTNode;
}

/// Edges
EDTGraphEdge *EDTGraph::getEdge(EDTGraphNode *From, EDTGraphNode *To) {
  /// Fetch the set of edges from @from.
  if (OutgoingEdges.find(From) == OutgoingEdges.end())
    return nullptr;

  /// Fetch the edge to @to
  auto &Tmp = OutgoingEdges[From];
  if (Tmp.find(To) == Tmp.end())
    return nullptr;
  return Tmp[To];
}

unordered_set<EDTGraphEdge *> EDTGraph::getIncomingEdges(EDTGraphNode *Node) {
  unordered_set<EDTGraphEdge *> Aux;
  if (IncomingEdges.find(Node) == IncomingEdges.end())
    return Aux;
  auto &Tmp = IncomingEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<EDTGraphEdge *> EDTGraph::getOutgoingEdges(EDTGraphNode *Node) {
  unordered_set<EDTGraphEdge *> Aux;
  if (OutgoingEdges.find(Node) == OutgoingEdges.end())
    return Aux;
  auto &Tmp = OutgoingEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<EDTGraphEdge *> EDTGraph::getEdges(EDTGraphNode *Node) {
  auto IncomingEdges = getIncomingEdges(Node);
  auto OutgoingEdges = getOutgoingEdges(Node);
  IncomingEdges.insert(OutgoingEdges.begin(), OutgoingEdges.end());
  return IncomingEdges;
}

EDTGraphEdge *EDTGraph::fetchOrCreateEdge(EDTGraphNode *From, EDTGraphNode *To,
                                          bool IsDataEdge) {
  auto *ExistingEdge = getEdge(From, To);
  if (ExistingEdge == nullptr) {
    /// The edge from @fromNode to @toNode doesn't exist yet
    LLVM_DEBUG(dbgs() << "        - Creating edge from \"EDT #"
                      << From->getEDT()->getID() << "\" to \"EDT #"
                      << To->getEDT()->getID() << "\"\n");
    EDTGraphEdge *NewEdge;
    if (IsDataEdge) {
      NewEdge = new EDTGraphDataEdge(From, To);
      LLVM_DEBUG(dbgs() << "          Data Edge\n");
    } else {
      NewEdge = new EDTGraphControlEdge(From, To);
      LLVM_DEBUG(dbgs() << "          Control Edge\n");
    }
    /// Add the new edge.
    addEdge(From, To, NewEdge);
    return NewEdge;
  }
  assert(ExistingEdge != nullptr);
  /// The edge is a control edge
  if (ExistingEdge->isControlEdge()) {
    if (!IsDataEdge)
      return ExistingEdge;
    /// The edge is a control edge with creation dep.
    /// Convert it to a data edge.
    assert(ExistingEdge->hasCreationDep() &&
           "The edge is not a creation edge - Can not convert to Data Edge");
    LLVM_DEBUG(dbgs() << "        - Converting control edge to date edge [\""
                      << From->getEDT()->getName() << "\" -> \""
                      << To->getEDT()->getName() << "\"]\n");
    auto *NewDataEdge = new EDTGraphDataEdge(From, To);
    NewDataEdge->setCreationDep(true);
    removeEdge(ExistingEdge);
    addEdge(From, To, NewDataEdge);
    return NewDataEdge;
  }
  /// The edge is a data edge
  assert(IsDataEdge && "The edge is a Data edge - Can not convert to Control "
                       "Edge");
  return ExistingEdge;
}

void EDTGraph::addEdge(EDTGraphNode *From, EDTGraphNode *To,
                       EDTGraphEdge *Edge) {
  auto &Tmp = OutgoingEdges[From];
  Tmp[To] = Edge;
  auto &Tmp2 = IncomingEdges[To];
  Tmp2[From] = Edge;
}

/// Add edges with EDT
void EDTGraph::addCreationEdge(EDT *From, EDT *To) {
  addCreationEdge(getNode(From), getNode(To));
}

void EDTGraph::addDataEdge(EDT *From, EDT *To, Value *V) {
  addDataEdge(getNode(From), getNode(To), V);
}

void EDTGraph::addControlEdge(EDT *From, EDT *To) {
  addControlEdge(getNode(From), getNode(To));
}

/// Add edges with EDTGraphNode
void EDTGraph::addCreationEdge(EDTGraphNode *From, EDTGraphNode *To) {
  EDTGraphEdge *E = fetchOrCreateEdge(From, To, false);
  E->setCreationDep(true);
}

void EDTGraph::addDataEdge(EDTGraphNode *From, EDTGraphNode *To, Value *V) {
  auto *DataEdge =
      dyn_cast<EDTGraphDataEdge>(fetchOrCreateEdge(From, To, true));
  if (V != nullptr)
    DataEdge->addValue(V);
}

void EDTGraph::addControlEdge(EDTGraphNode *From, EDTGraphNode *To) {
  fetchOrCreateEdge(From, To, false);
}

void EDTGraph::removeEdge(EDTGraphEdge *Edge) {
  auto *From = Edge->getFrom();
  auto *To = Edge->getTo();
  OutgoingEdges[From].erase(To);
  IncomingEdges[To].erase(From);
  delete Edge;
}

void EDTGraph::removeEdge(EDTGraphNode *From, EDTGraphNode *To) {
  auto *Edge = getEdge(From, To);
  if (Edge == nullptr)
    return;
  OutgoingEdges[From].erase(To);
  IncomingEdges[To].erase(From);
  delete Edge;
}

void EDTGraph::print(void) {
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << TAG << "Printing the EDT Graph\n");
  /// Print the outgoing edges.
  for (auto *EDTNode : getNodes()) {
    auto *E = EDTNode->getEDT();
    LLVM_DEBUG(dbgs() << "- EDT #" << E->getID() << " - \"" << E->getName()
                      << "\"\n");
    LLVM_DEBUG(dbgs() << "  - Type: " << toString(E->getTy()) << "\n");
    /// Data environment
    LLVM_DEBUG(dbgs() << "  - Data Environment:\n");
    auto &DE = E->getDataEnv();
    LLVM_DEBUG(dbgs() << "    - " << "Number of ParamV = " << DE.getParamC()
                      << "\n");
    for (auto &P : DE.ParamV) {
      LLVM_DEBUG(dbgs() << "      - " << *P << "\n");
    }
    LLVM_DEBUG(dbgs() << "    - " << "Number of DepV = " << DE.getDepC()
                      << "\n");
    for (auto &D : DE.DepV) {
      LLVM_DEBUG(dbgs() << "      - " << *D << "\n");
    }
    /// Dependencies
    LLVM_DEBUG(dbgs() << "  - Incoming Edges:\n");
    auto InEdges = getIncomingEdges(EDTNode);
    if (InEdges.size() == 0) {
      LLVM_DEBUG(dbgs() << "    - The EDT has no incoming edges\n");
    } else {
      /// Print the incoming edges.
      for (auto *DepEdge : InEdges) {
        auto *From = DepEdge->getFrom();
        auto *FromE = From->getEDT();
        LLVM_DEBUG(dbgs() << "    - [");
        if (DepEdge->isDataEdge()) {
          LLVM_DEBUG(dbgs() << "data");
        } else if (DepEdge->isControlEdge()) {
          LLVM_DEBUG(dbgs() << "control");
        }
        if (DepEdge->hasCreationDep()) {
          LLVM_DEBUG(dbgs() << "/ creation");
        }
        LLVM_DEBUG(dbgs() << "] \"EDT #" << FromE->getID() << "\"\n");
      }
    }

    LLVM_DEBUG(dbgs() << "  - Outgoing Edges:\n");
    auto OutEdges = getOutgoingEdges(EDTNode);
    if (OutEdges.size() == 0) {
      LLVM_DEBUG(dbgs() << "    - The EDT has no outgoing edges\n");
    } else {
      /// Print the outgoing edges.
      for (auto *DepEdge : OutEdges) {
        auto *To = DepEdge->getTo();
        auto *ToE = To->getEDT();
        LLVM_DEBUG(dbgs() << "    - [");
        if (DepEdge->isDataEdge()) {
          LLVM_DEBUG(dbgs() << "data");
        } else if (DepEdge->isControlEdge()) {
          LLVM_DEBUG(dbgs() << "control");
        }
        if (DepEdge->hasCreationDep()) {
          LLVM_DEBUG(dbgs() << "/ creation");
        }
        LLVM_DEBUG(dbgs() << "] \"EDT #" << ToE->getID() << "\"\n");
        if (DepEdge->isDataEdge()) {
          auto *DataEdge = cast<EDTGraphDataEdge>(DepEdge);
          auto Values = DataEdge->getValues();
          for (auto *V : Values) {
            LLVM_DEBUG(dbgs() << "        - " << *V << "\n");
          }
        }
      }
    }
    LLVM_DEBUG(dbgs() << "\n");
  }
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - -\n\n");
}
} // namespace arts