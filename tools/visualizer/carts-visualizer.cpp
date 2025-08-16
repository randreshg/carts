//==========================================================================
// File: carts-visualizer.cpp
// Purpose: Visualize ARTS Db/Edt graphs using ArtsGraphManager
//==========================================================================

#include "mlir/InitAllDialects.h"
#include "mlir/InitAllExtensions.h"
#include "mlir/InitAllPasses.h"
#include "mlir/InitAllTranslations.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Support/FileUtilities.h"
#include "mlir/Support/LogicalResult.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/ArtsDialect.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/ArtsGraphManager.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"

using namespace llvm;
using namespace mlir;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input .mlir file>"), cl::init("-"));

static cl::opt<std::string> OutputFilename(
    "o", cl::desc("Output filename (DOT)"), cl::value_desc("filename"),
    cl::init("-"));

static cl::opt<std::string> FunctionName(
    "func", cl::desc("Function to visualize (defaults to first func)"),
    cl::value_desc("name"), cl::init(""));

static cl::opt<bool> ConcurrencyView(
    "concurrency", cl::desc("Emit concurrency view instead of raw graphs"),
    cl::init(false));

static cl::opt<bool> SummaryOnly(
    "summary", cl::desc("Print summary to stderr (no DOT)"), cl::init(false));

static cl::opt<bool> Json(
    "json", cl::desc("Emit JSON instead of DOT"), cl::init(false));

/// Register dialects needed by analyses.
static void registerDialects(DialectRegistry &registry) {
  registry.insert<mlir::arts::ArtsDialect, mlir::func::FuncDialect>();
  mlir::registerAllPasses();
  mlir::registerAllTranslations();
  mlir::registerAllDialects(registry);
  mlir::registerAllExtensions(registry);
}

int main(int argc, char **argv) {
  InitLLVM y(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "CARTS Graph Visualizer\n");

  // Setup context/dialects
  DialectRegistry registry;
  registerDialects(registry);
  MLIRContext context(registry);
  context.disableMultithreading(true);

  // Open input
  auto file = openInputFile(InputFilename);
  if (!file) {
    errs() << "Error: Could not open input file: " << InputFilename << "\n";
    return 1;
  }

  // Parse module
  auto module = parseSourceString<ModuleOp>(file->getBuffer(), &context);
  if (!module) {
    errs() << "Error: Could not parse input file\n";
    return 1;
  }

  // Find function
  func::FuncOp targetFunc;
  module->walk([&](func::FuncOp f) {
    if (targetFunc)
      return;
    if (FunctionName.empty() || f.getName() == FunctionName)
      targetFunc = f;
  });

  if (!targetFunc) {
    errs() << "Error: No function found" << (FunctionName.empty() ? "" : (" with name '" + FunctionName + "'")) << "\n";
    return 1;
  }

  // Construct graphs and manager
  mlir::arts::DbAnalysis dbAnalysis(module.get());
  // DbGraph requires DbAnalysis*, EdtGraph requires DbGraph*
  auto dbGraph = std::make_unique<mlir::arts::DbGraph>(targetFunc, &dbAnalysis);
  auto edtGraph = std::make_unique<mlir::arts::EdtGraph>(targetFunc, dbGraph.get());
  mlir::arts::ArtsGraphManager manager(std::move(dbGraph), std::move(edtGraph));

  // Build
  manager.build();

  // Output
  if (SummaryOnly) {
    manager.print(errs());
    return 0;
  }

  auto output = openOutputFile(OutputFilename);
  if (!output) {
    errs() << "Error: Could not open output file: " << OutputFilename << "\n";
    return 1;
  }

  if (Json) {
    manager.exportToJson(output->os());
  } else if (ConcurrencyView) {
    manager.buildConcurrencyView(output->os());
  } else {
    manager.exportToDot(output->os());
  }

  output->keep();
  return 0;
}


