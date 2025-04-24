///==========================================================================
/// File: EdtAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.edt operations.
/// It also provides helpers to analyze regions and find dependencies,
/// parameters, and constants.
///==========================================================================

#ifndef CARTS_ANALYSIS_EDTANALYSIS_H
#define CARTS_ANALYSIS_EDTANALYSIS_H

#include "arts/ArtsDialect.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace mlir {
namespace arts {

///==========================================================================
/// EdtEnvManager Class Declaration
///==========================================================================
class EdtEnvManager {
public:
  EdtEnvManager(PatternRewriter &rewriter, Region &region);
  ~EdtEnvManager();

  /// Collect parameters and dependencies
  void naiveCollection(bool ignoreDeps = false);

  /// Analyze and adjust the set of parameters
  void adjust();

  /// Add interface
  bool addParameter(Value val);
  void addDependency(Value val, StringRef mode = "inout");

  /// Print
  void print();

  /// Getters
  Region &getRegion() { return region; }
  ArrayRef<Value> getParameters() { return parameters.getArrayRef(); }
  ArrayRef<Value> getConstants() { return constants.getArrayRef(); }
  ArrayRef<Value> getDependencies() { return dependencies.getArrayRef(); }
  DenseMap<Value, StringRef> &getDepsToProcess() { return depsToProcess; }

  void clearDepsToProcess() { depsToProcess.clear(); }

private:
  PatternRewriter &rewriter;
  Region &region;
  /// Set of possible parameters, constants, and dependencies
  SetVector<Value> parameters, constants, dependencies;
  /// Set of dependencies to process
  DenseMap<Value, StringRef> depsToProcess;
};

///==========================================================================
/// EdtAnalysis Class Declaration
///==========================================================================
// class EdtAnalysis {
// public:
//   explicit EdtAnalysis(Operation *module);

// private:
// };

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_EDTANALYSIS_H