///==========================================================================
/// File: CreateEvents.cpp
///==========================================================================

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/DataBlockAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

#define DEBUG_TYPE "create-events"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;
//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct CreateEventsPass : public arts::CreateEventsBase<CreateEventsPass> {

  void runOnOperation() override;

  /// Inserts a new event operand into a DB
  void insertEventToDb(OpBuilder &builder, DataBlockOp dbOp, Value newEvent,
                       bool isOut);
  /// Handles processing of grouped events.
  void processEvent(OpBuilder &builder,
                    SmallVector<DatablockGraph::Edge, 4> &eventEdges,
                    DatablockGraph *graph);

private:
  /// Retrieve the shared analysis result.
  DatablockAnalysis *dbAnalysis;
  /// Rewire map
  DenseMap<arts::DataBlockOp, arts::DataBlockOp> rewireMap;
};
} // end anonymous namespace

void CreateEventsPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG({ dbgs() << line << "CreateEventsPass STARTED\n" << line; });

  dbAnalysis = &getAnalysis<DatablockAnalysis>();

  /// Iterate over every function in the module.
  module->walk([&](func::FuncOp func) {
    /// Retrieve (or compute) the dependency graph for this function.
    auto graph = dbAnalysis->getOrCreateGraph(func);
    if (!graph || !graph->hasNodes())
      return;

    /// Create a new builder for the function.
    OpBuilder builder(func);
    builder.setInsertionPointToStart(&func.getBody().front());

    /// Create events based on the datablock graph.
    SmallVector<DatablockGraph::Edge, 4> eventEdgeBuffer;
    DenseMap<int32_t, int32_t> nodeToEvent;
    SmallVector<SmallVector<DatablockGraph::Edge, 4>, 4> events;
    events.reserve(graph->getProducers().size());

    /// Lambda to insert or update an event for a producer datablock node.
    auto insertEvent = [&](int32_t producerID, int32_t consumerID) {
      int32_t eventId = -1;
      if (graph->isEntryNode(producerID)) {
        eventId = static_cast<int32_t>(events.size());
        events.emplace_back(); /// Create new empty event
        events.back().push_back({producerID, consumerID});
        nodeToEvent.insert({consumerID, eventId});
        return;
      }

      auto it = nodeToEvent.find(producerID);
      if (it != nodeToEvent.end()) {
        eventId = it->second;
      } else {
        const auto &producer = *graph->getNode(producerID);
        if (producer.hasPtrDb) {
          auto parentIt = nodeToEvent.find(producer.parent->id);
          if (parentIt != nodeToEvent.end()) {
            eventId = parentIt->second;
          }
        }
        if (eventId == -1) {
          eventId = static_cast<int32_t>(events.size());
          events.emplace_back();
        }
      }
      events[eventId].push_back({producerID, consumerID});
      nodeToEvent[producerID] = eventId;
      if (graph->getNode(producerID)->hasPtrDb)
        nodeToEvent[graph->getNode(producerID)->parent->id] = eventId;
    };

    /// Analyze the graph edges.
    auto producers = graph->getProducers();

    /// If there are no producers, skip the function.
    if (producers.empty())
      return;

    /// Iterate over each edge of the graph.
    for (const auto &producerID : producers) {
      auto consumers = graph->getConsumers(producerID);
      for (const auto &consumerID : consumers)
        insertEvent(producerID, consumerID);
    }

    LLVM_DEBUG(DBGS() << "Processing events for function: " << func.getName()
                      << " with " << events.size() << " events\n";);
    int eventId = 0;
    for (auto &eventEdges : events) {
      LLVM_DEBUG({
        dbgs() << "  Event #" << eventId++ << ":\n";
        for (const auto &edge : eventEdges) {
          dbgs() << "    Producer #" << edge.first << ", Consumer #"
                 << edge.second << "\n";
        }
      });
      processEvent(builder, eventEdges, graph);
    }

    LLVM_DEBUG(dbgs() << "Finished processing events for function: "
                      << func.getName() << "\n");
  });

  /// Rewire the datablock operations to use the new events.
  for (auto &pair : rewireMap) {
    auto dbOp = pair.first;
    auto newDbOp = pair.second;
    assert(dbOp && newDbOp && "Invalid datablock operation");
    dbOp.replaceAllUsesWith(newDbOp.getOperation());
    dbOp.erase();
  }

  LLVM_DEBUG(DBGS() << line << "CreateEventsPass FINISHED\n" << line;);
}
void CreateEventsPass::insertEventToDb(OpBuilder &builder, DataBlockOp dbOp,
                                       Value newEvent, bool isOut) {
  auto origDbOp = dbOp;
  bool alreadyExists = false;
  /// If the dbOp is already in the rewireMap, use the newDbOp
  if (auto it = rewireMap.find(dbOp); it != rewireMap.end()) {
    dbOp = it->second;
    alreadyExists = true;
  }

  /// Set input or output event accordingly.
  Value inEvent = isOut ? dbOp.getInEvent() : newEvent;
  Value outEvent = isOut ? newEvent : dbOp.getOutEvent();
  if (!isOut)
    assert(!dbOp.getInEvent() && "DataBlock already has an Input event");
  else
    assert(!dbOp.getOutEvent() && "DataBlock already has an Output event");

  /// Set insertion point to the dbOp.
  auto loc = dbOp.getLoc();
  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(dbOp);

  /// Create a new DataBlockOp with the updated event.
  auto newDbOp = builder.create<arts::DataBlockOp>(
      loc, dbOp.getType(), dbOp.getModeAttr(), dbOp.getPtr(),
      dbOp.getElementType(), dbOp.getElementTypeSize(), dbOp.getIndices(),
      dbOp.getSizes(), inEvent, outEvent, dbOp.getAffineMapAttr());

  /// Copy remaining attributes while skipping those that are overwritten.
  for (auto attr : dbOp->getAttrs()) {
    if (attr.getName() == "operandSegmentSizes")
      continue;
    newDbOp->setAttr(attr.getName(), attr.getValue());
  }

  /// Update the datablock node in the rewire map.
  rewireMap[origDbOp] = newDbOp;
  if (alreadyExists) {
    /// Erase the old dbOp since a new one already exists.
    dbOp.erase();
  }
}

