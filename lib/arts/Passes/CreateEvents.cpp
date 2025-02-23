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
#include "arts/Passes/ArtsPasses.h"
#include "arts/ArtsDialect.h"
#include "arts/Analysis/DataBlockAnalysis.h"
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

  /// Event data structure
  struct Event {
    bool isGrouped = false;
    SmallVector<DatablockAnalysis::Edge> edges;
  };

  void runOnOperation() override;

  /// Inserts a new event operand into an EDT.
  void insertEventsToEdt(OpBuilder &builder, EdtOp edtOp,
                         ArrayRef<Value> newEvents);
  /// Helper to load an event value into the consumer's event map.
  Value loadEventToMap(OpBuilder &builder, DatablockAnalysis::Node &dbNode,
                       Value event, Location loc);
  /// Handles processing of grouped events.
  void processGroupedEvent(OpBuilder &builder, Event &event,
                           DatablockAnalysis::Graph &graph);
  /// Handles processing of non-grouped events.
  void processNonGroupedEvent(OpBuilder &builder, Event &event,
                              DatablockAnalysis::Graph &graph);

private:
  /// Retrieve the shared analysis result.
  DatablockAnalysis *dbAnalysis;
  /// Map to store the EDT events.
  DenseMap<EdtOp, SmallVector<Value>> edtToEvents;
};
} // end anonymous namespace

void CreateEventsPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << line << "CreateEventsPass STARTED\n" << line);
  ModuleOp module = getOperation();
  dbAnalysis = &getAnalysis<DatablockAnalysis>();

  /// Iterate over every function in the module.
  module->walk([&](func::FuncOp func) {
    /// Retrieve (or compute) the dependency graph for this function.
    auto &graph = dbAnalysis->getOrCreateGraph(func);
    if (graph.edges.empty())
      return;
    dbAnalysis->printGraph(func);

    /// Create events based on the datablock graph.
    SetVector<EdtOp> edtUsers;
    SmallVector<Event, 4> events;
    DenseMap<DatablockAnalysis::Node *, int32_t> nodeToEvent;
    events.reserve(graph.edges.size());

    /// Lambda to insert or update an event for a producer datablock node.
    auto insertEvent = [&](DatablockAnalysis::Node &producer,
                           DatablockAnalysis::Edge &edge, int32_t eventId = -1,
                           bool isGrouped = false) {
      if (eventId == -1) {
        events.push_back(Event());
        eventId = static_cast<int32_t>(events.size() - 1);
      }
      auto &event = events[eventId];
      event.isGrouped |= isGrouped;
      event.edges.push_back(edge);
      nodeToEvent[&producer] = eventId;
    };

    /// Analyze the graph edges.
    for (auto &edge : graph.edges) {
      auto &producer = graph.nodes[edge.producerID];
      if (nodeToEvent.count(&producer)) {
        insertEvent(producer, edge, nodeToEvent[&producer]);
        continue;
      }
      /// Insert EDT users for consumer and producer.
      assert(producer.userEdt && "Expected an EDT user");
      edtUsers.insert(producer.userEdt);
      edtUsers.insert(graph.nodes[edge.consumerID].userEdt);

      /// If the datablock is loop dependent, and points to a db, group the
      /// event.
      if (producer.isLoopDependent && producer.ptrIsDb) {
        int32_t eventId = -1;
        for (auto alias : producer.aliases) {
          auto &aliasNode = graph.nodes[alias];
          if (nodeToEvent.count(&aliasNode)) {
            eventId = nodeToEvent[&aliasNode];
            insertEvent(producer, edge, eventId);
            break;
          }
        }
        /// Insert new grouped event.
        insertEvent(producer, edge, -1, true);
        continue;
      }
      /// Create a new event for this producer.
      insertEvent(producer, edge, -1, false);
    }

    OpBuilder builder(func);

    builder.setInsertionPointToStart(&func.getBody().front());
    auto noneEvent =
        builder.create<arith::ConstantIntOp>(func.getLoc(), -1, 32);

    /// Preallocate the events for each EDT.
    DenseMap<EdtOp, SmallVector<Value>> edtToEvents;
    for (auto edt : edtUsers) {
      edtToEvents[edt] =
          SmallVector<Value>(edt.getDependencies().size(), noneEvent);
    }

    /// Process each event.
    for (auto &event : events) {
      if (event.isGrouped)
        processGroupedEvent(builder, event, graph);
      else
        processNonGroupedEvent(builder, event, graph);
    }

    /// Insert events into the EDTs.
    for (auto &pair : edtToEvents)
      insertEventsToEdt(builder, pair.first, pair.second);
  });

  LLVM_DEBUG(dbgs() << line << "CreateEventsPass FINISHED\n" << line);
}

