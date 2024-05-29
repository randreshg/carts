
#include "llvm/Support/Debug.h"
#include <cassert>

#include "ARTS.h"
#include "EDTEdge.h"

using namespace llvm;
using namespace arts;
using namespace arcana::noelle;

/// DEBUG
#define DEBUG_TYPE "edt-graph"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// EDTGraphEdge
EDTGraphEdge::EDTGraphEdge(EDTGraphNode *From, EDTGraphNode *To)
    : From(From), To(To) {}
EDTGraphEdge::~EDTGraphEdge() {}

void EDTGraphEdge::print(void) {
  LLVM_DEBUG(dbgs() << "EDT EDGE:\n");
  LLVM_DEBUG(dbgs() << "  - From: " << From->getEDT()->getOutlinedFnName()
                    << "\n");
  LLVM_DEBUG(dbgs() << "  - To: " << To->getEDT()->getOutlinedFnName() << "\n");
}

/// EDTGraphControlEdge
EDTGraphControlEdge::EDTGraphControlEdge(EDTGraphNode *From, EDTGraphNode *To)
    : EDTGraphEdge(From, To) {}
EDTGraphControlEdge::~EDTGraphControlEdge() {}

/// EDTGraphDataEdge
EDTGraphDataEdge::EDTGraphDataEdge(EDTGraphNode *From, EDTGraphNode *To)
    : EDTGraphEdge(From, To) {}

EDTGraphDataEdge::~EDTGraphDataEdge() {
  for (auto *V : Values)
    delete V;
}

void EDTGraphDataEdge::addValue(Value *V) {
  Values.insert(new EDTGraphDataEdgeVal(this, V));
}

void EDTGraphDataEdge::removeValue(Value *V) {
  auto It =
      std::find_if(Values.begin(), Values.end(), [V](EDTGraphDataEdgeVal *Val) {
        return Val->getValue() == V;
      });
  if (It != Values.end()) {
    delete *It;
    Values.erase(It);
  }
}

/// EDTGraphDataEdgeVal
EDTGraphDataEdgeVal::EDTGraphDataEdgeVal(EDTGraphDataEdge *Parent, Value *V)
    : Parent(Parent), V(V){};
EDTGraphDataEdgeVal::~EDTGraphDataEdgeVal() {}

/// EDTGraphNode
EDTGraphNode::EDTGraphNode(EDT &E) : E(E) {}
EDTGraphNode::~EDTGraphNode() {}

void EDTGraphNode::print(void) {
  LLVM_DEBUG(dbgs() << "EDT NODE:\n" << E << "\n");
}
