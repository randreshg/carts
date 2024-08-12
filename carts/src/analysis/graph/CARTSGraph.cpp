#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <cassert>
#include <cstdint>

#include "carts/analysis/graph/CARTSEdge.h"
#include "carts/analysis/graph/CARTSGraph.h"
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
/// CARTSGraph
CARTSGraph::CARTSGraph() {}

CARTSGraph::~CARTSGraph() {
  LLVM_DEBUG(dbgs() << TAG << "Destroying the CARTS Graph\n");
  for (auto &E : EDTs)
    delete E.second;
}

/// Analysis
bool CARTSGraph::isReachable(EDTGraphNode *From, EDTGraphNode *To) {
  for (auto *Edge : getOutgoingEdges(From)) {
    if (Edge->getTo() == To)
      return true;
    else if (isReachable(Edge->getTo(), To))
      return true;
  }
  return false;
}

bool CARTSGraph::isCreationReachable(EDTGraphNode *From, EDTGraphNode *To) {
  // for (auto *Edge : getOutgoingEdges(From)) {
  //   if (Edge->hasCreationDep() && Edge->getTo() == To) {
  //     return true;
  //   } else if (isCreationReachable(Edge->getTo(), To)) {
  //     return true;
  //   }
  // }
  return false;
}

bool CARTSGraph::isDataDependent(EDTGraphNode *From, EDTGraphNode *To) {
  for (auto *Edge : getOutgoingEdges(From)) {
    if (Edge->isDataEdge() && Edge->getTo() == To)
      return true;
  }
  return false;
}

