#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <cassert>
#include <cstdint>

#include "carts/analysis/graph/ARTSEdge.h"
#include "carts/analysis/graph/ARTSGraph.h"
#include "carts/utils/ARTS.h"

using namespace llvm;
using namespace arts;
using namespace std;

/// DEBUG
#define DEBUG_TYPE "arts-graph"
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

unordered_set<EDTGraphSlotNode *> EDTGraphNode::getSlotNodes() {
  unordered_set<EDTGraphSlotNode *> Aux;
  for (auto Pair : IncomingSlotNodes)
    Aux.insert(Pair.second);
  return Aux;
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

/// ARTSGraph
ARTSGraph::ARTSGraph() {}

ARTSGraph::~ARTSGraph() {
  LLVM_DEBUG(dbgs() << TAG << "Destroying the CARTS Graph\n");
  /// Delete the nodes
  for (auto &E : EDTs)
    delete E.second;
  /// Delete the creation edges
  for (auto &E : OutgoingCreationEdges) {
    for (auto &EE : E.second)
      delete EE.second;
  }
  /// Delete the data block edges
  for (auto &E : OutgoingDataBlockEdges) {
    for (auto &EE : E.second)
      delete EE.second;
  }
}

/// Analysis
bool ARTSGraph::isCreationReachable(EDT *From, EDT *To) {
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

bool ARTSGraph::isDataReachable(EDT *From, EDT *To) {
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

bool ARTSGraph::isDataReachable(EDT *From, EDT *To, uint32_t Slot) {
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

EDT *ARTSGraph::getParentSyncEDT(EDT *EDTNode, uint32_t Depth) {
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

bool ARTSGraph::isChild(EDT *Parent, EDT *Child) {
  CreationGraphEdge *Edge = getEdge(getNode(Parent), getNode(Child));
  if (Edge == nullptr)
    return false;
  return true;
}

/// Public Interface
EDTGraphNode *ARTSGraph::getOrCreateNode(EDT *E) {
  /// Check if the node already exists
  EDTGraphNode *EDTNode = getNode(E);
  if (EDTNode)
    return EDTNode;
  /// Create a new node
  EDTNode = new EDTGraphNode(*E);
  EDTs[E->getFn()] = EDTNode;
  return EDTNode;
}

EDTGraphNode *ARTSGraph::getOrCreateNode(EDT *E, EDT *ParentEDT) {
  EDTGraphNode *EDTNode = getOrCreateNode(E);
  insertCreationEdge(getNode(ParentEDT), EDTNode);
  return EDTNode;
}

EDTGraphSlotNode *ARTSGraph::getOrCreateSlotNode(EDT *Parent, uint32_t Slot) {
  EDTGraphNode *ParentNode = getNode(Parent);
  assert(ParentNode != nullptr && "The parent node is null");
  return ParentNode->getOrCreateSlotNode(Slot);
}

bool ARTSGraph::insertCreationEdge(EDT *From, EDT *To) {
  return insertCreationEdge(getNode(From), getNode(To));
}

bool ARTSGraph::insertCreationEdgeGuid(EDT *From, EDT *To, EDT *Guid) {
  CreationGraphEdge *Edge = getEdge(getNode(From), getNode(To));
  assert(Edge != nullptr && "The edge doesn't exist");
  return Edge->insertGuid(Guid);
}

bool ARTSGraph::insertCreationEdgeParameter(EDT *From, EDT *To,
                                            EDTValue *Parameter) {
  CreationGraphEdge *Edge = getEdge(getNode(From), getNode(To));
  assert(Edge != nullptr && "The edge doesn't exist");
  return Edge->insertParameter(Parameter);
}

bool ARTSGraph::insertDataBlockEdge(EDT *From, EDT *To, uint32_t Slot,
                                    DataBlock *DB) {
  EDTGraphNode *FromNode = getNode(From);
  EDTGraphSlotNode *ToNode = getOrCreateSlotNode(To, Slot);
  return insertDataBlockEdge(FromNode, ToNode, DB);
}

/// Private Interface
EDTGraphNode *ARTSGraph::getNode(EDT *E) const { return getNode(*E->getFn()); }

EDTGraphNode *ARTSGraph::getNode(Function &Fn) const {
  auto it = EDTs.find(&Fn);
  if (it != EDTs.end())
    return it->second;
  return nullptr;
}

unordered_set<EDTGraphNode *> ARTSGraph::getNodes() {
  unordered_set<EDTGraphNode *> Aux;
  for (auto Pair : EDTs)
    Aux.insert(Pair.second);
  return Aux;
}

EDTGraphNode *ARTSGraph::getEntryNode() {
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

EDTGraphNode *ARTSGraph::getExitNode() {
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
CreationGraphEdge *ARTSGraph::getEdge(EDTGraphNode *From, EDTGraphNode *To) {
  /// Fetch the set of edges from @from.
  if (OutgoingCreationEdges.find(From) == OutgoingCreationEdges.end())
    return nullptr;

  /// Fetch the edge to @to
  auto &Tmp = OutgoingCreationEdges[From];
  if (Tmp.find(To) == Tmp.end())
    return nullptr;
  return Tmp[To];
}

DataBlockGraphEdge *ARTSGraph::getEdge(EDTGraphNode *From,
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
ARTSGraph::getIncomingCreationEdges(EDTGraphNode *Node) {
  unordered_set<CreationGraphEdge *> Aux;
  if (IncomingCreationEdges.find(Node) == IncomingCreationEdges.end())
    return Aux;
  auto &Tmp = IncomingCreationEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<CreationGraphEdge *>
ARTSGraph::getOutgoingCreationEdges(EDTGraphNode *Node) {
  unordered_set<CreationGraphEdge *> Aux;
  if (OutgoingCreationEdges.find(Node) == OutgoingCreationEdges.end())
    return Aux;
  auto &Tmp = OutgoingCreationEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<DataBlockGraphEdge *>
ARTSGraph::getIncomingDataBlockEdges(EDTGraphNode *Node) {
  unordered_set<DataBlockGraphEdge *> Aux;
  auto EDTSlotNodes = Node->getSlotNodes();
  for (auto &SlotNode : EDTSlotNodes) {
    auto IncomingDataBlocks = getIncomingDataBlockEdges(SlotNode);
    for (DataBlockGraphEdge *DataBlockEdge : IncomingDataBlocks)
      Aux.insert(DataBlockEdge);
  }
  return Aux;
}

unordered_set<DataBlockGraphEdge *>
ARTSGraph::getIncomingDataBlockEdges(EDTGraphSlotNode *Node) {
  unordered_set<DataBlockGraphEdge *> Aux;
  if (IncomingDataBlockEdges.find(Node) == IncomingDataBlockEdges.end())
    return Aux;
  auto &Tmp = IncomingDataBlockEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<DataBlockGraphEdge *>
ARTSGraph::getOutgoingDataBlockEdges(EDTGraphNode *Node) {
  unordered_set<DataBlockGraphEdge *> Aux;
  if (OutgoingDataBlockEdges.find(Node) == OutgoingDataBlockEdges.end())
    return Aux;
  auto &Tmp = OutgoingDataBlockEdges[Node];
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

CreationGraphEdge *ARTSGraph::fetchOrCreateEdge(EDTGraphNode *From,
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

DataBlockGraphEdge *ARTSGraph::fetchOrCreateEdge(EDTGraphNode *From,
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
void ARTSGraph::insertEdge(EDTGraphNode *From, EDTGraphNode *To,
                           CreationGraphEdge *Edge) {
  OutgoingCreationEdges[From][To] = Edge;
  IncomingCreationEdges[To][From] = Edge;
}

void ARTSGraph::insertEdge(EDTGraphNode *From, EDTGraphSlotNode *To,
                           DataBlockGraphEdge *Edge) {
  OutgoingDataBlockEdges[From][To] = Edge;
  IncomingDataBlockEdges[To][From] = Edge;
}

CreationGraphEdge *ARTSGraph::insertCreationEdge(EDTGraphNode *From,
                                                 EDTGraphNode *To) {
  return fetchOrCreateEdge(From, To);
}

DataBlockGraphEdge *ARTSGraph::insertDataBlockEdge(EDTGraphNode *From,
                                                   EDTGraphSlotNode *To,
                                                   DataBlock *DB) {
  return fetchOrCreateEdge(From, To, DB);
}

// void ARTSGraph::removeEdge(CreationGraphEdge *Edge) {
//   auto *From = Edge->getFrom();
//   auto *To = Edge->getTo();
//   OutgoingCreationEdges[From].erase(To);
//   IncomingCreationEdges[To].erase(From);
//   delete Edge;
// }

// void ARTSGraph::removeEdge(EDTGraphNode *From, EDTGraphNode *To) {
//   auto *Edge = getEdge(From, To);
//   if (Edge == nullptr)
//     return;
//   OutgoingCreationEdges[From].erase(To);
//   IncomingCreationEdges[To].erase(From);
//   delete Edge;
// }

void ARTSGraph::print(void) {
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << TAG << "Printing the ARTS Graph\n");
  /// Print the outgoing edges.
  for (EDTGraphNode *EDTNode : getNodes()) {
    EDT *EDTInfo = EDTNode->getEDT();
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - \n");
    LLVM_DEBUG(dbgs() << "- EDT #" << EDTInfo->getID() << " - \""
                      << EDTInfo->getName() << "\"\n");
    LLVM_DEBUG(dbgs() << "  - Type: " << toString(EDTInfo->getTy()) << "\n");
    /// Data environment
    LLVM_DEBUG(dbgs() << "  - Data Environment:\n");
    EDTEnvironment &DE = EDTInfo->getDataEnv();
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
      LLVM_DEBUG(dbgs() << "    - The EDT has no incoming creation edges\n");
    } else {
      /// The EDT must have only one incoming creation edge.
      assert(InEdges.size() == 1 &&
             "The EDT has more than one incoming creation edge");
      CreationGraphEdge *DepEdge = *InEdges.begin();
      EDTGraphNode *FromNode = DepEdge->getFrom();
      EDT *FromEDT = FromNode->getEDT();
      LLVM_DEBUG(dbgs() << "    - [Creation] \"EDT #" << FromEDT->getID()
                        << "\"\n");
    }

    /// Input DataBlock edges
    auto InputSlotNodes = EDTNode->getSlotNodes();
    if (InputSlotNodes.size() == 0) {
      LLVM_DEBUG(dbgs() << "      - The EDT has no input DataBlock slots\n");
    } else {
      for (EDTGraphSlotNode *SlotNode : InputSlotNodes) {
        auto InDataEdges = getIncomingDataBlockEdges(SlotNode);
        if (InDataEdges.size() == 0) {
          LLVM_DEBUG(dbgs() << "      - The EDTSlot #" << SlotNode->getSlot()
                            << " has no incoming data edges\n");
        } else {
          LLVM_DEBUG(dbgs()
                     << "      - EDTSlot #" << SlotNode->getSlot() << "\n");
          for (DataBlockGraphEdge *DataEdge : InDataEdges) {
            DataBlock *DB = DataEdge->getDataBlock();
            LLVM_DEBUG(dbgs()
                       << "        - [DataBlock] " << *DB->getValue() << "\n");
          }
        }
      }
    }

    LLVM_DEBUG(dbgs() << "  - Outgoing Edges:\n");
    auto OutCreationEdges = getOutgoingCreationEdges(EDTNode);
    if (OutCreationEdges.size() == 0) {
      LLVM_DEBUG(dbgs() << "    - The EDT has no outgoing creation edges\n");
    } else {
      /// Print the outgoing edges.
      for (CreationGraphEdge *CreationEdge : OutCreationEdges) {
        EDTGraphNode *To = CreationEdge->getTo();
        EDT *ToEDT = To->getEDT();
        LLVM_DEBUG(dbgs() << "    - [Creation] \"EDT #" << ToEDT->getID()
                          << "\"\n");
      }
    }

    /// Output DataBlock edges
    auto OutDataBlockEdges = getOutgoingDataBlockEdges(EDTNode);
    for (auto &OutDataBlockEdge : OutDataBlockEdges) {
      EDTGraphSlotNode *To = OutDataBlockEdge->getTo();
      EDTGraphNode *ToNode = To->getParent();
      EDT *ToEDT = ToNode->getEDT();
      LLVM_DEBUG(dbgs() << "    - [DataBlock] \"EDT #" << ToEDT->getID()
                        << "\" in Slot #" << To->getSlot() << "\n");
      LLVM_DEBUG(dbgs() << "      - "
                        << *OutDataBlockEdge->getDataBlock()->getValue()
                        << "\n");
    }
  }
  LLVM_DEBUG(dbgs() << "\n");
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - -\n\n");
}
} // namespace arts