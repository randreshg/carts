
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <cassert>

#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"

#include "carts/analysis/graph/EDTEdge.h"
#include "carts/analysis/graph/EDTGraph.h"
#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSMetadata.h"
#include "noelle/core/CallGraphNode.hpp"
#include "noelle/core/Noelle.hpp"

using namespace llvm;
using namespace arts;
using namespace arcana::noelle;
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
//   LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
//   LLVM_DEBUG(dbgs() << TAG << "Building the EDT Graph\n");
//   FM = Cache.getNoelle().getFunctionsManager();
//   CG = FM->getProgramCallGraph();
//   /// Create nodes
//   createNodes();
//   setCreationDeps();
//   /// Debug Cache
//   LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
//   LLVM_DEBUG(dbgs() << TAG << "Printing the Cache\n");
//   for (auto &ValItr : Cache.getValues()) {
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
//   LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
//   analyzeDeps();
//   /// Debug module
//   // LLVM_DEBUG(dbgs() << Cache.getModule());
//   LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
// }

EDTGraph::~EDTGraph() {
  LLVM_DEBUG(dbgs() << TAG << "Destroying the EDT Graph\n");
  for (auto &E : EDTs)
    delete E.second;
}

/// Nodes
void EDTGraph::createNode(Function &Fn) {
  if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
    return;
  LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << TAG << " Processing function: " << Fn.getName() << "\n");

  EDTMetadata *MD = EDTMetadata::getMetadata(Fn);
  if (MD == nullptr)
    return;
  auto *E = EDT::get(MD);
  assert(E != nullptr && "The EDT is null");
  insertNode(E);
  /// Free memory
  delete MD;
}

void EDTGraph::createNodes() {
  // auto &M = Cache.getModule();
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Creating the EDT Nodes\n");
  // for (auto &Fn : M)
  //   createNode(Fn);

  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << EDTs.size() << " EDT Nodes were created\n");
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
}

void EDTGraph::setDeps(EDTGraphNode *Node) {
  auto *ParentEDTNode = Node;
  if (!ParentEDTNode)
    return;
  auto *ParentEDT = ParentEDTNode->getEDT();
  const auto ParentID = ParentEDT->getID();
  auto *ParentEDTFn = ParentEDT->getFn();
  auto *ParentCGNode = CG->getFunctionNode(ParentEDTFn);
  /// Fetch the outgoing edges.
  auto CallOutEdges = CG->getOutgoingEdges(ParentCGNode);
  if (CallOutEdges.size() == 0) {
    LLVM_DEBUG(dbgs() << " The EDT #" << ParentID
                      << "doesn't create any other EDTs\n");
    return;
  }

  /// Creation dependencies
  LLVM_DEBUG(dbgs() << "The EDT #" << ParentID
                    << " creates the following EDTs:\n");
  for (auto *ChildEdge : CallOutEdges) {
    auto *ChildNode = ChildEdge->getCallee();
    auto *ChildEDTNode = getNode(*ChildNode->getFunction());
    if (!ChildEDTNode)
      continue;
    LLVM_DEBUG(dbgs() << "   [");
    if (ChildEdge->isAMustCall()) {
      LLVM_DEBUG(dbgs() << "must");
    } else {
      LLVM_DEBUG(dbgs() << "may");
      assert(false && "Not implemented yet");
    }
    LLVM_DEBUG(dbgs() << "] EDT #" << ChildEDTNode->getEDT()->getID() << "\n");

    /// Print the sub-edges.
    for (auto ChildSubEdge : ChildEdge->getSubEdges()) {
      errs() << "     [";
      if (ChildSubEdge->isAMustCall()) {
        LLVM_DEBUG(dbgs() << "must");
      } else {
        LLVM_DEBUG(dbgs() << "may");
        assert(false && "Not implemented yet");
      }
      auto *ChildEDTCall =
          dyn_cast<CallBase>(ChildSubEdge->getCaller()->getInstruction());
      LLVM_DEBUG(dbgs() << "] \"" << *ChildEDTCall << "\"\n");
      ChildEDTNode->getEDT()->setCall(ChildEDTCall);
      addCreationEdge(ParentEDTNode, ChildEDTNode);
    }
  }

  /// Data dependencies
  auto &MSSA = Cache.getMemorySSA(*ParentEDTFn);
  /// Iterate though the edges
  auto OutEdges = getOutgoingEdges(ParentEDTNode);
  for (auto *DepEdge : OutEdges) {
    auto *ToEDTNode = DepEdge->getTo();
    auto &ToEDT = *ToEDTNode->getEDT();
    auto *ToEDTCall = ToEDT.getCall();
    LLVM_DEBUG(dbgs() << "   - EDTChild #" << ToEDT.getID() << "\n");
    LLVM_DEBUG(dbgs() << "     - Its call is : "
                      << ToEDTCall->getCalledFunction()->getName() << "\n");
    /// Get MemSSA definition of ToEDT Call
    auto *ClobberingEDT = getClobberingEDT(MSSA, ToEDTCall);
  }
}

