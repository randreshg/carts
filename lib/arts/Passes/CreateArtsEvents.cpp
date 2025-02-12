#include "arts/Analysis/DataBlockAnalysis.h"

/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/Support/LLVM.h"
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

namespace {

struct CreateArtsEventsPass
    : public arts::CreateArtsEventsBase<CreateArtsEventsPass> {

  /// Event data structure
  struct Event {
    bool isGrouped = false;
    SmallVector<DatablockAnalysis::Edge> edges;
  };

  /// Inserts a new event operand
  void insertEventsToEdt(OpBuilder &builder, EdtOp edtOp,
                         ArrayRef<Value> newEvents) {
    // Save original dependencies and events.
    SmallVector<Value, 4> dependencies(edtOp.getDependencies());
    SmallVector<Value, 4> events(edtOp.getEvents());
    events.append(newEvents.begin(), newEvents.end());

    /// Build a new EdtOp. We set the insertion point right where the old EDT
    builder.setInsertionPoint(edtOp);
    auto loc = edtOp.getLoc();
    auto newEdt = builder.create<arts::EdtOp>(loc, dependencies, events);

    /// Update operand segment sizes attribute to account for the new number of
    /// operands
    newEdt->setAttr(
        "operandSegmentSizes",
        builder.getI32VectorAttr({static_cast<int32_t>(dependencies.size()),
                                  static_cast<int32_t>(events.size())}));
    /// Copy the rest of the attributes
    for (auto attr : edtOp->getAttrs()) {
      if (attr.getName().str() == "operandSegmentSizes")
        continue;
      newEdt->setAttr(attr.getName(), attr.getValue());
    }

    /// Move the region's operations.
    newEdt.getBody().takeBody(edtOp.getBody());

    /// Remove the old EdtOp
    edtOp.erase();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    /// Retrieve the shared analysis result.
    auto &dbAnalysis = getAnalysis<DatablockAnalysis>();

    /// Iterate over every function in the module.
    module->walk([&](func::FuncOp func) {
      /// Retrieve (or compute) the dependency graph for this function.
      auto &graph = dbAnalysis.getOrCreateGraph(func);
      if (graph.edges.empty())
        return;
      dbAnalysis.printGraph(func);

      /// Create events based on the datablock graph.
      SetVector<EdtOp> edtUsers;
      SmallVector<Event, 4> events;
      DenseMap<DatablockAnalysis::Node *, int32_t> nodeToEvent;
      events.reserve(graph.edges.size());

      /// Pragma function to insert an event for a producer datablock node.
      auto insertEvent = [&](DatablockAnalysis::Node &producer,
                             DatablockAnalysis::Edge &edge,
                             int32_t eventId = -1, bool isGrouped = false) {
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

        /// Insert Edt user of consumer and producer.
        assert(producer.edtUser && "Expected an EDT user");
        edtUsers.insert(producer.edtUser);
        edtUsers.insert(graph.nodes[edge.consumerID].edtUser);

        /// If the datablock is loaded and dependent on a loop, group the event.
        if (producer.isLoopDependent && producer.isLoad) {
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

      /// Preallocate the events size with the number of dependencies and
      /// initialize with a none event.
      DenseMap<EdtOp, SmallVector<Value>> edtToEvents;
      for (auto edt : edtUsers) {
        edtToEvents[edt] =
            SmallVector<Value>(edt.getDependencies().size(), noneEvent);
      }

      /// Lambda to load an event value into the consumer's event map.
      auto loadEventToMap = [&](DatablockAnalysis::Node &dbNode, Value event,
                                Location loc) -> Value {
        builder.setInsertionPoint(dbNode.edtUser);
        auto eventLoad =
            builder.create<memref::LoadOp>(loc, event, dbNode.offsets);
        edtToEvents[dbNode.edtUser][dbNode.edtDepId] = eventLoad.getResult();
        return eventLoad.getResult();
      };

      /// Load events into the EDTs.
      for (const auto &e : events) {
        const auto &edges = e.edges;
        if (e.isGrouped) {
          /// Set insertion point to the beginning of the producer node's parent
          /// region. - Note: All producers are the same.
          auto &producerNode = graph.nodes[edges.front().producerID];
          auto loc = producerNode.op.getLoc();
          builder.setInsertionPointToStart(
              &producerNode.edtParent->getRegion(0).front());
          /// Get the parent node.
          assert(producerNode.isLoad && "Expected a load datablock");
          auto dbParent =
              cast<DataBlockOp>(producerNode.baseMemref.getDefiningOp());
          auto &dbParentNode = dbAnalysis.getNode(dbParent);
          /// Create event
          auto type = MemRefType::get(dbParentNode.op.getType().getShape(),
                                      builder.getIntegerType(64));
          auto eventOp = builder.create<arts::EventOp>(
              loc, type, dbParentNode.op.getSizes());
          /// Insert event load for the producer.
          auto defEvent =
              loadEventToMap(producerNode, eventOp.getResult(), loc);
          /// Insert event load for each consumer in the edge list.
          for (const auto &edge : edges) {
            auto &consumer = graph.nodes[edge.consumerID];
            if (edge.isDirect)
              edtToEvents[consumer.edtUser][consumer.edtDepId] = defEvent;
            else
              loadEventToMap(consumer, eventOp.getResult(), loc);
          }
        } else {
          /// For non-grouped events, expect a single edge.
          assert(edges.size() == 1 && "Expected a single edge");
          auto &producerNode = graph.nodes[edges.front().producerID];
          auto loc = producerNode.op.getLoc();
          builder.setInsertionPoint(producerNode.op);
          /// Create event.
          // Value numElements = builder.create<arith::ConstantIndexOp>(loc, 1);
          auto type = producerNode.op.getResult().getType();
          auto eventOp = builder.create<arts::EventOp>(
              loc, type, producerNode.op.getSizes());
          /// Insert event load for the producer.
          loadEventToMap(producerNode, eventOp.getResult(), loc);
        }
      }

      /// Insert events into the EDTs.
      for (auto &pair : edtToEvents)
        insertEventsToEdt(builder, pair.first, pair.second);
    });
  }
};

} // end anonymous namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateEventsPass() {
  return std::make_unique<CreateArtsEventsPass>();
}
} // namespace arts
} // namespace mlir
