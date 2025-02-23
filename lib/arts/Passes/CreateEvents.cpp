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
  ModuleOp module = getOperation();
  LLVM_DEBUG({
    dbgs() << line << "CreateEventsPass STARTED\n" << line;
    // module.dump();
  });

  dbAnalysis = &getAnalysis<DatablockAnalysis>();

  /// Iterate over every function in the module.
  module->walk([&](func::FuncOp func) {
    /// Retrieve (or compute) the dependency graph for this function.
    auto &graph = dbAnalysis->getOrCreateGraph(func);
    if (graph.edges.empty())
      return;

    /// Create events based on the datablock graph.
    SetVector<EdtOp> edtUsers;
    SmallVector<Event, 4> events;
    DenseMap<DatablockAnalysis::Node *, int32_t> nodeToEvent;
    events.reserve(graph.edges.size());

    /// Lambda to insert or update an event for a producer datablock node.
    auto insertEvent = [&](DatablockAnalysis::Node &producer,
                           DatablockAnalysis::Edge &edge,
                           int32_t eventId = -1) {
      if (eventId == -1) {
        events.push_back(Event());
        eventId = static_cast<int32_t>(events.size() - 1);
      }
      auto &event = events[eventId];
      event.edges.push_back(edge);
      nodeToEvent[&producer] = eventId;
    };

    /// Analyze the graph edges.
    /// Iterate over each edge in the dependency graph.
    for (auto &edge : graph.edges) {
      auto &producer = graph.nodes[edge.producerID];

      /// If an event is already associated with the producer node,
      /// reuse it and update the event with the new edge.
      if (nodeToEvent.count(&producer)) {
        insertEvent(producer, edge, nodeToEvent[&producer]);
        continue;
      }

      /// Record the EDT users for both producer and consumer nodes.
      /// It is assumed that each producer has an associated EDT.
      assert(producer.userEdt && "Producer must have an associated EDT.");
      edtUsers.insert(producer.userEdt);
      edtUsers.insert(graph.nodes[edge.consumerID].userEdt);

      /// For loop-dependent datablocks with a DB pointer,
      /// attempt to reuse an existing event from any alias.
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
        insertEvent(producer, edge, -1);
        continue;
      }

      /// For non-loop-dependent datablocks or those without a DB pointer,
      /// create a new event.
      insertEvent(producer, edge, -1);
    }

    /// Debug events
    LLVM_DEBUG({
      dbgs() << "Events:\n";
      for (auto &event : events) {
        dbgs() << "  Event\n";
        for (auto &edge : event.edges) {
          dbgs() << "    Producer: " << edge.producerID
                 << ", Consumer: " << edge.consumerID << "\n";
        }
      }
    });

    OpBuilder builder(func);
    builder.setInsertionPointToStart(&func.getBody().front());
    auto noneEvent =
        builder.create<arith::ConstantIntOp>(func.getLoc(), -1, 32);

    /// Preallocate none events for each EDT.
    for (auto edt : edtUsers) {
      edtToEvents[edt] =
          SmallVector<Value>(edt.getDependencies().size(), noneEvent);
    }

    /// Process each event.
    for (auto &event : events) {
      if (event.edges.size() > 1)
        processGroupedEvent(builder, event, graph);
      else
        processNonGroupedEvent(builder, event, graph);
    }

    /// Insert events into the EDTs.
    for (auto &pair : edtToEvents)
      insertEventsToEdt(builder, pair.first, pair.second);
  });

  LLVM_DEBUG({
    dbgs() << line << "CreateEventsPass FINISHED\n" << line;
    // module.dump();
  });
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
  /// Set the insertion point to the EDT associated with the node.
  builder.setInsertionPoint(dbNode.userEdt);
  auto eventDim = event.getType().cast<MemRefType>().getShape().size();
  auto loadIndices = dbNode.indices;
  if (loadIndices.size() != eventDim) {
    auto zeroIndex = builder.create<arith::ConstantIntOp>(
        loc, 0, builder.getIntegerType(64));
    loadIndices.resize(eventDim, zeroIndex);
  }
  auto eventLoad =
      builder.create<memref::LoadOp>(loc, event, loadIndices).getResult();
  edtToEvents[dbNode.userEdt][dbNode.userEdtPos] = eventLoad;
  return eventLoad;
}

void CreateEventsPass::processGroupedEvent(OpBuilder &builder, Event &event,
                                           DatablockAnalysis::Graph &graph) {
  const auto &edges = event.edges;
  /// Assume all producers are the same for grouped events.
  auto &producerNode = graph.nodes[edges.front().producerID];
  auto loc = producerNode.op.getLoc();
  /// Set insertion point at the beginning of the parent's region.
  builder.setInsertionPointToStart(
      &producerNode.edtParent->getRegion(0).front());
  auto dbParent = cast<DataBlockOp>(producerNode.ptr.getDefiningOp());
  auto &dbParentNode = dbAnalysis->getNode(dbParent);
  LLVM_DEBUG(dbgs() << "Parent dimension: "
                    << dbParentNode.op.getType().getShape().size() << "\n");

  /// Compute the maximum number of consumer indices.
  uint32_t maxConsumerIndices = 0;
  for (const auto &edge : edges)
    maxConsumerIndices = std::max(
        maxConsumerIndices,
        static_cast<uint32_t>(graph.nodes[edge.consumerID].indices.size()));

  /// Prepare the adjusted shape using the parent's dimensions/
  auto parentDim = dbParentNode.op.getType().getShape().size();
  auto originalShape = dbParentNode.op.getType().getShape();
  uint32_t fillCount =
      std::min(maxConsumerIndices, static_cast<uint32_t>(parentDim));
  SmallVector<int64_t> shape(fillCount);
  for (uint32_t i = 0; i < fillCount; ++i)
    shape[i] = originalShape[i];

  /// Collect the sizes for each dimension up to maxConsumerIndices.
  SmallVector<Value> sizes;
  sizes.reserve(maxConsumerIndices);
  for (uint32_t i = 0; i < maxConsumerIndices; ++i)
    sizes.push_back(dbParentNode.op.getSizes()[i]);

  auto eventType = MemRefType::get(shape, builder.getIntegerType(64));
  auto eventOp = builder.create<arts::EventOp>(loc, eventType, sizes);
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
  auto type = MemRefType::get(producerNode.op.getType().getShape(),
                              builder.getIntegerType(64));
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
