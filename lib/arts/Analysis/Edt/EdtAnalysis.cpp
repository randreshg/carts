///==========================================================================
/// File: EdtAnalysis.cpp
///==========================================================================

#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/ArtsDialect.h"
/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
/// Others
#include "mlir/Transforms/RegionUtils.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"

/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt_analysis)

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
      parameters.insert(operand);
    } else if (!ignoreDeps) {
      depsToProcess[operand] = "inout";
    }
  }
}

void EdtEnvManager::adjust() {
  /// Analyze the parameters.
  /// Check if there is any parameter that loads a memref and indices of any
  /// dependency
  // DenseMap<Value, SmallVector<arts::DbDepOp>> baseToDepsMap;
  // for (auto dep : dependencies) {
  //   auto depOp = cast<arts::DbDepOp>(dep.getDefiningOp());
  //   auto depBase = depOp.getSource();
  //   baseToDepsMap[depBase].push_back(depOp);
  //   if (parameters.contains(depBase))
  //     parameters.remove(depBase);
  // }

  // SmallVector<Value> paramsToRemove;
  // for (auto param : parameters) {
  //   auto defOp = param.getDefiningOp();
  //   if (!defOp)
  //     continue;

  //   /// We are only interested in memref.load operations
  //   auto loadOp = dyn_cast<memref::LoadOp>(defOp);
  //   if (!loadOp)
  //     continue;

  //   /// If the base ptr of the load is not a dep, continue
  //   auto memref = loadOp.getMemRef();
  //   auto indices = loadOp.getIndices();
  //   if (!baseToDepsMap.count(memref))
  //     continue;

  //   /// Get all the dependencies that have the same base as the load
  //   for (auto dep : baseToDepsMap[memref]) {
  //     auto depIndices = dep.getIndices();
  //     if (indices.size() != depIndices.size())
  //       continue;

  //     /// Check if the indices match
  //     if (!llvm::all_of(llvm::zip(indices, depIndices), [](auto pair) {
  //           return std::get<0>(pair) == std::get<1>(pair);
  //         }))
  //       continue;

  //     /// Insert a new load at the start of the region, and replace all the
  //     /// uses of the parameter
  //     OpBuilder::InsertionGuard guard(rewriter);
  //     rewriter.setInsertionPointToStart(&region.front());
  //     auto newLoad = rewriter.create<memref::LoadOp>(loadOp.getLoc(),
  //                                                    dep.getResult(), indices);
  //     replaceInRegion(region, loadOp.getResult(), newLoad.getResult());

  //     /// Remove the parameter
  //     paramsToRemove.push_back(param);
  //   }
  // }

  /// Remove the parameters
  // for (auto param : paramsToRemove)
  //   parameters.remove(param);
}

bool EdtEnvManager::addParameter(Value val) { return parameters.insert(val); }

void EdtEnvManager::addDependency(Value val, StringRef mode) {
  /// If the dependency is not a db operation, add it to depsToProcess
  // auto depOp = dyn_cast<arts::DbDepOp>(val.getDefiningOp());
  // if (!depOp) {
  //   depsToProcess[val] = mode;
  //   return;
  // }
  // dependencies.insert(depOp);
}

void EdtEnvManager::print() {
  std::string debugInfo;
  llvm::raw_string_ostream debugStream(debugInfo);
  debugStream << ARTS_LINE << "EDT Environment:\n";
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
  debugStream << ARTS_LINE;
  debugStream.flush();
  // ARTS_INFO(llvm::StringRef(debugInfo));
  // ARTS_INFO(debugInfo);
}

///==========================================================================
/// EdtAnalysis
///==========================================================================

static Value getOriginalAllocationInternal(Value v) {
  if (v.isa<BlockArgument>()) {
    Block *block = v.getParentBlock();
    Operation *owner = block->getParentOp();
    if (auto edt = dyn_cast<EdtOp>(owner)) {
      unsigned argIndex = v.cast<BlockArgument>().getArgNumber();
      Value operand = edt.getOperand(argIndex);
      return getOriginalAllocationInternal(operand);
    }
    return v;
  }
  if (Operation *op = v.getDefiningOp()) {
    if (auto castOp = dyn_cast<memref::CastOp>(op))
      return getOriginalAllocationInternal(castOp.getSource());
    if (auto subview = dyn_cast<memref::SubViewOp>(op))
      return getOriginalAllocationInternal(subview.getSource());
    if (isa<memref::AllocOp, memref::AllocaOp, memref::GetGlobalOp>(op))
      return v;
  }
  return v;
}

Value EdtAnalysis::getOriginalAllocation(Value v) { return getOriginalAllocationInternal(v); }

unsigned EdtAnalysis::getElementByteWidth(Type t) {
  if (auto it = t.dyn_cast<IntegerType>()) return it.getWidth() / 8u;
  if (auto ft = t.dyn_cast<FloatType>()) return ft.getWidth() / 8u;
  return 0u;
}

