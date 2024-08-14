
#include "llvm/Support/Debug.h"
#include <cassert>

#include "carts/analysis/graph/ARTSEdge.h"
#include "carts/analysis/graph/ARTSGraph.h"
#include "carts/utils/ARTS.h"

using namespace llvm;
using namespace arts;

/// DEBUG
#define DEBUG_TYPE "arts-graph"
// #if !defined(NDEBUG)
// static constexpr auto TAG = "[" DEBUG_TYPE "] ";
// #endif

/// CreationGraphEdge
CreationGraphEdge::CreationGraphEdge(EDTGraphNode *From, EDTGraphNode *To)
    : From(From), To(To) {}
CreationGraphEdge::~CreationGraphEdge() {}
EDTGraphNode *CreationGraphEdge::getFrom() { return From; }
EDTGraphNode *CreationGraphEdge::getTo() { return To; }

bool CreationGraphEdge::insertParameter(EDTValue *V) {
  return Parameters.insert(V);
}

bool CreationGraphEdge::insertGuid(EDT *Guid) { return Guids.insert(Guid); }

bool CreationGraphEdge::hasParameter(EDTValue *V) {
  return Parameters.count(V);
}
bool CreationGraphEdge::hasGuid(EDT *Guid) { return Guids.count(Guid); }

bool CreationGraphEdge::removeParameter(EDTValue *V) {
  return Parameters.remove(V);
}

bool CreationGraphEdge::removeGuid(EDT *Guid) { return Guids.remove(Guid); }

uint32_t CreationGraphEdge::getParametersSize() { return Parameters.size(); }
uint32_t CreationGraphEdge::getGuidsSize() { return Guids.size(); }

void CreationGraphEdge::forEachParameter(std::function<void(EDTValue *)> F) {
  for (Value *V : Parameters)
    F(V);
}

void CreationGraphEdge::forEachGuid(std::function<void(EDT *)> F) {
  for (EDT *Guid : Guids)
    F(Guid);
}

/// DataBlockGraphEdge
DataBlockGraphEdge::DataBlockGraphEdge(EDTGraphNode *From, EDTGraphSlotNode *To,
                                       DataBlock *DB)
    : From(From), To(To), DB(DB) {}

DataBlockGraphEdge::~DataBlockGraphEdge() {}
EDTGraphNode *DataBlockGraphEdge::getFrom() { return From; }
EDTGraphSlotNode *DataBlockGraphEdge::getTo() { return To; }
DataBlock *DataBlockGraphEdge::getDataBlock() { return DB; }
