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

/// EDTGraphNode
EDTGraphNode::EDTGraphNode(EDT &E) : E(E) {}
EDTGraphNode::~EDTGraphNode() {
  for (auto &E : IncomingSlotNodes)
    delete E.second;
}

void EDTGraphNode::print(void) {
  LLVM_DEBUG(dbgs() << "EDT NODE:\n" << E << "\n");
}

EDT *EDTGraphNode::getEDT() { return &E; }

EDTGraphSlotNode *EDTGraphNode::getOrCreateSlotNode(uint32_t Slot) {
  auto it = IncomingSlotNodes.find(Slot);
  if (it != IncomingSlotNodes.end())
    return it->second;
  EDTGraphSlotNode *SlotNode = new EDTGraphSlotNode(this, Slot);
  IncomingSlotNodes[Slot] = SlotNode;
  return SlotNode;
}

/// EDTGraphSlotNode
EDTGraphSlotNode::EDTGraphSlotNode(EDTGraphNode *Parent, uint32_t Slot)
    : Parent(Parent), Slot(Slot) {}
EDTGraphSlotNode::~EDTGraphSlotNode() {}

void EDTGraphSlotNode::print(void) {
  LLVM_DEBUG(dbgs() << "EDT SLOT NODE:\n");
  LLVM_DEBUG(dbgs() << "  - Parent: " << Parent->getEDT()->getName() << "\n");
  LLVM_DEBUG(dbgs() << "  - Slot: " << Slot << "\n");
}

EDTGraphNode *EDTGraphSlotNode::getParent() { return Parent; }
uint32_t EDTGraphSlotNode::getSlot() { return Slot; }

bool EDTGraphSlotNode::insertIncomingDataEdge(DataBlockGraphEdge *Edge) {
  return IncomingDataEdges.insert(Edge);
}

SetVector<DataBlockGraphEdge *> &EDTGraphSlotNode::getIncomingDataEdges() {
  return IncomingDataEdges;
}

/// CARTSGraph
CARTSGraph::CARTSGraph() {}

CARTSGraph::~CARTSGraph() {
  LLVM_DEBUG(dbgs() << TAG << "Destroying the CARTS Graph\n");
  for (auto &E : EDTs)
    delete E.second;
}

/// Analysis
bool CARTSGraph::isCreationReachable(EDT *From, EDT *To) {
  EDTGraphNode *FromNode = getNode(From);
  EDTGraphNode *ToNode = getNode(To);
  for (auto *Edge : getOutgoingCreationEdges(FromNode)) {
    if (Edge->getTo() == ToNode)
      return true;
    else if (isCreationReachable(Edge->getTo()->getEDT(), To))
      return true;
  }
  return false;
}

bool CARTSGraph::isDataReachable(EDT *From, EDT *To) {
  EDTGraphNode *FromNode = getNode(From);
  EDTGraphNode *ToNode = getNode(To);
  for (DataBlockGraphEdge *Edge : getOutgoingDataBlockEdges(FromNode)) {
    EDTGraphNode *EdgeNode = Edge->getTo()->getParent();
    if (EdgeNode == ToNode)
      return true;
    else if (isDataReachable(EdgeNode->getEDT(), To))
      return true;
  }
  return false;
}

bool CARTSGraph::isDataReachable(EDT *From, EDT *To, uint32_t Slot) {
  EDTGraphNode *FromNode = getNode(From);
  EDTGraphNode *ToNode = getNode(To);
  for (DataBlockGraphEdge *Edge : getOutgoingDataBlockEdges(FromNode)) {
    EDTGraphSlotNode *EdgeNode = Edge->getTo();
    if (EdgeNode->getParent() == ToNode && EdgeNode->getSlot() == Slot)
      return true;
    else if (isDataReachable(EdgeNode->getParent()->getEDT(), To, Slot))
      return true;
  }
  return false;
}

