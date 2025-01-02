//===- ConvertOpenMPToARTSHierarchical.cpp - Hierarchical OMP->ARTS -------===//
//
// This file implements a module pass that converts OpenMP ops
// (omp.parallel, omp.master, omp.task, etc.) into ARTS ops
// (arts.parallel, arts.single, arts.edt), storing them in a hierarchical
// data structure (OmpNode) to handle nesting. It also performs ephemeral
// memref usage analysis to build "in/out/inout" dependencies for tasks,
// and captures parameters that are used above each task region.
//
//===----------------------------------------------------------------------===//

#include "PassDetails.h"

// #include "mlir/Dialect/Affine/IR/AffineOps.h"
// #include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/RegionUtils.h"

#include "carts/Dialect.h"
#include "carts/Ops.h"
#include "carts/Passes/Passes.h"

// #include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "convert-openmp-to-arts"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace std;
using namespace mlir;
using namespace arts;


/// Represents an OMP region (parallel, master, or task), plus children.
struct OmpNode {
  /// Attributes
  enum class Kind { Parallel, Master, Task };
  Kind kind;
  Operation *op;
  OmpNode *parent = nullptr;
  SmallVector<OmpNode *> children;
  int taskIndex = -1;

  /// Constructors
  OmpNode(Kind K, Operation *op, OmpNode *parent)
      : kind(K), op(op), parent(parent) {}

  /// Destructor
  ~OmpNode() {
    for (auto &childPtr : children) {
      if (!childPtr)
        continue;
      delete childPtr;
    }
  }
};

namespace {
struct ConvertOpenMPToARTSPass
    : public mlir::arts::ConvertOpenMPToARTSBase<ConvertOpenMPToARTSPass> {

  ConvertOpenMPToARTSPass() = default;
  ~ConvertOpenMPToARTSPass() override {
    for (auto &nodePtr : ompHierarchy)
      delete nodePtr;
  }

  void runOnOperation() override;

private:
  void runOnFunction(func::FuncOp func);

  /// OpenMP hierarchy
  void buildOmpHierarchy(Operation *op, OmpNode *parent,
                         SmallVector<OmpNode *> &topLevel);
  void printOmpHierarchy(OmpNode &node, int indent = 0);

  /// Dependency buffers
  void getDepBuffers();
  void analyzeDepBuffers();

  /// Transformations
  void transformFunction(func::FuncOp func);

  /// Rewriters
  void rewriteParallel(omp::ParallelOp parOp);
  void rewriteMaster(omp::MasterOp mastOp);
  void rewriteTask(omp::TaskOp taskOp);

  /// Helpers
  ArrayAttr buildDependencyArray(OpBuilder &builder, omp::TaskOp task);
  SmallVector<Value> buildParameterArray(OpBuilder &builder, omp::TaskOp task);

  /// Attributes
  SmallVector<OmpNode *> ompHierarchy;
  /// Maps an omp region to the OmpNode
  DenseMap<omp::TaskOp, OmpNode *> taskMap;
  DenseMap<omp::ParallelOp, OmpNode *> parallelMap;
  DenseMap<omp::MasterOp, OmpNode *> masterMap;
  /// Maps a dependency buffer to the set of tasks that use it
  DenseMap<Value, SmallVector<omp::TaskOp>> depBufferMap;
};
} // namespace

void ConvertOpenMPToARTSPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << line << "ConvertOpenMPToARTSPass STARTED\n" << line);
  MLIRContext *context = &getContext();
  // context->getOrLoadDialect<mlir::arts::ArtsDialect>();

  LLVM_DEBUG(dbgs() << "Loaded dialects: "<< context->getLoadedDialects().size() << "\n");
  LLVM_DEBUG(dbgs() << "Available dialects: "<< context->getAvailableDialects().size() << "\n");

  for (auto &dialect : context->getLoadedDialects()) {
    LLVM_DEBUG(dbgs() << "Loaded dialect: " << dialect->getNamespace() << "\n");
  }

  for(auto &dialect : context->getAvailableDialects()) {
    LLVM_DEBUG(dbgs() << "Available dialect: " << dialect << "\n");
  }


  ModuleOp mod = getOperation();
  /// Analyze each function and skip declarations
  for (auto func : mod.getOps<func::FuncOp>()) {
    if (func.isDeclaration())
      continue;
    runOnFunction(func);
  }
  LLVM_DEBUG(dbgs() << line << "ConvertOpenMPToARTSPass FINISHED\n" << line);
}

void ConvertOpenMPToARTSPass::runOnFunction(func::FuncOp func) {
  LLVM_DEBUG(DBGS() << "Handle function: " << func.getName() << "\n");

  /// Create a hierarchical OMP structure and collect regions
  {
    auto &entryBlk = func.getBody().front();
    for (auto &op : entryBlk)
      buildOmpHierarchy(&op, /*parent=*/nullptr, ompHierarchy);
  }
  LLVM_DEBUG(DBGS() << "OMP hierarchy.\n");
  for (auto &nodePtr : ompHierarchy)
    printOmpHierarchy(*nodePtr);

  /// Transform each function
  transformFunction(func);
  /// If there are any tasks, analyze dependency buffers.
  // analyzeDepBuffers();
}

void ConvertOpenMPToARTSPass::transformFunction(func::FuncOp func) {
  LLVM_DEBUG(DBGS() << "Transforming function: " << func.getName() << "\n");
  /// Rewrite each parallel, master, and task
  for (auto &nodePtr : ompHierarchy) {
    auto &node = *nodePtr;
    switch (node.kind) {
    case OmpNode::Kind::Parallel:
      rewriteParallel(cast<omp::ParallelOp>(node.op));
      break;
    case OmpNode::Kind::Master:
      rewriteMaster(cast<omp::MasterOp>(node.op));
      break;
    case OmpNode::Kind::Task:
      rewriteTask(cast<omp::TaskOp>(node.op));
      break;
    }
  }
}

void ConvertOpenMPToARTSPass::buildOmpHierarchy(
    Operation *op, OmpNode *parent, SmallVector<OmpNode *> &topLevel) {
  if (!op)
    return;

  auto handleNode = [&](OmpNode::Kind K) {
    auto node = new OmpNode(K, op, parent);
    if (parent)
      parent->children.push_back(std::move(node));
    else
      topLevel.push_back(node);
    return (parent ? parent->children.back() : topLevel.back());
  };

  auto processRegion = [&](Region &region, OmpNode *newNode) {
    if (!region.empty()) {
      auto &blk = region.front();
      for (auto &nestedOp : blk)
        buildOmpHierarchy(&nestedOp, newNode, topLevel);
    }
  };

  /// Parallel
  if (auto par = dyn_cast<omp::ParallelOp>(op)) {
    OmpNode *newNode = handleNode(OmpNode::Kind::Parallel);
    processRegion(par.getRegion(), newNode);
    parallelMap[par] = newNode;
    return;
  }

  /// Master
  if (auto mast = dyn_cast<omp::MasterOp>(op)) {
    OmpNode *newNode = handleNode(OmpNode::Kind::Master);
    processRegion(mast.getRegion(), newNode);
    masterMap[mast] = newNode;
    return;
  }

  /// Task
  if (auto task = dyn_cast<omp::TaskOp>(op)) {
    OmpNode *newNode = handleNode(OmpNode::Kind::Task);
    processRegion(task.getRegion(), newNode);
    taskMap[task] = newNode;
    return;
  }

  /// Other ops
  for (auto &region : op->getRegions()) {
    for (auto &block : region) {
      for (auto &nestedOp : block) {
        buildOmpHierarchy(&nestedOp, parent, topLevel);
      }
    }
  }
}

