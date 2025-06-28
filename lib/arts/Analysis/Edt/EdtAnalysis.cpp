///==========================================================================
/// File: EdtAnalysis.cpp
///==========================================================================

#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Utils/ArtsUtils.h"
/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
/// Others
#include "mlir/Transforms/RegionUtils.h"

/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "edt-analysis"
#define LINE "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

///==========================================================================
/// EdtEnvManager
///==========================================================================
EdtEnvManager::EdtEnvManager(PatternRewriter &rewriter, Region &region)
    : rewriter(rewriter), region(region) {}

EdtEnvManager::~EdtEnvManager() {}

void EdtEnvManager::naiveCollection(bool ignoreDeps) {
  // LLVM_DEBUG(dbgs() << LINE
  //                   << "Naive collection of parameters and dependencies:
  //                   \n");
  /// Ignore collecting dependencies if there are already depsToProcess
  /// This is the case of omp tasks
  if (!depsToProcess.empty())
    ignoreDeps = true;

  /// Checks if the value is a constant, if so, ignore it
  auto isConstant = [&](Value val) {
    auto defOp = val.getDefiningOp();
    if (!defOp)
      return false;

    auto constantOp = dyn_cast<arith::ConstantOp>(defOp);
    if (!constantOp)
      return false;
    constants.insert(val);
    return true;
  };

  /// Collect all the values used in the region that are defined above it
  SetVector<Value> usedValues;
  getUsedValuesDefinedAbove(region, usedValues);
  for (Value operand : usedValues) {
    if (operand.getType().isIntOrIndexOrFloat()) {
      if (isConstant(operand))
        continue;
      // LLVM_DEBUG(dbgs() << "  Adding parameter: " << operand << "\n");
      parameters.insert(operand);
    } else if (!ignoreDeps) {
      // LLVM_DEBUG(dbgs() << "  Adding dependency: " << operand << "\n");
      depsToProcess[operand] = "inout";
    }
  }
  LLVM_DEBUG(dbgs() << LINE);
}

void EdtEnvManager::adjust() {
  /// Analyze the parameters.
  /// Check if there is any parameter that loads a memref and indices of any
  /// dependency
  DenseMap<Value, SmallVector<arts::DbDepOp>> baseToDepsMap;
  for (auto dep : dependencies) {
    auto depOp = cast<arts::DbDepOp>(dep.getDefiningOp());
    auto depBase = depOp.getSource();
    baseToDepsMap[depBase].push_back(depOp);
    if (parameters.contains(depBase))
      parameters.remove(depBase);
  }

  SmallVector<Value> paramsToRemove;
  for (auto param : parameters) {
    auto defOp = param.getDefiningOp();
    if (!defOp)
      continue;

    /// We are only interested in memref.load operations
    auto loadOp = dyn_cast<memref::LoadOp>(defOp);
    if (!loadOp)
      continue;

    /// If the base ptr of the load is not a dep, continue
    auto memref = loadOp.getMemRef();
    auto indices = loadOp.getIndices();
    if (!baseToDepsMap.count(memref))
      continue;

    /// Get all the dependencies that have the same base as the load
    for (auto dep : baseToDepsMap[memref]) {
      auto depIndices = dep.getIndices();
      if (indices.size() != depIndices.size())
        continue;

      /// Check if the indices match
      if (!llvm::all_of(llvm::zip(indices, depIndices), [](auto pair) {
            return std::get<0>(pair) == std::get<1>(pair);
          }))
        continue;

      /// Insert a new load at the start of the region, and replace all the
      /// uses of the parameter
      OpBuilder::InsertionGuard guard(rewriter);
      rewriter.setInsertionPointToStart(&region.front());
      auto newLoad = rewriter.create<memref::LoadOp>(loadOp.getLoc(),
                                                     dep.getResult(), indices);
      replaceInRegion(region, loadOp.getResult(), newLoad.getResult());

      /// Remove the parameter
      paramsToRemove.push_back(param);
    }
  }

  /// Remove the parameters
  for (auto param : paramsToRemove)
    parameters.remove(param);
}

bool EdtEnvManager::addParameter(Value val) { return parameters.insert(val); }

void EdtEnvManager::addDependency(Value val, StringRef mode) {
  /// If the dependency is not a db operation, add it to depsToProcess
  auto depOp = dyn_cast<arts::DbDepOp>(val.getDefiningOp());
  if (!depOp) {
    depsToProcess[val] = mode;
    return;
  }
  dependencies.insert(depOp);
}

void EdtEnvManager::print() {
  std::string debugInfo;
  llvm::raw_string_ostream debugStream(debugInfo);
  debugStream << LINE << "EDT Environment:\n";
  debugStream << "- Parameters:\n";
  for (auto param : parameters)
    debugStream << "   " << param << "\n";
  debugStream << "\n";

  debugStream << "- Constants:\n";
  for (auto constant : constants)
    debugStream << "   " << constant << "\n";
  debugStream << "\n";

  debugStream << "- Dependencies:\n";
  for (auto dep : dependencies)
    debugStream << "   " << dep << "\n";
  debugStream << LINE;

  LLVM_DEBUG(dbgs() << debugStream.str());
}

///==========================================================================
/// EdtAnalysis
///==========================================================================