void CreateEventsPass::insertEventsToEdt(OpBuilder &builder, EdtOp edtOp,
                                         ArrayRef<Value> newEvents) {
  /// Save original dependencies and events.
  SmallVector<Value, 4> dependencies(edtOp.getDependencies());
  SmallVector<Value, 4> events(edtOp.getEvents());
  events.append(newEvents.begin(), newEvents.end());

  /// Build a new EdtOp. Set the insertion point at the old EDT.
  builder.setInsertionPoint(edtOp);
  auto loc = edtOp.getLoc();
  auto newEdt = builder.create<arts::EdtOp>(loc, dependencies, events);

  /// Update the operandSegmentSizes attribute.
  newEdt->setAttr(
      "operandSegmentSizes",
      builder.getI32VectorAttr({static_cast<int32_t>(dependencies.size()),
                                static_cast<int32_t>(events.size())}));
  /// Copy the rest of the attributes.
  for (auto attr : edtOp->getAttrs()) {
    if (attr.getName().str() == "operandSegmentSizes")
      continue;
    newEdt->setAttr(attr.getName(), attr.getValue());
  }

  /// Move the region's operations.
  newEdt.getBody().takeBody(edtOp.getBody());

  /// Remove the old EdtOp.
  edtOp.erase();
}

Value CreateEventsPass::loadEventToMap(OpBuilder &builder,
                                       DatablockAnalysis::Node &dbNode,
                                       Value event, Location loc) {
  builder.setInsertionPoint(dbNode.userEdt);
  auto eventLoad = builder.create<memref::LoadOp>(loc, event, dbNode.indices);
  edtToEvents[dbNode.userEdt][dbNode.userEdtPos] = eventLoad.getResult();
  return eventLoad.getResult();
}

void CreateEventsPass::processGroupedEvent(OpBuilder &builder, Event &event,
                                           DatablockAnalysis::Graph &graph) {
  const auto &edges = event.edges;
  /// Assume all producers are the same for grouped events.
  auto &producerNode = graph.nodes[edges.front().producerID];
  auto loc = producerNode.op.getLoc();
  builder.setInsertionPointToStart(
      &producerNode.edtParent->getRegion(0).front());
  /// Expect a load datablock.
  assert(producerNode.isLoad && "Expected a load datablock");
  auto dbParent = cast<DataBlockOp>(producerNode.ptr.getDefiningOp());
  auto &dbParentNode = dbAnalysis->getNode(dbParent);
  auto type = MemRefType::get(dbParentNode.op.getType().getShape(),
                              builder.getIntegerType(64));
  auto eventOp =
      builder.create<arts::EventOp>(loc, type, dbParentNode.op.getSizes());
  auto defEvent =
      loadEventToMap(builder, producerNode, eventOp.getResult(), loc);
  /// Process each consumer.
  for (const auto &edge : edges) {
    auto &consumer = graph.nodes[edge.consumerID];
    if (edge.isDirect)
      edtToEvents[consumer.userEdt][consumer.userEdtPos] = defEvent;
    else
      loadEventToMap(builder, consumer, eventOp.getResult(), loc);
  }
}

void CreateEventsPass::processNonGroupedEvent(OpBuilder &builder, Event &event,
                                              DatablockAnalysis::Graph &graph) {
  /// Expect exactly one edge.
  auto &producerNode = graph.nodes[event.edges.front().producerID];
  auto loc = UnknownLoc::get(builder.getContext());
  builder.setInsertionPoint(producerNode.op);
  auto type = producerNode.op.getResult().getType();
  auto eventOp =
      builder.create<arts::EventOp>(loc, type, producerNode.op.getSizes());
  loadEventToMap(builder, producerNode, eventOp.getResult(), loc);
}

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateEventsPass() {
  return std::make_unique<CreateEventsPass>();
}
} // namespace arts
} // namespace mlir