void ConvertOpenMPToARTSPass::printOmpHierarchy(OmpNode &node, int indent) {
  /// Indent
  for (int i = 0; i < indent; i++) {
    LLVM_DEBUG(dbgs() << " ");
  }
  /// Print node
  switch (node.kind) {
  case OmpNode::Kind::Parallel:
    LLVM_DEBUG(dbgs() << "Parallel\n");
    break;
  case OmpNode::Kind::Master:
    LLVM_DEBUG(dbgs() << "Master\n");
    break;
  case OmpNode::Kind::Task:
    LLVM_DEBUG(dbgs() << "Task\n");
    break;
  }

  for (auto &childPtr : node.children)
    printOmpHierarchy(*childPtr, indent + 2);
}

void ConvertOpenMPToARTSPass::getDepBuffers() {
  if (taskMap.empty())
    return;

  LLVM_DEBUG(DBGS() << "There are " << taskMap.size() << " tasks.\n");
  /// Get dependency buffers by analyzing the 'depend' clauses
  for (auto &taskPair : taskMap) {
    auto task = taskPair.first;
    // auto node = taskPair.second;

    /// Analyze depend clauses
    auto dependList = task.getDependsAttr();
    for (unsigned i = 0, e = dependList.size(); i < e; ++i) {
      auto depClause =
          llvm::cast<mlir::omp::ClauseTaskDependAttr>(dependList[i]);
      auto depType = depClause.getValue();
      auto depVar = task.getDependVars()[i];
      LLVM_DEBUG(dbgs() << "Depend clause: "
                        << stringifyClauseTaskDepend(depType) << " -> "
                        << depVar << "\n");
      depBufferMap[depVar].push_back(task);
    }
  }
}

void ConvertOpenMPToARTSPass::analyzeDepBuffers() {
  /// Get dependency buffers
  getDepBuffers();

  /// Analyze uses of the dependency buffers
  LLVM_DEBUG(DBGS() << "Dependency buffer analysis - Size: "
                    << depBufferMap.size() << "\n");
  for (auto &depPair : depBufferMap) {
    auto depVar = depPair.first;
    // auto &tasks = depPair.second;

    LLVM_DEBUG(dbgs() << "Dependency buffer: " << depVar << "\n");
    for (Operation *user : depVar.getUsers()) {
      /// If the user is a task, ignore it - We already knew that
      if (isa<omp::TaskOp>(user))
        continue;
      LLVM_DEBUG(dbgs() << "User: " << *user << "\n");
      /// We are mostly interested in the stores
      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        auto storeVal = store.getValue();
        LLVM_DEBUG(dbgs() << "Store: " << storeVal << "\n");
        /// Analyze the store value
        if (auto load = storeVal.getDefiningOp<memref::LoadOp>()) {
          LLVM_DEBUG(dbgs() << "Load: " << load << "\n");
          /// Check if there are other loads of the same memref
          for (Operation *loadUser : load.getMemref().getUsers()) {
            if (auto otherLoad = dyn_cast<memref::LoadOp>(loadUser)) {
              LLVM_DEBUG(dbgs() << "Other Load: " << otherLoad << "\n");
              /// Further analysis can be done here
            }
          }
        }
      }
    }
    /// Analyze the uses of the depVar

    // for (auto &task : tasks) {
    //   LLVM_DEBUG(dbgs() << "Task: " << task << "\n");
    // }
  }
}

void ConvertOpenMPToARTSPass::rewriteParallel(omp::ParallelOp parOp) {
  auto loc = parOp.getLoc();
  OpBuilder b(parOp);
  auto artsPar = b.create<arts::ParallelOp>(loc);
  artsPar.getBody().push_back(new Block());
  Block &blk = artsPar.getBody().front();
  {
    Block &old = parOp.getRegion().front();
    while (!old.empty()) {
      Operation &movable = old.front();
      movable.moveBefore(&blk, blk.end());
    }
    b.setInsertionPointToEnd(&blk);
    b.create<arts::YieldOp>(loc);
  }
  parOp.erase();
}

