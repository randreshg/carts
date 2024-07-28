
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

bool EDTGraphDataEdge::addDataBlock(EDTDataBlock *DB) {
  return DataBlocks.insert(DB);
}

bool EDTGraphDataEdge::addParameter(EDTValue *V) {
  return Parameters.insert(V);
}

bool EDTGraphDataEdge::addGuid(EDT *Guid) { return Guids.insert(Guid); }

bool EDTGraphDataEdge::removeDataBlock(EDTDataBlock *DB) {
  return DataBlocks.remove(DB);
}

bool EDTGraphDataEdge::removeParameter(EDTValue *V) {
  return Parameters.remove(V);
}

bool EDTGraphDataEdge::removeGuid(EDT *Guid) { return Guids.remove(Guid); }

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
