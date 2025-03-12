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
    SmallVector<DatablockAnalysis::Edge> edges;
  };

  void runOnOperation() override;

  /// Inserts a new event operand into a DB
  void insertEventToDb(OpBuilder &builder, DataBlockOp dbOp, Value newEvent,
                       bool isOut);
  /// Handles processing of grouped events.
  void processEvent(OpBuilder &builder, Event &event,
                    DatablockAnalysis::Graph &graph);
  /// Handles processing of non-grouped events.
  // void processNonGroupedEvent(OpBuilder &builder, Event &event,
  //                             DatablockAnalysis::Graph &graph);

private:
  /// Retrieve the shared analysis result.
  DatablockAnalysis *dbAnalysis;
  /// Map to store the EDT events.
  DenseMap<EdtOp, SmallVector<Value>> edtToEvents;
  /// NoneEvent constant.
  Value noneEvent = nullptr;
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

    /// Create a new builder for the function.
    OpBuilder builder(func);
    builder.setInsertionPointToStart(&func.getBody().front());
    noneEvent = builder.create<arith::ConstantIntOp>(func.getLoc(), -1, 32);

    /// Create events based on the datablock graph.
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
      /// If an event is already associated with the producer node,
      /// reuse it and update the event with the new edge.
      auto &producer = graph.nodes[edge.producerID];
      if (nodeToEvent.count(&producer)) {
        insertEvent(producer, edge, nodeToEvent[&producer]);
        continue;
      }

      /// If an event is already associated with the consumer node,
      /// ignore it, it will be handled by the producer.
      /// TODO: This might be wrong.
      auto &consumer = graph.nodes[edge.consumerID];
      if (nodeToEvent.count(&consumer)) {
        continue;
      }

      /// For loop-dependent datablocks attempt to reuse an existing event
      // if (producer.isLoopDependent && producer.hasPtrDb) {
      //   int32_t eventId = -1;
      //   for (auto alias : producer.aliases) {
      //     auto &aliasNode = graph.nodes[alias];
      //     if (nodeToEvent.count(&aliasNode)) {
      //       eventId = nodeToEvent[&aliasNode];
      //       insertEvent(producer, edge, eventId);
      //       break;
      //     }
      //   }
      //   insertEvent(producer, edge, -1);
      //   continue;
      // }

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

    /// Process each event.
    for (auto &event : events) {
      processEvent(builder, event, graph);
      // if (event.edges.size() > 1)
      //   processGroupedEvent(builder, event, graph);
      // else
      //   processNonGroupedEvent(builder, event, graph);
    }
  });

  LLVM_DEBUG({
    dbgs() << line << "CreateEventsPass FINISHED\n" << line;
    // module.dump();
  });
}

void CreateEventsPass::insertEventToDb(OpBuilder &builder, DataBlockOp dbOp,
                                       Value newEvent, bool isOut) {
  Value inEvent = nullptr;
  Value outEvent = nullptr;
  if (!isOut) {
    assert(!dbOp.getInEvent() && "DataBlock already has an Input event");
    inEvent = newEvent;
  } else {
    assert(!dbOp.getOutEvent() && "DataBlock already has an Output event");
    outEvent = newEvent;
  }

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
    auto name = attr.getName();
    if (name == "operandSegmentSizes")
      continue;
    newDbOp->setAttr(name, attr.getValue());
  }

  /// Replace all uses of the original DataBlockOp with the new one.
  dbOp.replaceAllUsesWith(newDbOp.getOperation());

  /// Erase the original DataBlockOp.
  dbOp.erase();
}

void CreateEventsPass::processEvent(OpBuilder &builder, Event &event,
                                           DatablockAnalysis::Graph &graph) {
  LLVM_DEBUG(DBGS() << "Processing grouped event\n");
  const auto &edges = event.edges;

  /// Assume single producer for grouped events.
  auto &producerNode = graph.nodes[edges.front().producerID];
  auto loc = producerNode.op.getLoc();

  /// Set insertion point to the beginning of the edt parent's region.
  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPointToStart(
      &producerNode.edtParent->getRegion(0).front());

  /// Create an event type based on the parent's operation type and a 64-bit
  /// integer.
  arts::EventOp eventOp = nullptr;
  if (producerNode.hasPtrDb) {
    auto &dbParentOp = producerNode.parent->op;
    auto eventType = MemRefType::get(dbParentOp.getType().getShape(),
                                     builder.getIntegerType(64));
    eventOp =
        builder.create<arts::EventOp>(loc, eventType, dbParentOp.getSizes());
  } else {
    auto eventType = MemRefType::get(producerNode.op.getType().getShape(),
                                     builder.getIntegerType(64));
    eventOp = builder.create<arts::EventOp>(loc, eventType,
                                            producerNode.op.getSizes());
  }

  /// Set event for the producer
  insertEventToDb(builder, producerNode.op, eventOp.getResult(), true);

  /// Process each consumer edge.
  for (const auto &edge : edges) {
    auto &consumer = graph.nodes[edge.consumerID];
    insertEventToDb(builder, consumer.op, eventOp.getResult(), false);
  }
}

// void CreateEventsPass::processNonGroupedEvent(OpBuilder &builder, Event
// &event,
//                                               DatablockAnalysis::Graph
//                                               &graph) {
//   LLVM_DEBUG(DBGS() << "Processing non-grouped event\n");
//   OpBuilder::InsertionGuard IG(builder);
//   auto &producerNode = graph.nodes[event.edges.front().producerID];
//   auto &consumerNode = graph.nodes[event.edges.front().consumerID];
//   auto loc = UnknownLoc::get(builder.getContext());
//   builder.setInsertionPoint(producerNode.op);
//   auto type = MemRefType::get(producerNode.op.getType().getShape(),
//                               builder.getIntegerType(64));
//   auto eventOp =
//       builder.create<arts::EventOp>(loc, type, producerNode.op.getSizes());
//   eventOp.setIsSingle();
//   insertEventToDb(builder, producerNode.op, eventOp.getResult(), true);
//   insertEventToDb(builder, consumerNode.op, eventOp.getResult(), false);
// }

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateEventsPass() {
  return std::make_unique<CreateEventsPass>();
}
} // namespace arts
} // namespace mlir
