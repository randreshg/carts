
#include "arts/Analysis/DataBlockAnalysis.h"

/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "polygeist/Ops.h"
/// Other
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
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
/// This pass uses the shared ArtsDataBlockAnalysis to generate events
/// for datablock nodes that produce data (i.e. with mode "out" or "inout").
/// The event op (arts.event) is created for each such node.
/// In a real implementation, you might attach these events to the dependency
/// information of EDTs so that later scheduling optimizations can be applied.
struct CreateArtsEventsPass
    : public arts::CreateArtsEventsBase<CreateArtsEventsPass> {

  void runOnOperation() override {
    ModuleOp module = getOperation();
    /// Retrieve the shared analysis result.
    /// (This analysis must have been registered and computed earlier.)
    auto &dbAnalysis = getAnalysis<DatablockAnalysis>();

    /// Iterate over every function in the module.
    for (auto func : module.getOps<func::FuncOp>()) {
      auto graph = dbAnalysis.getOrCreateGraph(func);
      dbAnalysis.printGraph(func);

      /// Analyze the graph edges
      for (auto &edge : graph.edges) {
        DatablockAnalysis::Node &producer = graph.nodes[edge.producerID];
        DatablockAnalysis::Node &consumer = graph.nodes[edge.consumerID];
        if (producer.mode == "out" || producer.mode == "inout") {
          llvm::errs() << "Created event for datablock node #" << producer.id
                       << "\n";
        }
      }

      // DominanceInfo domInfo(func);
      // /// Retrieve (or compute) the dependency graph for this function.
      // auto &graph = dbAnalysis.getOrCreateGraph(func);

      // /// Set an insertion point (here, at the end of the first block).
      // OpBuilder builder(func);
      // builder.setInsertionPointToEnd(&func.getBody().front());

      // /// Iterate over each node in the graph.
      // for (auto &node : graph.nodes) {
      //   /// For each datablock that is an "out" dependency (or inout),
      //   /// generate an event op.
      //   if (node.mode == "out" || node.mode == "inout") {
      //     /// For simplicity, we assume the event size is 1.
      //     auto sizeConst =
      //         builder.create<arith::ConstantIndexOp>(func.getLoc(), 1);
      //     /// Create an arts.event op (assume it returns a value of type
      //     /// !arts.event). (The exact builder signature depends on your dialect
      //     /// definition.) auto eventOp =
      //     /// builder.create<arts::EventOp>(func.getLoc(),
      //     /// sizeConst.getResult());
      //     llvm::errs() << "Created event for datablock node #" << node.id
      //                  << "\n";
      //     /// Optionally, you can attach the event to the node (e.g. by storing
      //     /// it in a pass-local map or by setting an attribute on the datablock
      //     /// op).
      //   }
      // }
    }
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