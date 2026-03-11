///==========================================================================///
/// File: StringAnalysis.h
///
/// This class analyzes the module and itentifies the set of string memref.
///==========================================================================///

#ifndef CARTS_ANALYSIS_STRINGANALYSIS_H
#define CARTS_ANALYSIS_STRINGANALYSIS_H

#include "arts/Analysis/ArtsAnalysis.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// StringAnalysis
///===----------------------------------------------------------------------===///
class StringAnalysis : public ArtsAnalysis {
public:
  explicit StringAnalysis(ArtsAnalysisManager &manager);

  void run();
  void invalidate() override;

  bool isStringMemRef(Value value) const;
  const DenseSet<Value> &getStringMemRefs() const;
  const DenseMap<Value, LLVM::GlobalOp> &getGlobalSources() const;

private:
  void ensureAnalyzed() const;

  /// Cached results from the analysis.
  DenseSet<Value> stringMemRefs;
  DenseMap<Value, LLVM::GlobalOp> globalSources;
  ModuleOp module;
  mutable bool built = false;

  /// Check whether the LLVM global contains a null-terminated string.
  bool isStringGlobal(LLVM::GlobalOp globalOp);

  /// Recursively track users of the given global value.
  void trackGlobalUsers();

  /// Track memrefs that are initialized from string globals.
  void trackMemrefInitializations();

  /// Analyze all call operations for string functions and extract underlying
  /// memrefs.
  void trackCallOps();

  /// Helper to recognize string-related functions.
  bool isStringFunction(StringRef funcName);
};

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_STRINGANALYSIS_H