void EDTGraph::setCreationDeps() {
  /// Analyze the CallGraph
  auto FunctionNodes = CG->getFunctionNodes(true);
  for (auto Node : FunctionNodes) {
    auto *NodeFn = Node->getFunction();
    auto *ParentEDTNode = getNode(*NodeFn);
    if (!ParentEDTNode)
      continue;
    /// Fetch the outgoing edges.
  }
}

EDTGraphNode *EDTGraph::getClobberingEDT(MemorySSA &MSSA, CallBase *Inst) {
  MemorySSAWalker *Walker = MSSA.getWalker();
  SmallVector<MemoryAccess *> WorkList{Walker->getClobberingMemoryAccess(Inst)};
  SmallSet<MemoryAccess *, 8> Visited;
  // MemoryLocation Loc(MemoryLocation::get(Inst));

  LLVM_DEBUG(dbgs() << "     Checking clobbering of: "
                    << Inst->getCalledFunction()->getName() << '\n');

  while (!WorkList.empty()) {
    MemoryAccess *MA = WorkList.pop_back_val();
    if (!Visited.insert(MA).second)
      continue;
    // MA->dump();
    if (MemoryDef *Def = dyn_cast<MemoryDef>(MA)) {
      /// Check if it is a call to an EDT
      Instruction *DefInst = Def->getMemoryInst();
      if (!DefInst) {
        /// Is it live on entry?
        if (MSSA.isLiveOnEntryDef(MA)) {
          LLVM_DEBUG(dbgs() << "        - Data dependency with Parent EDT\n");
          /// Ignore for now, we will out this information, after we have the
          /// other dependencies.
          /// NOTE: What about phi nodes?
        }
        continue;
      }
      /// Is it a call?
      if (auto *Call = dyn_cast<CallBase>(DefInst)) {
        if (auto *EDTNode = getNode(*Call->getCalledFunction())) {
          /// Is it really a dependency?
          LLVM_DEBUG(dbgs() << "        - Data dependency with EDT #"
                            << EDTNode->getEDT()->getID() << "\n");
        }
      }
      /// Push work to the WorkList
      WorkList.push_back(Def->getDefiningAccess());
      continue;
    }
    /// Push work to the WorkList
    if (MemoryPhi *Phi = cast<MemoryPhi>(MA)) {
      for (auto &Use : Phi->incoming_values())
        WorkList.push_back(cast<MemoryAccess>(&Use));
    }
  }

  return nullptr;
}