EDT *CARTSGraph::getParentSyncEDT(EDT *EDTNode, uint32_t Depth) {
  EDTGraphNode *Node = nullptr;
  for (CreationGraphEdge *Edge : getIncomingCreationEdges(Node)) {
    EDT *ParentEDT = Edge->getFrom()->getEDT();
    if (ParentEDT->isAsync())
      return getParentSyncEDT(ParentEDT, Depth + 1);

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
  CreationGraphEdge *Edge = getEdge(getNode(Parent), getNode(Child));
  if (Edge == nullptr)
    return false;
  return true;
}

/// Public Interface
EDTGraphNode *CARTSGraph::getOrCreateNode(EDT *E) {
  /// Check if the node already exists
  EDTGraphNode *EDTNode = getNode(E);
  if (EDTNode)
    return EDTNode;
  /// Create a new node
  EDTNode = new EDTGraphNode(*E);
  EDTs[E->getFn()] = EDTNode;
  return EDTNode;
}

EDTGraphNode *CARTSGraph::getOrCreateNode(EDT *E, EDT *ParentEDT) {
  EDTGraphNode *EDTNode = getOrCreateNode(E);
  insertCreationEdge(getNode(ParentEDT), EDTNode);
  return EDTNode;
}

EDTGraphSlotNode *CARTSGraph::getOrCreateSlotNode(EDT *Parent, uint32_t Slot) {
  EDTGraphNode *ParentNode = getNode(Parent);
  assert(ParentNode != nullptr && "The parent node is null");
  return ParentNode->getOrCreateSlotNode(Slot);
}

bool CARTSGraph::insertCreationEdge(EDT *From, EDT *To) {
  return insertCreationEdge(getNode(From), getNode(To));
}

bool CARTSGraph::insertCreationEdgeGuid(EDT *From, EDT *To, EDT *Guid) {
  CreationGraphEdge *Edge = getEdge(getNode(From), getNode(To));
  assert(Edge != nullptr && "The edge doesn't exist");
  return Edge->insertGuid(Guid);
}

bool CARTSGraph::insertCreationEdgeParameter(EDT *From, EDT *To,
                                             EDTValue *Parameter) {
  CreationGraphEdge *Edge = getEdge(getNode(From), getNode(To));
  assert(Edge != nullptr && "The edge doesn't exist");
  return Edge->insertParameter(Parameter);
}

bool CARTSGraph::insertDataBlockEdge(EDT *From, EDT *To, uint32_t Slot,
                                     DataBlock *DB) {
  EDTGraphNode *FromNode = getNode(From);
  EDTGraphSlotNode *ToNode = getOrCreateSlotNode(To, Slot);
  return insertDataBlockEdge(FromNode, ToNode, DB);
}

/// Private Interface
EDTGraphNode *CARTSGraph::getNode(EDT *E) const { return getNode(*E->getFn()); }

EDTGraphNode *CARTSGraph::getNode(Function &Fn) const {
  auto it = EDTs.find(&Fn);
  if (it != EDTs.end())
    return it->second;
  return nullptr;
}

unordered_set<EDTGraphNode *> CARTSGraph::getNodes() {
  unordered_set<EDTGraphNode *> Aux;
  for (auto Pair : EDTs)
    Aux.insert(Pair.second);
  return Aux;
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
  for (auto *Edge : getOutgoingCreationEdges(Entry)) {
    /// The exit node is created by the Entry node and is a sync Done region
    EDT *ToEDT = Edge->getTo()->getEDT();
    if (!ToEDT->isAsync()) {
      ExitNode = getNode(ToEDT->getDoneSync());
      return ExitNode;
    }
  }
  return nullptr;
}

/// Edges
CreationGraphEdge *CARTSGraph::getEdge(EDTGraphNode *From, EDTGraphNode *To) {
  /// Fetch the set of edges from @from.
  if (OutgoingCreationEdges.find(From) == OutgoingCreationEdges.end())
    return nullptr;

  /// Fetch the edge to @to
  auto &Tmp = OutgoingCreationEdges[From];
  if (Tmp.find(To) == Tmp.end())
    return nullptr;
  return Tmp[To];
}

DataBlockGraphEdge *CARTSGraph::getEdge(EDTGraphNode *From,
                                        EDTGraphSlotNode *To) {
  /// Fetch the set of edges from @from.
  if (OutgoingDataBlockEdges.find(From) == OutgoingDataBlockEdges.end())
    return nullptr;

  /// Fetch the edge to @to
  auto &Tmp = OutgoingDataBlockEdges[From];
  if (Tmp.find(To) == Tmp.end())
    return nullptr;
  return Tmp[To];
}

unordered_set<CreationGraphEdge *>
CARTSGraph::getIncomingCreationEdges(EDTGraphNode *Node) {
  unordered_set<CreationGraphEdge *> Aux;
  if (IncomingCreationEdges.find(Node) == IncomingCreationEdges.end())
    return Aux;
  auto &Tmp = IncomingCreationEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<CreationGraphEdge *>
CARTSGraph::getOutgoingCreationEdges(EDTGraphNode *Node) {
  unordered_set<CreationGraphEdge *> Aux;
  if (OutgoingCreationEdges.find(Node) == OutgoingCreationEdges.end())
    return Aux;
  auto &Tmp = OutgoingCreationEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<DataBlockGraphEdge *>
CARTSGraph::getIncomingDataBlockEdges(EDTGraphSlotNode *Node) {
  unordered_set<DataBlockGraphEdge *> Aux;
  if (IncomingDataBlockEdges.find(Node) == IncomingDataBlockEdges.end())
    return Aux;
  auto &Tmp = IncomingDataBlockEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<DataBlockGraphEdge *>
CARTSGraph::getOutgoingDataBlockEdges(EDTGraphNode *Node) {
  unordered_set<DataBlockGraphEdge *> Aux;
  if (OutgoingDataBlockEdges.find(Node) == OutgoingDataBlockEdges.end())
    return Aux;
  auto &Tmp = OutgoingDataBlockEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

CreationGraphEdge *CARTSGraph::fetchOrCreateEdge(EDTGraphNode *From,
                                                 EDTGraphNode *To) {
  CreationGraphEdge *ExistingEdge = getEdge(From, To);
  if (ExistingEdge)
    return ExistingEdge;

  /// The edge from @fromNode to @toNode doesn't exist yet
  LLVM_DEBUG(dbgs() << "        - Creating CreatingEdge from \"EDT #"
                    << From->getEDT()->getID() << "\" to \"EDT #"
                    << To->getEDT()->getID() << "\"\n");
  CreationGraphEdge *NewEdge = new CreationGraphEdge(From, To);
  insertEdge(From, To, NewEdge);
  return NewEdge;
}

DataBlockGraphEdge *CARTSGraph::fetchOrCreateEdge(EDTGraphNode *From,
                                                  EDTGraphSlotNode *To,
                                                  DataBlock *DB) {
  DataBlockGraphEdge *ExistingEdge = getEdge(From, To);
  if (ExistingEdge)
    return ExistingEdge;

  /// The edge from @fromNode to @toNode doesn't exist yet
  LLVM_DEBUG(dbgs() << "        - Creating DataBlockEdge from \"EDT #"
                    << From->getEDT()->getID() << "\" to \"EDT #"
                    << To->getParent()->getEDT()->getID() << "\" in Slot #"
                    << To->getSlot() << "\n");
  DataBlockGraphEdge *NewEdge = new DataBlockGraphEdge(From, To, DB);
  insertEdge(From, To, NewEdge);
  return NewEdge;
}

/// Insert edges
void CARTSGraph::insertEdge(EDTGraphNode *From, EDTGraphNode *To,
                            CreationGraphEdge *Edge) {
  auto &Tmp = OutgoingCreationEdges[From];
  Tmp[To] = Edge;
  auto &Tmp2 = IncomingCreationEdges[To];
  Tmp2[From] = Edge;
}

void CARTSGraph::insertEdge(EDTGraphNode *From, EDTGraphSlotNode *To,
                            DataBlockGraphEdge *Edge) {
  auto &Tmp = OutgoingDataBlockEdges[From];
  Tmp[To] = Edge;
  auto &Tmp2 = IncomingDataBlockEdges[To];
  Tmp2[From] = Edge;
}

CreationGraphEdge *CARTSGraph::insertCreationEdge(EDTGraphNode *From,
                                                  EDTGraphNode *To) {
  return fetchOrCreateEdge(From, To);
}

DataBlockGraphEdge *CARTSGraph::insertDataBlockEdge(EDTGraphNode *From,
                                                    EDTGraphSlotNode *To,
                                                    DataBlock *DB) {
  return fetchOrCreateEdge(From, To, DB);
}

void CARTSGraph::removeEdge(CreationGraphEdge *Edge) {
  auto *From = Edge->getFrom();
  auto *To = Edge->getTo();
  OutgoingCreationEdges[From].erase(To);
  IncomingCreationEdges[To].erase(From);
  delete Edge;
}

void CARTSGraph::removeEdge(EDTGraphNode *From, EDTGraphNode *To) {
  auto *Edge = getEdge(From, To);
  if (Edge == nullptr)
    return;
  OutgoingCreationEdges[From].erase(To);
  IncomingCreationEdges[To].erase(From);
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
    auto InEdges = getIncomingCreationEdges(EDTNode);
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
    auto OutEdges = getOutgoingCreationEdges(EDTNode);
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