void ConvertOpenMPToARTSPass::rewriteMaster(omp::MasterOp mastOp) {
  auto loc = mastOp.getLoc();
  OpBuilder b(mastOp);
  auto single = b.create<arts::SingleOp>(loc);
  single.getBody().push_back(new Block());
  Block &blk = single.getBody().front();
  {
    Block &old = mastOp.getRegion().front();
    while (!old.empty()) {
      Operation &movable = old.front();
      movable.moveBefore(&blk, blk.end());
    }
    b.setInsertionPointToEnd(&blk);
    b.create<arts::YieldOp>(loc);
  }
  mastOp.erase();
}

void ConvertOpenMPToARTSPass::rewriteTask(omp::TaskOp task) {
  // We'll gather the existing depend(...) clauses to build
  // the arts.edt dependencies. Also gather read/write from ephemeral usage.

  // Example of reading the depends:
  //   for (auto clause : task.getDependClause()) {
  //     // e.g. clause might be "taskdependout -> %alloca"
  //     auto depClause = clause.cast<omp::DependClauseAttr>();
  //     ...
  //   }

  // Create arts.edt with "parameters" we infer from used-values
  // and "dependencies" from the OMP depends clauses.

  OpBuilder b(task);
  auto depArray = buildDependencyArray(b, task);
  SmallVector<Value> paramArr = buildParameterArray(b, task);

  auto edtOp = b.create<arts::EdtOp>(task.getLoc(), paramArr, depArray);
  {
    // Move the body into edtOp
    auto &r = edtOp.getBody().emplaceBlock();
    b.setInsertionPointToEnd(&r);
    b.create<arts::YieldOp>(task.getLoc());

    // Move old region
    Block &oldBlk = task.getRegion().front();
    while (!oldBlk.empty()) {
      Operation &opToMove = oldBlk.front();
      opToMove.moveBefore(&r, r.end());
    }
  }
  task.erase();
}

ArrayAttr ConvertOpenMPToARTSPass::buildDependencyArray(OpBuilder &builder,
                                                        omp::TaskOp task) {
  SmallVector<Attribute> deps;
  // Example: read the "depend" clause directly
  //   for (auto clause : task.getDependClause()) {
  //     auto depClause = clause.cast<omp::DependClauseAttr>();
  //     auto depType = depClause.getDependencyType(); // in/out/inout
  //     for (auto var : depClause.getVars()) {
  //       // build a dictionary
  //       //   { mode = "in", memref=var, indices=? }
  //       // If it's rank-1 or single index, store it in "indices"
  //       // else store empty.
  //       // This is a simplified approach
  //       // (you might want a real representation of subviews).
  //       auto modeStr = stringifyClauseDependencyType(depType);
  //       auto dict = builder.getDictionaryAttr({
  //         builder.getNamedAttr("mode", builder.getStringAttr(modeStr)),
  //         builder.getNamedAttr("memref",
  //           builder.getStringAttr(varNameOrSomething(var))),
  //         builder.getNamedAttr("indices", builder.getArrayAttr({/*...*/}))
  //       });
  //       deps.push_back(dict);
  //     }
  //   }
  return builder.getArrayAttr(deps);
}

SmallVector<Value>
ConvertOpenMPToARTSPass::buildParameterArray(OpBuilder &builder,
                                             omp::TaskOp task) {
  // Typical approach: getUsedValuesDefinedAbove(task.getRegion(), taskRegion,
  // used) For demonstration, return empty
  SmallVector<Value> result;
  return result;
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
unique_ptr<Pass> createConvertOpenMPtoARTSPass() {
  return std::make_unique<ConvertOpenMPToARTSPass>();
}
} // namespace arts
} // namespace mlir