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

  /// Context to keep track of events for each EDT operation.
  struct EdtContext {
    /// Map to keep track of datablock operations and their corresponding
    /// events.
    DenseMap<int32_t, int32_t> nodeToEvent;
    /// Each value is a vector holding events. Each event contains a vector
    /// of dependency edges (producer-consumer).
    SmallVector<SmallVector<DatablockGraph::Edge, 4>, 4> events;
  };

  /// Edt Entry
  EdtOp edtEntry;
};
} // end anonymous namespace

void CreateEventsPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG(dbgs() << line << "CreateEventsPass STARTED\n" << line;);

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

    /// Map to keep track of the events for a given EDT Context.
    DenseMap<EdtOp, EdtContext> edtToEvents;

    /// Insert or update an event for a producer datablock node.
    auto insertEvent = [&](int32_t producerID, int32_t consumerID) {
      /// If the producer is an entry node, use the consumer ID.
      int32_t dbID = graph->isEntryNode(producerID) ? consumerID : producerID;
      auto *dbNode = graph->getNode(dbID);
      assert(dbNode && "Datablock node must exist");

      auto &context =
          edtToEvents[dbNode->edtParent ? dbNode->edtParent : edtEntry];
      auto &nodeToEvent = context.nodeToEvent;
      int32_t eventId = -1;

      /// Helper lambda to register the edge and associate nodes.
      auto addEdge = [&](int32_t eId) {
        context.events[eId].push_back({producerID, consumerID});
        nodeToEvent.try_emplace(producerID, eId);
        nodeToEvent.try_emplace(consumerID, eId);
      };

      if (auto it = nodeToEvent.find(dbID); it != nodeToEvent.end()) {
        /// Datablock node already has an event; update it.
        addEdge(it->second);
        return;
      }

      if (dbNode->parent) {
        int32_t parentID = dbNode->parent->id;
        if (auto it = nodeToEvent.find(parentID); it != nodeToEvent.end()) {
          /// Parent is associated with an event; update it.
          addEdge(it->second);
          return;
        }
        /// Create a new event and associate parent.
        eventId = static_cast<int32_t>(context.events.size());
        context.events.emplace_back();
        context.events.back().push_back({producerID, consumerID});
        nodeToEvent[producerID] = eventId;
        nodeToEvent[consumerID] = eventId;
        nodeToEvent[parentID] = eventId;
        return;
      }

      /// If no event exists, create a new event.
      eventId = static_cast<int32_t>(context.events.size());
      context.events.emplace_back();
      context.events.back().push_back({producerID, consumerID});
      nodeToEvent[producerID] = eventId;
      nodeToEvent[consumerID] = eventId;
    };

    /// Analyze the graph edges.
    auto producers = graph->getProducers();

    /// If there are no producers, skip the function.
    if (producers.empty())
      return;

    /// Iterate over each edge of the graph.
    for (auto producerID : producers) {
      for (auto consumerID : graph->getConsumers(producerID))
        insertEvent(producerID, consumerID);
    }

    /// Process the events for each EDT context.
    auto edtCtxItr = 0;
    for (auto &edtCtx : edtToEvents) {
      auto &events = edtCtx.second.events;
      LLVM_DEBUG(DBGS() << "Processing events for function: " << func.getName()
                        << " and EDT context #" << edtCtxItr << " with "
                        << events.size() << " events\n");
      int eventId = 0;
      for (auto &eventEdges : events) {
        LLVM_DEBUG({
          dbgs() << "  Event #" << eventId++ << ":\n";
          for (auto &edge : eventEdges) {
            dbgs() << "    Producer #" << edge.first << ", Consumer #"
                   << edge.second << "\n";
          }
        });
        processEvent(builder, eventEdges, graph);
      }

      LLVM_DEBUG(dbgs() << "Finished processing events for function: "
                        << func.getName() << " and EDT context #" << edtCtxItr
                        << "\n");
      ++edtCtxItr;
    };

    /// Rewire the datablock operations to use the new events.
    for (auto &pair : rewireMap) {
      auto dbOp = pair.first;
      auto newDbOp = pair.second;
      assert(dbOp && newDbOp && "Invalid datablock operation");
      dbOp.replaceAllUsesWith(newDbOp.getOperation());
      dbOp.erase();
    }

    LLVM_DEBUG(dbgs() << line << "CreateEventsPass FINISHED\n" << line);
  });
}

void CreateEventsPass::insertEventToDb(OpBuilder &builder, DataBlockOp dbOp,
                                       Value newEvent, bool isOut) {
  auto origDbOp = dbOp;
  bool alreadyExists = false;
  if (auto it = rewireMap.find(dbOp); it != rewireMap.end()) {
    dbOp = it->second;
    alreadyExists = true;
  }

  /// Avoid overwriting an existing event.
  if ((isOut && dbOp.getOutEvent()) || (!isOut && dbOp.getInEvent()))
    return;

  auto loc = dbOp.getLoc();
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(dbOp);

  /// Set the event operands.
  Value inEvent = isOut ? dbOp.getInEvent() : newEvent;
  Value outEvent = isOut ? newEvent : dbOp.getOutEvent();

  auto newDbOp = builder.create<arts::DataBlockOp>(
      loc, dbOp.getType(), dbOp.getModeAttr(), dbOp.getPtr(),
      dbOp.getElementType(), dbOp.getElementTypeSize(), dbOp.getIndices(),
      dbOp.getSizes(), inEvent, outEvent, dbOp.getAffineMapAttr());

  /// Copy all attributes except "operandSegmentSizes".
  for (auto attr : dbOp->getAttrs()) {
    if (attr.getName() != "operandSegmentSizes")
      newDbOp->setAttr(attr.getName(), attr.getValue());
  }

  rewireMap[origDbOp] = newDbOp;
  if (alreadyExists)
    dbOp.erase();
}

void CreateEventsPass::processEvent(
    OpBuilder &builder, SmallVector<DatablockGraph::Edge, 4> &eventEdges,
    DatablockGraph *graph) {
  OpBuilder::InsertionGuard IG(builder);

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

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateEventsPass() {
  return std::make_unique<CreateEventsPass>();
}
} // namespace arts
} // namespace mlir
