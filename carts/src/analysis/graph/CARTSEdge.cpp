
#include "llvm/Support/Debug.h"
#include <cassert>

#include "carts/analysis/graph/CARTSEdge.h"
#include "carts/analysis/graph/CARTSGraph.h"
#include "carts/utils/ARTS.h"

using namespace llvm;
using namespace arts;

/// DEBUG
#define DEBUG_TYPE "edt-graph"
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
SetVector<EDTValue *> &CreationGraphEdge::getParameters() { return Parameters; }
SetVector<EDT *> &CreationGraphEdge::getGuids() { return Guids; }

/// DataBlockGraphEdge
DataBlockGraphEdge::DataBlockGraphEdge(EDTGraphNode *From, EDTGraphSlotNode *To,
                                       DataBlock *DB)
    : From(From), To(To), DB(DB) {}

DataBlockGraphEdge::~DataBlockGraphEdge() {}
EDTGraphNode *DataBlockGraphEdge::getFrom() { return From; }
EDTGraphSlotNode *DataBlockGraphEdge::getTo() { return To; }
DataBlock *DataBlockGraphEdge::getDataBlock() { return DB; }