void EDTGraph::analyzeDeps() {
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Analyzing dependencies\n");
  /// Fetch the PDG
  auto &NM = Cache.getNoelle();
  auto PDG = NM.getProgramDependenceGraph();
  /// Analyze Memory SSA
  /// Iterate through the EDT Nodes
  auto EDTNodes = getNodes();
  for (auto *EDTNode : EDTNodes) {
    auto &EDT = *EDTNode->getEDT();
    LLVM_DEBUG(dbgs() << " - EDT #" << EDT.getID() << "\n");
    auto &EDTFn = *EDT.getFn();
    /// Get the Memory SSA for the function
    auto &MSSA = Cache.getMemorySSA(EDTFn);
    MSSA.print(dbgs());
    /// Iterate though the edges
    auto OutEdges = getOutgoingEdges(EDTNode);
    for (auto *DepEdge : OutEdges) {
      auto *ToEDTNode = DepEdge->getTo();
      auto &ToEDT = *ToEDTNode->getEDT();
      auto *ToEDTCall = ToEDT.getCall();
      LLVM_DEBUG(dbgs() << "   - EDTChild #" << ToEDT.getID() << "\n");
      LLVM_DEBUG(dbgs() << "     - Its call is : "
                        << ToEDTCall->getCalledFunction()->getName() << "\n");
      /// Get MemSSA definition of ToEDT Call
      auto *ClobberingEDT = getClobberingEDT(MSSA, ToEDTCall);
    }

    LLVM_DEBUG(
        dbgs() << "\n\n-------------------------------------------------\n");
    /// For every Value that is a dependency in an EDT, check if it is used by
    /// another EDT.
    // auto &DepValues = Cache.getValues();
    // for (auto &DepValItr : DepValues) {
    //   auto *EDTDep = DepValItr.first;
    //   LLVM_DEBUG(dbgs() << " - Value: " << *EDTDep << "\n");
    //   auto &EDTs = DepValItr.second;
    //   auto FDG =
    //       PDG->createFunctionSubgraph(*EDTs[0]->getCall()->getFunction());

    //   auto IterFn = [&](Value *Src, DGEdge<Value, Value> *Dep) -> bool {
    //     LLVM_DEBUG(dbgs() << "     " << *Src << " - ");
    //     /// Control dependencies
    //     if (isa<ControlDependence<Value, Value>>(Dep)) {
    //       LLVM_DEBUG(dbgs() << " CONTROL\n");
    //       assert(false && "Not implemented yet");
    //       return false;
    //     }
    //     /// Data dependencies -> Registers
    //     LLVM_DEBUG(dbgs() << " DATA ");
    //     auto DataDep = cast<DataDependence<Value, Value>>(Dep);
    //     if (DataDep->isRAWDependence()) {
    //       LLVM_DEBUG(dbgs() << " RAW ");
    //     }
    //     if (DataDep->isWARDependence()) {
    //       LLVM_DEBUG(dbgs() << " WAR ");
    //     }
    //     if (DataDep->isWAWDependence()) {
    //       LLVM_DEBUG(dbgs() << " WAW ");
    //     }
    //     /// Further analysis has to be performed here to guarantee that the
    //     /// dependence is a memory dependence. Leave it as it is for now.
    //     if (isa<MemoryDependence<Value, Value>>(DataDep)) {
    //       LLVM_DEBUG(dbgs() << " MEMORY");
    //       auto memDep = cast<MemoryDependence<Value, Value>>(DataDep);
    //       if (isa<MustMemoryDependence<Value, Value>>(memDep)) {
    //         LLVM_DEBUG(dbgs() << "-> MUST ");
    //       } else {
    //         LLVM_DEBUG(dbgs() << "-> MAY ");
    //       }
    //     }

    //     /// Create the data edge
    //     LLVM_DEBUG(dbgs() << "\n");
    //     // addDataEdge(&FromEDTNode, &ToEDTNode, Src);
    //     return false;
    //   };
    //   FDG->iterateOverDependencesTo(EDTDep, true, true, true, IterFn);

    //   // auto EDTC = EDTs.size();
    //   // if (EDTC == 0) {
    //   //   LLVM_DEBUG(dbgs() << "   - The value " << *V
    //   //                     << " is not used by any EDTCall\n");
    //   //   continue;
    //   // }
    //   // LLVM_DEBUG(dbgs() << "   - The value " << *V << " is used by " <<
    //   EDTC
    //   //                   << " EDTCalls\n");
    //   // for (auto *E : EDTs) {
    //   //   LLVM_DEBUG(dbgs() << "     - EDT #" << E->getID() << "\n");
    //   // }
    // }
  }
  /// Iterate through the list of EDTs
  // for (auto &EDTPair : EDTs) {
  //   auto *EDTFn = EDTPair.first;
  //   auto FDG = PDG->createFunctionSubgraph(*EDTFn);
  //   auto &FromEDTNode = *EDTPair.second;
  //   auto &FromEDT = *FromEDTNode.getEDT();
  //   auto FromEDTID = FromEDT.getID();
  //   /// Analyze outgoing edges
  //   auto OutEdges = getOutgoingEdges(&FromEDTNode);
  //   LLVM_DEBUG(dbgs() << "- EDT #" << FromEDTID << "\n");
  //   if (OutEdges.size() == 0) {
  //     LLVM_DEBUG(dbgs() << "   - It doesn't have outgoing edges\n");
  //     continue;
  //   }
  //   /// Analyze the outgoing edges.
  //   LLVM_DEBUG(dbgs() << "   It has " << OutEdges.size()
  //                     << " outgoing edges\n");
  //   for (auto *DepEdge : OutEdges) {
  //     /// If the edge is not a creation edge, skip it
  //     if (!DepEdge->hasCreationDep()) {
  //       /// The edge from @fromNode to @toNode is not a creation edge.
  //       /// It is a data or control edge.
  //       LLVM_DEBUG(dbgs() << "   - The edge from EDT #" << FromEDTID
  //                         << " to EDT #" <<
  //                         DepEdge->getTo()->getEDT()->getID()
  //                         << " is not a creation edge\n");
  //       continue;
  //     }
  //     auto &ToEDTNode = *DepEdge->getTo();
  //     auto &ToEDT = *ToEDTNode.getEDT();
  //     auto *ToEDTCall = ToEDT.getCall();
  //     assert(ToEDTCall != nullptr && "The EDT doesn't have a call");
  //     LLVM_DEBUG(dbgs() << "   - EDT #" << ToEDT.getID() << " depends
  //     on:\n");
  //     /// DepV are the values that the EDT depends on.
  //     auto &ToDE = ToEDT.getDataEnv();
  //     /// Iterate over the rest of dependencies
  //     auto IterFn = [this, &ToEDT, &FromEDTNode, &ToEDTNode](
  //                       Value *Src, DGEdge<Value, Value> *Dep) -> bool {
  //       auto SrcEDTs = Cache.getEDTs(Src);
  //       auto SrcEDTsLen = SrcEDTs.size();
  //       if (SrcEDTsLen == 0) {
  //         // LLVM_DEBUG(dbgs() << "     " << *Src << " - Is not used by any
  //         // EDTCall\n");
  //         return false;
  //       }
  //       /// Check if the source is EDTTo
  //       if (!is_contained(SrcEDTs, &ToEDT)) {
  //         LLVM_DEBUG(dbgs() << "     " << *Src
  //                           << " - Is a dep that is not sent to the
  //                           EDT\n");
  //         return false;
  //       }
  //       LLVM_DEBUG(dbgs() << "     " << *Src << " - ");
  //       /// Control dependencies
  //       if (isa<ControlDependence<Value, Value>>(Dep)) {
  //         LLVM_DEBUG(dbgs() << " CONTROL\n");
  //         assert(false && "Not implemented yet");
  //         return false;
  //       }
  //       /// Data dependencies -> Registers
  //       LLVM_DEBUG(dbgs() << " DATA ");
  //       auto DataDep = cast<DataDependence<Value, Value>>(Dep);
  //       // if (DataDep->isRAWDependence()) {
  //       //   LLVM_DEBUG(dbgs() << " RAW ");
  //       // }
  //       // if (DataDep->isWARDependence()) {
  //       //   LLVM_DEBUG(dbgs() << " WAR ");
  //       // }
  //       // if (DataDep->isWAWDependence()) {
  //       //   LLVM_DEBUG(dbgs() << " WAW ");
  //       // }
  //       /// Further analysis has to be performed here to guarantee that the
  //       /// dependence is a memory dependence. Leave it as it is for now.
  //       if (isa<MemoryDependence<Value, Value>>(DataDep)) {
  //         LLVM_DEBUG(dbgs() << " MEMORY");
  //         auto memDep = cast<MemoryDependence<Value, Value>>(DataDep);
  //         if (isa<MustMemoryDependence<Value, Value>>(memDep)) {
  //           LLVM_DEBUG(dbgs() << "-> MUST ");
  //         } else {
  //           LLVM_DEBUG(dbgs() << "-> MAY ");
  //         }
  //       }
  //       if (SrcEDTsLen > 1) {
  //         LLVM_DEBUG(dbgs() << " (The value is used by more than one
  //         EDT)");
  //       }
  //       /// Create the data edge
  //       LLVM_DEBUG(dbgs() << "\n");
  //       addDataEdge(&FromEDTNode, &ToEDTNode, Src);
  //       return false;
  //     };
  //     FDG->iterateOverDependencesTo(ToEDTCall, true, true, true, IterFn);
  //   }
  // }
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