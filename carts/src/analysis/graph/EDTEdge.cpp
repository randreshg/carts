
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

bool EDTGraphDataEdge::addEDTValue(EDTValue *V) { return Values.insert(V); }

bool EDTGraphDataEdge::addEDTGuid(EDT *Guid) { return EDTGuids.insert(Guid); }

bool EDTGraphDataEdge::removeDataBlock(EDTDataBlock *DB) {
  return DataBlocks.remove(DB);
}

bool EDTGraphDataEdge::removeEDTValue(EDTValue *V) { return Values.remove(V); }

bool EDTGraphDataEdge::removeEDTGuid(EDT *EDTGuid) {
  return EDTGuids.remove(EDTGuid);
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