EDT *CARTSGraph::getParentSyncEDT(EDTGraphNode *Node, uint32_t Depth) {
  for (auto *Edge : getIncomingEdges(Node)) {
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

bool CARTSGraph::isChild(EDT *Parent, EDT *Child) {
  return isChild(getNode(Parent), getNode(Child));
}

bool CARTSGraph::isChild(EDTGraphNode *Parent, EDTGraphNode *Child) {
  EDTGraphCreationEdge *Edge = getEdge(Parent, Child);
  if (Edge == nullptr || !Edge->hasCreationDep())
    return false;
  return true;
}

/// Nodes
void CARTSGraph::createNode(Function &Fn) {
  if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
    return;
  LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << TAG << " Processing function: " << Fn.getName() << "\n");

  auto *E = EDT::get(&Fn);
  assert(E != nullptr && "The EDT is null");
  insertNode(E);
}

SetVector<EDTGraphNode *> CARTSGraph::getNodes() {
  unordered_set<EDTGraphNode *> Aux;
  for (auto Pair : EDTs)
    Aux.insert(Pair.second);
  return Aux;
}

EDTGraphNode *CARTSGraph::getNode(EDT *E) const { return getNode(*E->getFn()); }

EDTGraphNode *CARTSGraph::getNode(Function &Fn) const {
  auto it = EDTs.find(&Fn);
  if (it != EDTs.end())
    return it->second;
  return nullptr;
}

EDTGraphNode *CARTSGraph::getEntryNode() {
  if (EntryNode != nullptr)
    return EntryNode;
  /// Fetch the entry node
  auto EDTNodes = getNodes();
  for (auto &EDTNode : EDTNodes) {
    if (EDTNode->getEDT()->isMain()) {
      EntryNode = EDTNode;
      return EDTNode;
    }
  }
  return nullptr;
}

EDTGraphNode *CARTSGraph::getExitNode() {
  if (ExitNode != nullptr)
    return ExitNode;
  /// Fetch the exit node
  /// Get entry node
  EDTGraphNode *Entry = getEntryNode();
  /// Analyze creation edges of the entry node
  for (auto *Edge : getOutgoingEdges(Entry)) {
    /// The exit node is created by the Entry node and is a sync Done region
    if (Edge->hasCreationDep()) {
      EDT *ToEDT = Edge->getTo()->getEDT();
      if (!ToEDT->isAsync()) {
        ExitNode = getNode(ToEDT->getDoneSync());
        return ExitNode;
      }
    }
  }
  return nullptr;
}

EDTGraphNode *CARTSGraph::insertNode(EDT *E) {
  EDTGraphNode *EDTNode = new EDTGraphNode(*E);
  EDTs[E->getFn()] = EDTNode;
  return EDTNode;
}

EDTGraphNode *CARTSGraph::insertNode(EDT *E, EDTGraphNode *ParentNode,
                                     Function *Fn) {
  EDTGraphNode *EDTNode = (Fn == nullptr) ? insertNode(E) : insertNode(E, *Fn);
  addCreationEdge(ParentNode, EDTNode);
  return EDTNode;
}

EDTGraphNode *CARTSGraph::insertNode(EDT *E, Function &Fn) {
  EDTGraphNode *EDTNode = new EDTGraphNode(*E);
  EDTs[&Fn] = EDTNode;
  return EDTNode;
}

/// Edges
EDTGraphCreationEdge *CARTSGraph::getEdge(EDT *From, EDT *To) {
  return getEdge(getNode(From), getNode(To));
}

unordered_set<EDTGraphCreationEdge *> CARTSGraph::getIncomingEdges(EDT *Node) {
  return getIncomingEdges(getNode(Node));
}

CARTSGraph::getIncomingEdges(EDTGraphNode *Node) {
  unordered_set<EDTGraphCreationEdge *> Aux;
  if (IncomingEdges.find(Node) == IncomingEdges.end())
    return Aux;
  auto &Tmp = IncomingEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<EDTGraphCreationEdge *> CARTSGraph::getOutgoingEdges(EDT *Node) {
  return getOutgoingEdges(getNode(Node));
}

EDTGraphCreationEdge *CARTSGraph::getEdge(EDTGraphNode *From,
                                          EDTGraphNode *To) {
  /// Fetch the set of edges from @from.
  if (OutgoingEdges.find(From) == OutgoingEdges.end())
    return nullptr;

  /// Fetch the edge to @to
  auto &Tmp = OutgoingEdges[From];
  if (Tmp.find(To) == Tmp.end())
    return nullptr;
  return Tmp[To];
}


unordered_set<EDTGraphCreationEdge *>
CARTSGraph::getOutgoingEdges(EDTGraphNode *Node) {
  unordered_set<EDTGraphCreationEdge *> Aux;
  if (OutgoingEdges.find(Node) == OutgoingEdges.end())
    return Aux;
  auto &Tmp = OutgoingEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<EDTGraphCreationEdge *> CARTSGraph::getEdges(EDTGraphNode *Node) {
  auto IncomingEdges = getIncomingEdges(Node);
  auto OutgoingEdges = getOutgoingEdges(Node);
  IncomingEdges.insert(OutgoingEdges.begin(), OutgoingEdges.end());
  return IncomingEdges;
}

EDTGraphCreationEdge *CARTSGraph::fetchOrCreateEdge(EDTGraphNode *From,
                                                    EDTGraphNode *To,
                                                    bool IsDataEdge) {
  EDTGraphCreationEdge *ExistingEdge = getEdge(From, To);
  if (ExistingEdge == nullptr) {
    /// The edge from @fromNode to @toNode doesn't exist yet
    LLVM_DEBUG(dbgs() << "        - Creating edge from \"EDT #"
                      << From->getEDT()->getID() << "\" to \"EDT #"
                      << To->getEDT()->getID() << "\"\n");
    EDTGraphCreationEdge *NewEdge;
    if (IsDataEdge) {
      NewEdge = new EDTGraphDataEdge(From, To);
      LLVM_DEBUG(dbgs() << "          Data Edge\n");
    } else {
      NewEdge = new EDTGraphCreationEdge(From, To);
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
    LLVM_DEBUG(dbgs() << "        - Converting Control edge to Data edge [\""
                      << From->getEDT()->getName() << "\" -> \""
                      << To->getEDT()->getName() << "\"]\n");
    auto *NewDataEdge = new EDTGraphDataEdge(From, To);
    NewDataEdge->setCreationDep(ExistingEdge->hasCreationDep());
    removeEdge(ExistingEdge);
    addEdge(From, To, NewDataEdge);
    return NewDataEdge;
  }
  // /// The edge is a data edge
  // assert(IsDataEdge &&
  //        "The edge is a Data edge - Can not convert to Control Edge");
  return ExistingEdge;
}

void CARTSGraph::addEdge(EDTGraphNode *From, EDTGraphNode *To,
                         EDTGraphCreationEdge *Edge) {
  auto &Tmp = OutgoingEdges[From];
  Tmp[To] = Edge;
  auto &Tmp2 = IncomingEdges[To];
  Tmp2[From] = Edge;
}

/// Add edges with EDT
EDTGraphCreationEdge *CARTSGraph::addCreationEdge(EDT *From, EDT *To) {
  return addCreationEdge(getNode(From), getNode(To));
}

EDTGraphCreationEdge *CARTSGraph::addDataEdge(EDT *From, EDT *To,
                                              DataBlock *DB) {
  return addDataEdge(getNode(From), getNode(To), DB);
}

EDTGraphCreationEdge *CARTSGraph::addDataEdge(EDT *From, EDT *To,
                                              EDTValue *Parameter) {
  return addDataEdge(getNode(From), getNode(To), Parameter);
}

EDTGraphCreationEdge *CARTSGraph::addDataEdge(EDT *From, EDT *To, EDT *Guid) {
  return addDataEdge(getNode(From), getNode(To), Guid);
}

EDTGraphCreationEdge *CARTSGraph::addControlEdge(EDT *From, EDT *To) {
  return addControlEdge(getNode(From), getNode(To));
}

/// Add edges with EDTGraphNode
EDTGraphCreationEdge *CARTSGraph::addCreationEdge(EDTGraphNode *From,
                                                  EDTGraphNode *To) {
  EDTGraphCreationEdge *CreationEdge = fetchOrCreateEdge(From, To, false);
  CreationEdge->setCreationDep(true);
  return CreationEdge;
}

EDTGraphCreationEdge *CARTSGraph::addDataEdge(EDTGraphNode *From,
                                              EDTGraphNode *To, DataBlock *DB) {
  assert(DB != nullptr && "The DataBlock is null");
  EDTGraphDataEdge *DataEdge =
      dyn_cast<EDTGraphDataEdge>(fetchOrCreateEdge(From, To, true));
  DataEdge->addDataBlock(DB);
  return DataEdge;
}

EDTGraphCreationEdge *CARTSGraph::addDataEdge(EDTGraphNode *From,
                                              EDTGraphNode *To,
                                              EDTValue *Parameter) {
  assert(Parameter != nullptr && "The Parameter is null");
  EDTGraphDataEdge *DataEdge =
      dyn_cast<EDTGraphDataEdge>(fetchOrCreateEdge(From, To, true));
  DataEdge->addParameter(Parameter);
  return DataEdge;
}

EDTGraphCreationEdge *CARTSGraph::addDataEdge(EDTGraphNode *From,
                                              EDTGraphNode *To, EDT *Guid) {
  assert(Guid != nullptr && "The Guid is null");
  EDTGraphDataEdge *DataEdge =
      dyn_cast<EDTGraphDataEdge>(fetchOrCreateEdge(From, To, true));
  DataEdge->addGuid(Guid);
  return DataEdge;
}

EDTGraphCreationEdge *CARTSGraph::addControlEdge(EDTGraphNode *From,
                                                 EDTGraphNode *To) {
  return fetchOrCreateEdge(From, To, false);
}

void CARTSGraph::removeEdge(EDTGraphCreationEdge *Edge) {
  auto *From = Edge->getFrom();
  auto *To = Edge->getTo();
  OutgoingEdges[From].erase(To);
  IncomingEdges[To].erase(From);
  delete Edge;
}

void CARTSGraph::removeEdge(EDTGraphNode *From, EDTGraphNode *To) {
  auto *Edge = getEdge(From, To);
  if (Edge == nullptr)
    return;
  OutgoingEdges[From].erase(To);
  IncomingEdges[To].erase(From);
  delete Edge;
}

void CARTSGraph::print(void) {
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << TAG << "Printing the EDT Graph\n");
  /// Print the outgoing edges.
  for (auto *EDTNode : getNodes()) {
    auto *EDTInfo = EDTNode->getEDT();
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - \n");
    LLVM_DEBUG(dbgs() << "- EDT #" << EDTInfo->getID() << " - \""
                      << EDTInfo->getName() << "\"\n");
    LLVM_DEBUG(dbgs() << "  - Type: " << toString(EDTInfo->getTy()) << "\n");
    /// Data environment
    LLVM_DEBUG(dbgs() << "  - Data Environment:\n");
    auto &DE = EDTInfo->getDataEnv();
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
        EDTGraphNode *From = DepEdge->getFrom();
        EDT *FromEDT = From->getEDT();
        LLVM_DEBUG(dbgs() << "    - [");
        if (DepEdge->isDataEdge()) {
          LLVM_DEBUG(dbgs() << "data");
        } else if (DepEdge->isControlEdge()) {
          LLVM_DEBUG(dbgs() << "control");
        }
        if (DepEdge->hasCreationDep()) {
          LLVM_DEBUG(dbgs() << "/ creation");
        }
        LLVM_DEBUG(dbgs() << "] \"EDT #" << FromEDT->getID() << "\"\n");
      }
    }

    LLVM_DEBUG(dbgs() << "  - Outgoing Edges:\n");
    auto OutEdges = getOutgoingEdges(EDTNode);
    if (OutEdges.size() == 0) {
      LLVM_DEBUG(dbgs() << "    - The EDT has no outgoing edges\n");
    } else {
      /// Print the outgoing edges.
      for (auto *DepEdge : OutEdges) {
        EDTGraphNode *To = DepEdge->getTo();
        EDT *ToEDT = To->getEDT();
        LLVM_DEBUG(dbgs() << "    - [");
        if (DepEdge->isDataEdge()) {
          LLVM_DEBUG(dbgs() << "data");
        } else if (DepEdge->isControlEdge()) {
          LLVM_DEBUG(dbgs() << "control");
        }
        if (DepEdge->hasCreationDep()) {
          LLVM_DEBUG(dbgs() << "/ creation");
        }
        LLVM_DEBUG(dbgs() << "] \"EDT #" << ToEDT->getID() << "\"\n");
        if (DepEdge->isDataEdge()) {
          /// Parameters
          auto *DataEdge = cast<EDTGraphDataEdge>(DepEdge);
          LLVM_DEBUG(dbgs() << "      - Parameters:\n");
          for (auto *P : DataEdge->getParameters()) {
            LLVM_DEBUG(dbgs() << "        - " << *P << "\n");
          }
          /// Guids
          LLVM_DEBUG(dbgs() << "      - Guids:\n");
          for (auto *G : DataEdge->getGuids()) {
            LLVM_DEBUG(dbgs() << "        - EDT #" << G->getID() << "\n");
          }
          /// DataBlocks
          auto &DataBlocks = DataEdge->getDataBlocks();
          LLVM_DEBUG(dbgs() << "      - DataBlocks:\n");
          for (DataBlock *DB : DataBlocks) {
            /// Parent-Child Dependency
            if (DepEdge->hasCreationDep()) {
              LLVM_DEBUG(dbgs() << "        - " << *DB << " / to slot "
                                << DB->getSlot() << "\n");
              continue;
            }

            /// Remote dependencies
            LLVM_DEBUG(dbgs()
                       << "        - " << *DB << " / from slot "
                       << DB->getSlot() << " to " << (DB->getToSlot()) << "\n");
          }
        }
      }
    }
  }
  LLVM_DEBUG(dbgs() << "\n");
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - -\n\n");
}
} // namespace arts