void CreateEventsPass::processEvent(
    OpBuilder &builder, SmallVector<DatablockGraph::Edge, 4> &eventEdges,
    DatablockGraph *graph) {
  OpBuilder::InsertionGuard IG(builder);

  LLVM_DEBUG(dbgs() << "Processing event: \n");
  for (const auto &edge : eventEdges) {
    LLVM_DEBUG(dbgs() << "    Producer #" << edge.first << ", Consumer #"
                      << edge.second << "\n");
  }

  /// Use the first consumer to determine event type and size.
  LLVM_DEBUG(dbgs() << "    Using consumer #" << eventEdges.front().second
                    << "\n";);
  auto *dbOp = graph->getNode(eventEdges.front().second);
  assert(dbOp && "Event must have a valid datablock node");
  /// Set insertion point to the beginning of the EDT parent's region if
  /// available,
  if (dbOp->edtParent) {
    builder.setInsertionPointToStart(&dbOp->edtParent->getRegion(0).front());
  }
  /// Otherwise, set the insertion point to the dbOp.
  else {
    builder.setInsertionPoint(dbOp->op);
  }

  if (dbOp->parent) {
    assert(dbOp->parent && "Grouped event must have a parent datablock");
    LLVM_DEBUG(dbgs() << "     Parent datablock: " << dbOp->parent->id
                      << "\n";);
    dbOp = graph->getNode(dbOp->parent->op);
  }

  auto eventType = MemRefType::get(dbOp->op.getType().getShape(),
                                   builder.getIntegerType(64));
  auto loc = dbOp->op.getLoc();
  auto eventOp =
      builder.create<arts::EventOp>(loc, eventType, dbOp->op.getSizes());

  /// Process each edge.
  for (const auto &edge : eventEdges) {
    if (!graph->isEntryNode(edge.first)) {
      const auto *producerNode = graph->getNode(edge.first);
      insertEventToDb(builder, producerNode->op, eventOp.getResult(), true);
    }
    const auto *consumerNode = graph->getNode(edge.second);
    insertEventToDb(builder, consumerNode->op, eventOp.getResult(), false);
  }
}

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateEventsPass() {
  return std::make_unique<CreateEventsPass>();
}
} // namespace arts
} // namespace mlir