uint64_t EdtAnalysis::estimateMaxLoopDepth(Region &region) {
  // Lightweight heuristic: count nested scf.for/while/parallel nesting
  uint64_t maxDepth = 0, cur = 0;
  region.walk([&](Operation *op) {
    if (isa<scf::ForOp, scf::WhileOp, scf::ParallelOp>(op)) {
      ++cur;
      maxDepth = std::max(maxDepth, cur);
    }
    // Reset when leaving block; for conservativeness, ignore precise scopes
  });
  return maxDepth;
}

EdtAnalysis::EdtAnalysis(Operation *module, DbGraph *db, EdtGraph *edt)
    : module(module), dbGraph(db), edtGraph(edt) {}

void EdtAnalysis::analyze() {
  summaries.clear();
  module->walk([&](func::FuncOp func) { analyzeFunc(func); });
}

void EdtAnalysis::analyzeFunc(func::FuncOp func) {
  // Assign order index in program order per function for reuse proximity
  unsigned idx = 0;
  func.walk([&](EdtOp edt) {
    Operation *op = edt.getOperation();
    taskOrderIndex[op] = idx++;
    summaries[op] = summarizeEdt(op);
  });
}

const EdtTaskSummary *EdtAnalysis::getSummary(Operation *edtOp) const {
  auto it = summaries.find(edtOp);
  if (it == summaries.end()) return nullptr;
  return &it->second;
}

EdtTaskSummary EdtAnalysis::summarizeEdt(Operation *edtOp) const {
  EdtTaskSummary s;
  // Count ops and memory behavior; estimate bytes using element type
  static_cast<EdtOp>(edtOp).walk([&](Operation *op) {
    ++s.totalOps;
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      ++s.numLoads;
      auto mt = load.getMemref().getType().cast<MemRefType>();
      uint64_t elemBytes = getElementByteWidth(mt.getElementType());
      s.bytesRead += elemBytes;
      Value base = getOriginalAllocation(load.getMemref());
      if (base) s.basesRead.insert(base);
      if (dbGraph) {
        if (auto acq = base.getDefiningOp<DbAcquireOp>()) {
          if (auto alloc = dbGraph->getParentAlloc(acq)) {
            Operation *allocOp = alloc.getOperation();
            s.dbAllocsRead.insert(allocOp);
            s.dbAllocAccessCount[allocOp] += 1;
            s.dbAllocAccessBytes[allocOp] += elemBytes;
          }
        } else if (auto rel = base.getDefiningOp<DbReleaseOp>()) {
          if (auto alloc = dbGraph->getParentAlloc(rel)) {
            Operation *allocOp = alloc.getOperation();
            s.dbAllocsRead.insert(allocOp);
            s.dbAllocAccessCount[allocOp] += 1;
            s.dbAllocAccessBytes[allocOp] += elemBytes;
          }
        } else if (auto alloc = base.getDefiningOp<DbAllocOp>()) {
          Operation *allocOp = alloc.getOperation();
          s.dbAllocsRead.insert(allocOp);
          s.dbAllocAccessCount[allocOp] += 1;
          s.dbAllocAccessBytes[allocOp] += elemBytes;
        }
      }
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      ++s.numStores;
      auto mt = store.getMemref().getType().cast<MemRefType>();
      uint64_t elemBytes = getElementByteWidth(mt.getElementType());
      s.bytesWritten += elemBytes;
      Value base = getOriginalAllocation(store.getMemref());
      if (base) s.basesWritten.insert(base);
      if (dbGraph) {
        if (auto acq = base.getDefiningOp<DbAcquireOp>()) {
          if (auto alloc = dbGraph->getParentAlloc(acq)) {
            Operation *allocOp = alloc.getOperation();
            s.dbAllocsWritten.insert(allocOp);
            s.dbAllocAccessCount[allocOp] += 1;
            s.dbAllocAccessBytes[allocOp] += elemBytes;
          }
        } else if (auto rel = base.getDefiningOp<DbReleaseOp>()) {
          if (auto alloc = dbGraph->getParentAlloc(rel)) {
            Operation *allocOp = alloc.getOperation();
            s.dbAllocsWritten.insert(allocOp);
            s.dbAllocAccessCount[allocOp] += 1;
            s.dbAllocAccessBytes[allocOp] += elemBytes;
          }
        } else if (auto alloc = base.getDefiningOp<DbAllocOp>()) {
          Operation *allocOp = alloc.getOperation();
          s.dbAllocsWritten.insert(allocOp);
          s.dbAllocAccessCount[allocOp] += 1;
          s.dbAllocAccessBytes[allocOp] += elemBytes;
        }
      }
    } else if (isa<func::CallOp>(op)) {
      ++s.numCalls;
    }
  });
  uint64_t memOps = s.numLoads + s.numStores;
  s.computeToMemRatio = memOps ? (double)(s.totalOps - memOps) / (double)memOps : (double)s.totalOps;
  s.maxLoopDepth = estimateMaxLoopDepth(static_cast<EdtOp>(edtOp).getRegion());
  return s;
}

