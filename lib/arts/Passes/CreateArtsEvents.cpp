
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
#include "mlir/Dialect/Func/IR/FuncOps.h"
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
                         SmallVector<Value> newEvents) {
    OpBuilder::InsertionGuard b(builder);
    /// Gather original operand groups
    SmallVector<Value> parameters(edtOp.getParameters().begin(),
                                  edtOp.getParameters().end());
    SmallVector<Value> constants(edtOp.getConstants().begin(),
                                 edtOp.getConstants().end());
    SmallVector<Value> dependencies(edtOp.getDependencies().begin(),
                                    edtOp.getDependencies().end());
    SmallVector<Value> events(edtOp.getEvents().begin(),
                              edtOp.getEvents().end());
    events.append(newEvents.begin(), newEvents.end());
    auto attrs = edtOp->getAttrs();

    /// Build a new EdtOp. We set the insertion point right where the old EDT
    builder.setInsertionPoint(edtOp);
    auto loc = edtOp.getLoc();

    /// Create a new EdtOp
    auto newEdt = builder.create<arts::EdtOp>(loc, parameters, constants,
                                              dependencies, events);
    // Update operand segment sizes attribute to account for the new number of
    // operands
    newEdt->setAttr(
        "operand_segment_sizes",
        builder.getI32VectorAttr({static_cast<int32_t>(parameters.size()),
                                  static_cast<int32_t>(constants.size()),
                                  static_cast<int32_t>(dependencies.size()),
                                  static_cast<int32_t>(events.size())}));
    // newEdt->setAttrs(attrs);
    newEdt.getBody().takeBody(edtOp.getBody());

    /// Remove the old EdtOp
    edtOp.erase();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    /// Retrieve the shared analysis result.
    /// (This analysis must have been registered and computed earlier.)
    auto &dbAnalysis = getAnalysis<DatablockAnalysis>();

    /// Iterate over every function in the module.
    module->walk([&](func::FuncOp func) {
      /// Retrieve (or compute) the dependency graph for this function.
      auto &graph = dbAnalysis.getOrCreateGraph(func);
      dbAnalysis.printGraph(func);

      /// Create events based on the datablock graph.
      SmallVector<Event, 4> events;
      DenseMap<DatablockAnalysis::Node *, int32_t> nodeToEvent;

      /// Pragma function to insert an event for a producer node.
      /// Cache the event id for the producer node.
      auto insertEvent = [&](DatablockAnalysis::Node &producer,
                             DatablockAnalysis::Edge &edge,
                             int32_t eventId = -1, bool isGrouped = false) {
        if (eventId == -1) {
          events.emplace_back();
          eventId = int32_t(events.size() - 1);
        } else {
          assert(eventId < int32_t(events.size()));
        }
        auto &event = events[eventId];
        event.isGrouped |= isGrouped;
        event.edges.push_back(edge);
        nodeToEvent[&producer] = eventId;
      };

      /// Analyze the graph edges
      for (auto &edge : graph.edges) {
        auto &producer = graph.nodes[edge.producerID];
        /// If the event for this producer has already been created, use it.
        auto found = nodeToEvent.find(&producer);
        if (found != nodeToEvent.end()) {
          insertEvent(producer, edge, found->second);
          continue;
        }

        /// If producer is loop dependent, and loads data from a parent
        /// datablock, we group the events.
        if (producer.isLoopDependent && producer.isLoad) {
          int32_t eventId = -1;
          for (auto alias : producer.aliases) {
            auto &aliasNode = graph.nodes[alias];
            auto aliasFound = nodeToEvent.find(&aliasNode);
            if (aliasFound != nodeToEvent.end()) {
              eventId = aliasFound->second;
              insertEvent(producer, edge, eventId);
              break;
            }
          }
          /// Insert new grouped event
          insertEvent(producer, edge, -1, true);
          continue;
        }

        /// Create a new event for this producer.
        /// No need to group events for now.
        insertEvent(producer, edge, -1, false);
      }

      /// Lambda to load an event value into the consumer's event map.
      DenseMap<EdtOp, SmallVector<Value>> edtToEvents;
      OpBuilder builder(func);
      auto loadEventToMap = [&](auto &consumer, Value event, Location loc) {
        builder.setInsertionPoint(consumer.edtUser);
        auto eventLoad =
            builder.create<memref::LoadOp>(loc, event, consumer.offsets);
        edtToEvents[consumer.edtUser].push_back(eventLoad.getResult());
        return eventLoad.getResult();
      };

      /// Load events into the EDTs.
      for (auto e : events) {
        auto edges = e.edges;

        if (e.isGrouped) {
          /// Set insertion point to the beginning of the producer node's parent
          //. region.
          auto &producerNode = graph.nodes[e.edges[0].producerID];
          auto loc = producerNode.op.getLoc();
          builder.setInsertionPointToStart(
              &producerNode.edtParent->getRegion(0).front());

          /// Get the parent node.
          assert(producerNode.isLoad && "Expected a load datablock");
          auto dbParent =
              cast<DataBlockOp>(producerNode.baseMemref.getDefiningOp());
          auto &dbParentNode = dbAnalysis.getNode(dbParent);

          /// Create event with computed numElements.
          Value numElements = builder.create<arith::ConstantIndexOp>(loc, 1);
          for (auto sz : dbParentNode.sizes)
            numElements = builder.create<arith::MulIOp>(loc, numElements, sz);
          auto type = MemRefType::get(dbParentNode.op.getType().getShape(),
                                      builder.getIntegerType(64));
          auto event =
              builder.create<arts::EventOp>(loc, type, numElements, true);

          /// Insert event load for the producer.
          auto defEvent = loadEventToMap(producerNode, event.getResult(), loc);

          /// Insert event load for each consumer in the edge list.
          for (auto &edge : edges) {
            auto &consumer = graph.nodes[edge.consumerID];
            if (edge.isDirect)
              edtToEvents[consumer.edtUser].push_back(defEvent);
            else
              loadEventToMap(consumer, event.getResult(), loc);
          }
          continue;
        }

        /// For non-grouped events, expect a single edge.
        assert(edges.size() == 1 && "Expected a single edge");
        auto &producerNode = graph.nodes[e.edges[0].producerID];
        auto loc = producerNode.op.getLoc();
        builder.setInsertionPoint(producerNode.op);

        /// Create event.
        Value numElements = builder.create<arith::ConstantIndexOp>(loc, 1);
        auto type = producerNode.op.getResult().getType();
        auto eventOp =
            builder.create<arts::EventOp>(loc, type, numElements, false);

        /// Insert event load for the producer.
        loadEventToMap(producerNode, eventOp.getResult(), loc);
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