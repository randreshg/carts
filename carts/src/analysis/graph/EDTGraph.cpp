#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <cassert>

#include "carts/analysis/ARTS.h"
#include "carts/analysis/graph/EDTEdge.h"
#include "carts/analysis/graph/EDTGraph.h"
// #include "carts/utils/ARTSUtils.h"

using namespace llvm;
using namespace arts;
// using namespace arts::utils;
using namespace arcana::noelle;
using namespace std;

/// DEBUG
#define DEBUG_TYPE "edt-graph"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// EDTGraph
EDTGraph::EDTGraph(EDTNoelleCache &Cache) : Cache(Cache) {
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Building the EDT Graph\n");
  // createNodes();
  // analyzeDeps();
  /// Debug module
  LLVM_DEBUG(dbgs() << Cache.getModule());
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
}

EDTGraph::~EDTGraph() {
  LLVM_DEBUG(dbgs() << TAG << "Destroying the EDT Graph\n");
  for (auto &E : EDTs)
    delete E.second;
}

/// Nodes
void EDTGraph::createNode(Function &Fn) {
  if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
    return;
  LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - - - - - - - - - -\n ");
  LLVM_DEBUG(dbgs() << TAG << " Processing function: " << Fn.getName() << "\n");
  auto *ParentEDTNode = getNode(Fn);
  assert(ParentEDTNode != nullptr && "ParentEDTNode is null");
  for (auto &Inst : instructions(&Fn)) {
    auto *CB = dyn_cast<CallBase>(&Inst);
    if (!CB)
      continue;
    /// Get the callee
    // omp::Type RTF = omp::getRTFunction(*CB);
    // switch (RTF) {
    // case omp::PARALLEL: {
    //   LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
    //   LLVM_DEBUG(dbgs() << TAG << "Parallel Region Found:\n" << *CB << "\n");
    //   insertNode(new ParallelEDT(Cache, CB), ParentEDTNode);
    //   insertNode(new ParallelDoneEDT(Cache, CB), ParentEDTNode);
    // } break;
    // case omp::TASKALLOC: {
    //   LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
    //   LLVM_DEBUG(dbgs() << TAG << "Task Region Found:\n" << *CB << "\n");
    //   insertNode(new TaskEDT(Cache, CB), ParentEDTNode);
    // } break;
    // case omp::TASKWAIT: {
    //   assert(false && "Taskwait not implemented yet");
    // } break;
    // case omp::OTHER: {
    //   // LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - -\n");
    //   // LLVM_DEBUG(dbgs() << TAG << "Other Function Found:\n" << *CB <<
    //   // "\n"); IPA?
    // } break;
    // default:
    //   continue;
    //   break;
    // }
  }
}

void EDTGraph::createNodes() {
  auto &M = Cache.getModule();
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Creating the EDT Nodes\n");
  /// Create MainEDT
  auto &NM = Cache.getNoelle();
  auto FM = NM.getFunctionsManager();
  // insertNode(new MainEDT(Cache), *FM->getEntryFunction());

  for (auto &Fn : M)
    createNode(Fn);

  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << EDTs.size() << " EDT Nodes were created\n");
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
}