static double jaccardOverlap(const DenseSet<Value> &a, const DenseSet<Value> &b) {
  if (a.empty() && b.empty()) return 0.0;
  size_t inter = 0;
  for (auto v : a) inter += b.contains(v);
  size_t uni = a.size();
  for (auto v : b) if (!a.contains(v)) ++uni;
  return uni ? (double)inter / (double)uni : 0.0;
}

EdtPairAffinity EdtAnalysis::affinity(Operation *aOp, Operation *bOp) const {
  EdtPairAffinity aff;
  auto *sa = getSummary(aOp);
  auto *sb = getSummary(bOp);
  if (!sa || !sb) return aff;
  DenseSet<Value> readA = sa->basesRead, writeA = sa->basesWritten;
  DenseSet<Value> readB = sb->basesRead, writeB = sb->basesWritten;

  DenseSet<Value> basesA = readA;
  for (auto v : writeA) basesA.insert(v);
  DenseSet<Value> basesB = readB;
  for (auto v : writeB) basesB.insert(v);

  aff.dataOverlap = jaccardOverlap(basesA, basesB);

  // Hazard: overlapping bases with at least one writer
  size_t overlap = 0, hazard = 0;
  for (auto v : basesA) if (basesB.contains(v)) {
    ++overlap;
    bool writer = writeA.contains(v) || writeB.contains(v);
    hazard += writer;
  }
  aff.hazardScore = overlap ? (double)hazard / (double)overlap : 0.0;
  aff.mayConflict = aff.hazardScore > 0.0;

  // Reuse proximity: inverse of program distance (normalize by total tasks)
  auto ita = taskOrderIndex.find(aOp);
  auto itb = taskOrderIndex.find(bOp);
  if (ita != taskOrderIndex.end() && itb != taskOrderIndex.end() && ita->second != itb->second) {
    unsigned dist = ita->second > itb->second ? ita->second - itb->second : itb->second - ita->second;
    double prox = 1.0 / (1.0 + (double)dist);
    aff.reuseProximity = prox;
  }
  aff.localityScore = aff.dataOverlap * aff.reuseProximity;
  aff.concurrencyRisk = aff.hazardScore * aff.dataOverlap;
  return aff;
}

void EdtAnalysis::print(func::FuncOp func, llvm::raw_ostream &os) const {
  os << "EDT Analysis for function: " << func.getName() << "\n";
  func.walk([&](EdtOp edt) {
    if (auto *s = getSummary(edt)) {
      os << "- EDT at" << edt.getLoc() << ": ops=" << s->totalOps
         << ", loads=" << s->numLoads << ", stores=" << s->numStores
         << ", calls=" << s->numCalls << ", RO=" << s->isReadOnly()
         << ", RW=" << s->isReadWrite() << ", C/M=" << s->computeToMemRatio
         << ", depth=" << s->maxLoopDepth << "\n";
    }
  });
}

void EdtAnalysis::toJson(func::FuncOp func, llvm::raw_ostream &os) const {
  os << "{\n  \"function\": \"" << func.getName() << "\",\n  \"edts\": [\n";
  bool first = true;
  func.walk([&](EdtOp edt) {
    auto *s = getSummary(edt);
    if (!s) return;
    if (!first) os << ",\n";
    first = false;
    os << "    {\"loc\": \"" << edt.getLoc() << "\", "
          "\"ops\": " << s->totalOps << ", \"loads\": " << s->numLoads
       << ", \"stores\": " << s->numStores << ", \"calls\": " << s->numCalls
       << ", \"bytesR\": " << s->bytesRead << ", \"bytesW\": " << s->bytesWritten
       << ", \"depth\": " << s->maxLoopDepth << ", \"cmr\": " << s->computeToMemRatio << "}";
  });
  os << "\n  ]\n}\n";
}

void EdtAnalysis::emitEdtsArray(func::FuncOp func, llvm::raw_ostream &os) const {
  os << "\"edts\": [\n";
  bool first = true;
  func.walk([&](EdtOp edt) {
    auto *s = getSummary(edt);
    if (!s) return;
    if (!first) os << ",\n";
    first = false;
    os << "  {\"loc\": \"" << edt.getLoc() << "\", \"ops\": " << s->totalOps
       << ", \"loads\": " << s->numLoads << ", \"stores\": " << s->numStores
       << ", \"calls\": " << s->numCalls << ", \"bytesR\": " << s->bytesRead
       << ", \"bytesW\": " << s->bytesWritten << ", \"depth\": " << s->maxLoopDepth
       << ", \"cmr\": " << s->computeToMemRatio << "}";
  });
  os << "\n]";
}