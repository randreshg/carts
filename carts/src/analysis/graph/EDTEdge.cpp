
#include "llvm/Support/Debug.h"
#include <cassert>

#include "carts/analysis/graph/EDTEdge.h"
#include "carts/utils/ARTS.h"

using namespace llvm;
using namespace arts;

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
  LLVM_DEBUG(dbgs() << "  - From: " << From->getEDT()->getName() << "\n");
  LLVM_DEBUG(dbgs() << "  - To: " << To->getEDT()->getName() << "\n");
}

/// EDTGraphControlEdge
EDTGraphControlEdge::EDTGraphControlEdge(EDTGraphNode *From, EDTGraphNode *To)
    : EDTGraphEdge(From, To) {}
EDTGraphControlEdge::~EDTGraphControlEdge() {}

/// EDTGraphDataEdge
EDTGraphDataEdge::EDTGraphDataEdge(EDTGraphNode *From, EDTGraphNode *To)
    : EDTGraphEdge(From, To) {}

EDTGraphDataEdge::~EDTGraphDataEdge() {
  // for (auto *V : Values)
  //   delete V;
}

void EDTGraphDataEdge::addDataBlock(EDTDataBlock *DB) {
  DataBlocks.insert(new EDTGraphDataBlockEdge(this, DB));
}

void EDTGraphDataEdge::removeDataBlock(EDTDataBlock *DB) {
  // auto It =
  //     std::find_if(Values.begin(), Values.end(), [V](EDTGraphDataBlockEdge
  //     *Val) {
  //       return Val->getValue() == V;
  //     });
  // if (It != Values.end()) {
  //   delete *It;
  //   Values.erase(It);
  // }
}

/// EDTGraphDataBlockEdge
EDTGraphDataBlockEdge::EDTGraphDataBlockEdge(EDTGraphDataEdge *Parent,
                                             EDTDataBlock *DB)
    : Parent(Parent), DB(DB){};
EDTGraphDataBlockEdge::~EDTGraphDataBlockEdge() {}

/// EDTGraphNode
EDTGraphNode::EDTGraphNode(EDT &E) : E(E) {}
EDTGraphNode::~EDTGraphNode() {}

void EDTGraphNode::print(void) {
  LLVM_DEBUG(dbgs() << "EDT NODE:\n" << E << "\n");
}