void EDTGraph::analyzeDeps() {
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Analyzing dependencies\n");
  /// Fetch the PDG
  auto &NM = Cache.getNoelle();
  // auto FM = NM.getFunctionsManager();
  auto PDG = NM.getProgramDependenceGraph();

  /// Iterate through the list of EDTs
  for (auto &EDTPair : EDTs) {
    auto *EDTFn = EDTPair.first;
    auto FDG = PDG->createFunctionSubgraph(*EDTFn);

    auto &FromEDTNode = *EDTPair.second;
    auto &FromEDT = *FromEDTNode.getEDT();
    /// Analyze outgoing edges
    auto OutEdges = getOutgoingEdges(&FromEDTNode);
    LLVM_DEBUG(dbgs() << "- EDT \"" << FromEDT.getName() << "\"\n");
    if (OutEdges.size() == 0) {
      LLVM_DEBUG(dbgs() << "   - The EDT doesn't have outgoing edges\n");
      continue;
    }
    /// Analyze the outgoing edges.
    LLVM_DEBUG(dbgs() << "   The EDT has " << OutEdges.size()
                      << " outgoing edges\n");
    for (auto *DepEdge : OutEdges) {
      /// If the edge is not a creation edge, skip it
      if (!DepEdge->hasCreationDep()) {
        /// The edge from @fromNode to @toNode is not a creation edge.
        /// It is a data or control edge.
        LLVM_DEBUG(dbgs() << "   - The edge from \"" << FromEDT.getName()
                          << "\" to \"" << DepEdge->getTo()->getEDT()->getName()
                          << "\" is not a creation edge\n");
        continue;
      }
      auto &ToEDTNode = *DepEdge->getTo();
      auto &ToEDT = *ToEDTNode.getEDT();
      auto *ToEDTCall = ToEDT.getCall();
      assert(ToEDTCall != nullptr && "The EDT doesn't have a call");

      LLVM_DEBUG(dbgs() << "   - EDT \"" << ToEDT.getName()
                        << "\" depends on:\n");
      /// DepV are the values that the EDT depends on.
      auto &ToDE = ToEDT.getDataEnv();
      for (auto &D : ToDE.DepV) {
        LLVM_DEBUG(dbgs() << "     " << *D << " - DATA DEPV\n");
        addDataEdge(&FromEDTNode, &ToEDTNode, D);
      }
      /// Iterate over the rest of dependencies
      auto IterFn = [this, &ToEDT, &FromEDTNode, &ToEDTNode](
                        Value *Src, DGEdge<Value, Value> *Dep) -> bool {
        auto SrcEDTs = Cache.getEDTs(Src);
        if (SrcEDTs.size() == 0)
          return false;
        /// Check if the source is EDTTo
        if (!is_contained(SrcEDTs, &ToEDT))
          return false;
        LLVM_DEBUG(dbgs() << "     " << *Src << " - ");
        /// Control dependencies
        if (isa<ControlDependence<Value, Value>>(Dep)) {
          LLVM_DEBUG(dbgs() << " CONTROL\n");
          assert(false && "Not implemented yet");
          return false;
        }
        /// Data dependencies -> Registers
        LLVM_DEBUG(dbgs() << " DATA\n");
        auto DataDep = cast<DataDependence<Value, Value>>(Dep);
        // if (DataDep->isRAWDependence()) {
        //   LLVM_DEBUG(dbgs() << " RAW ");
        // }
        // if (DataDep->isWARDependence()) {
        //   LLVM_DEBUG(dbgs() << " WAR ");
        // }
        // if (DataDep->isWAWDependence()) {
        //   LLVM_DEBUG(dbgs() << " WAW ");
        // }
        /// Further analysis has to be performed here to guarantee that the
        /// dependence is a memory dependence. Leave it as it is for now.
        if (isa<MemoryDependence<Value, Value>>(DataDep)) {
          LLVM_DEBUG(dbgs() << " MEMORY\n");
          // auto memDep = cast<MemoryDependence<Value, Value>>(DataDep);
          // if (isa<MustMemoryDependence<Value, Value>>(memDep)) {
          //   LLVM_DEBUG(dbgs() << " MUST ");
          // } else {
          //   LLVM_DEBUG(dbgs() << " MAY ");
          // }
        }

        /// Create the data edge
        addDataEdge(&FromEDTNode, &ToEDTNode, Src);
        // LLVM_DEBUG(dbgs() << "\n");
        return false;
      };
      FDG->iterateOverDependencesTo(ToEDTCall, true, true, true, IterFn);
    }
  }
}

unordered_set<EDTGraphNode *> EDTGraph::getNodes() {
  unordered_set<EDTGraphNode *> Aux;
  for (auto Pair : EDTs) {
    // auto *Fn = Pair.first;
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
  auto &Tmp = OutgoingEdges.at(From);
  if (Tmp.find(To) == Tmp.end())
    return nullptr;
  return Tmp.at(To);
}

unordered_set<EDTGraphEdge *> EDTGraph::getIncomingEdges(EDTGraphNode *Node) {
  unordered_set<EDTGraphEdge *> Aux;
  if (IncomingEdges.find(Node) == IncomingEdges.end())
    return Aux;
  auto &Tmp = IncomingEdges.at(Node);
  for (auto &E : Tmp)
    Aux.insert(E.second);
  return Aux;
}

unordered_set<EDTGraphEdge *> EDTGraph::getOutgoingEdges(EDTGraphNode *Node) {
  unordered_set<EDTGraphEdge *> Aux;
  if (OutgoingEdges.find(Node) == OutgoingEdges.end())
    return Aux;
  auto &Tmp = OutgoingEdges.at(Node);
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
    LLVM_DEBUG(dbgs() << "        - Creating edge from \""
                      << From->getEDT()->getName() << "\" to \""
                      << To->getEDT()->getName() << "\"\n");
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
    LLVM_DEBUG(dbgs() << "        - Converting edge from \""
                      << From->getEDT()->getName() << "\" to \""
                      << To->getEDT()->getName() << "\"\n");
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
    LLVM_DEBUG(dbgs() << "- EDT \"" << E->getName() << "\"\n");
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
        if (DepEdge->hasCreationDep()) {
          LLVM_DEBUG(dbgs() << "creation");
        } else if (DepEdge->isDataEdge()) {
          LLVM_DEBUG(dbgs() << "data");
        } else if (DepEdge->isControlEdge()) {
          LLVM_DEBUG(dbgs() << "control");
        }
        LLVM_DEBUG(dbgs() << "] \"" << FromE->getName() << "\"\n");
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
        if (DepEdge->hasCreationDep()) {
          LLVM_DEBUG(dbgs() << "creation");
        } else if (DepEdge->isDataEdge()) {
          LLVM_DEBUG(dbgs() << "data");
        } else if (DepEdge->isControlEdge()) {
          LLVM_DEBUG(dbgs() << "control");
        }
        LLVM_DEBUG(dbgs() << "] \"" << ToE->getName() << "\"\n");
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