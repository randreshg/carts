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
    if (auto alloc = dyn_cast<memref::AllocOp>(op))
      return alloc.getMemref();
  }
  return v;
}

Value EdtAnalysis::getOriginalAllocation(Value v) {
  return getOriginalAllocationInternal(v);
}

unsigned EdtAnalysis::getElementByteWidth(Type t) {
  if (auto memref = t.dyn_cast<MemRefType>()) {
    Type elementType = memref.getElementType();
    return elementType.getIntOrFloatBitWidth() / 8;
  }
  return 0;
}

uint64_t EdtAnalysis::estimateMaxLoopDepth(Region &region) {
  uint64_t maxDepth = 0;
  region.walk([&](Operation *op) {
    if (isa<scf::ForOp, scf::ParallelOp>(op)) {
      maxDepth++;
    }
  });
  return maxDepth;
}

EdtAnalysis::EdtAnalysis(Operation *module, DbGraph *db, EdtGraph *edt) 
    : module(module), dbGraph(db), edtGraph(edt) {
  // Constructor just initializes; actual analysis is done in analyze()
}

void EdtAnalysis::analyze() {
  if (auto moduleOp = dyn_cast<ModuleOp>(module)) {
    for (auto func : moduleOp.getOps<func::FuncOp>()) {
      analyzeFunc(func);
    }
  }
}

void EdtAnalysis::analyzeFunc(func::FuncOp func) {
  unsigned taskIndex = 0;
  func.walk([&](Operation *op) {
    if (auto edt = dyn_cast<EdtOp>(op)) {
      summaries[op] = summarizeEdt(op);
      taskOrderIndex[op] = taskIndex++;
    }
  });
}

EdtTaskSummary EdtAnalysis::summarizeEdt(Operation *edtOp) const {
  EdtTaskSummary summary;
  auto edt = cast<EdtOp>(edtOp);
  
  // Basic region analysis
  summary.maxLoopDepth = estimateMaxLoopDepth(edt.getRegion());
  
  // Count operations
  edt.getRegion().walk([&](Operation *op) {
    summary.totalOps++;
    if (isa<memref::LoadOp>(op)) {
      summary.numLoads++;
    } else if (isa<memref::StoreOp>(op)) {
      summary.numStores++;
    } else if (isa<func::CallOp>(op)) {
      summary.numCalls++;
    }
  });
  
  // Calculate compute to memory ratio
  uint64_t memOps = summary.numLoads + summary.numStores;
  if (memOps > 0) {
    summary.computeToMemRatio = static_cast<double>(summary.totalOps - memOps) / memOps;
  }
  
  // Analyze captured values for base allocations
  SetVector<Value> capturedValues;
  getUsedValuesDefinedAbove(edt.getRegion(), capturedValues);
  
  for (Value v : capturedValues) {
    if (auto memrefType = v.getType().dyn_cast<MemRefType>()) {
      Value baseAlloc = getOriginalAllocation(v);
      
      // Determine if this is read or written by looking at usage
      bool isRead = false, isWritten = false;
      edt.getRegion().walk([&](Operation *op) {
        for (Value operand : op->getOperands()) {
          if (operand == v) {
            if (isa<memref::LoadOp>(op)) {
              isRead = true;
            } else if (isa<memref::StoreOp>(op)) {
              isWritten = true;
            }
          }
        }
      });
      
      if (isRead) {
        summary.basesRead.insert(baseAlloc);
        summary.bytesRead += memrefType.getNumElements() * getElementByteWidth(memrefType);
      }
      if (isWritten) {
        summary.basesWritten.insert(baseAlloc);
        summary.bytesWritten += memrefType.getNumElements() * getElementByteWidth(memrefType);
      }
    }
  }
  
  return summary;
}

const EdtTaskSummary *EdtAnalysis::getSummary(Operation *edtOp) const {
  auto it = summaries.find(edtOp);
  return it != summaries.end() ? &it->second : nullptr;
}

EdtPairAffinity EdtAnalysis::affinity(Operation *a, Operation *b) const {
  EdtPairAffinity result;
  
  const EdtTaskSummary *summaryA = getSummary(a);
  const EdtTaskSummary *summaryB = getSummary(b);
  
  if (!summaryA || !summaryB) {
    return result; // Return zero affinity
  }
  
  // Calculate data overlap (Jaccard similarity)
  DenseSet<Value> unionBases = summaryA->basesRead;
  unionBases.insert(summaryA->basesWritten.begin(), summaryA->basesWritten.end());
  unionBases.insert(summaryB->basesRead.begin(), summaryB->basesRead.end());
  unionBases.insert(summaryB->basesWritten.begin(), summaryB->basesWritten.end());
  
  DenseSet<Value> intersectionBases;
  for (Value base : summaryA->basesRead) {
    if (summaryB->basesRead.count(base) || summaryB->basesWritten.count(base)) {
      intersectionBases.insert(base);
    }
  }
  for (Value base : summaryA->basesWritten) {
    if (summaryB->basesRead.count(base) || summaryB->basesWritten.count(base)) {
      intersectionBases.insert(base);
    }
  }
  
  if (!unionBases.empty()) {
    result.dataOverlap = static_cast<double>(intersectionBases.size()) / unionBases.size();
  }
  
  // Calculate hazard score (write conflicts)
  unsigned conflicts = 0;
  for (Value base : intersectionBases) {
    if ((summaryA->basesWritten.count(base) && summaryB->basesRead.count(base)) ||
        (summaryA->basesRead.count(base) && summaryB->basesWritten.count(base)) ||
        (summaryA->basesWritten.count(base) && summaryB->basesWritten.count(base))) {
      conflicts++;
    }
  }
  
  if (!intersectionBases.empty()) {
    result.hazardScore = static_cast<double>(conflicts) / intersectionBases.size();
    result.mayConflict = conflicts > 0;
  }
  
  // Simple program order proximity (could be enhanced with actual distance)
  auto orderA = taskOrderIndex.lookup(a);
  auto orderB = taskOrderIndex.lookup(b);
  if (orderA != orderB) {
    result.reuseProximity = 1.0 / (1.0 + std::abs(static_cast<int>(orderA) - static_cast<int>(orderB)));
  }
  
  result.localityScore = result.dataOverlap * result.reuseProximity;
  result.concurrencyRisk = result.hazardScore * result.dataOverlap;
  
  return result;
}

void EdtAnalysis::print(func::FuncOp func, llvm::raw_ostream &os) const {
  os << "EdtAnalysis for function: " << func.getName() << "\n";
  
  func.walk([&](Operation *op) {
    if (auto edt = dyn_cast<EdtOp>(op)) {
      const EdtTaskSummary *summary = getSummary(op);
      if (summary) {
        unsigned taskIndex = taskOrderIndex.lookup(op);
        os << "  EDT #" << taskIndex << ":\n";
        os << "    Total ops: " << summary->totalOps << "\n";
        os << "    Loads: " << summary->numLoads << ", Stores: " << summary->numStores << "\n";
        os << "    Calls: " << summary->numCalls << "\n";
        os << "    Max loop depth: " << summary->maxLoopDepth << "\n";
        os << "    Compute/Memory ratio: " << summary->computeToMemRatio << "\n";
        os << "    Bases read: " << summary->basesRead.size() << ", written: " << summary->basesWritten.size() << "\n";
        os << "    Bytes read: " << summary->bytesRead << ", written: " << summary->bytesWritten << "\n";
      }
    }
  });
}

void EdtAnalysis::toJson(func::FuncOp func, llvm::raw_ostream &os) const {
  os << "{\n";
  os << "  \"function\": \"" << func.getName() << "\",\n";
  os << "  \"edts\": [\n";
  
  bool first = true;
  func.walk([&](Operation *op) {
    if (auto edt = dyn_cast<EdtOp>(op)) {
      const EdtTaskSummary *summary = getSummary(op);
      if (summary) {
        if (!first) os << ",\n";
        first = false;
        
        unsigned taskIndex = taskOrderIndex.lookup(op);
        os << "    {\n";
        os << "      \"taskId\": " << taskIndex << ",\n";
        os << "      \"totalOps\": " << summary->totalOps << ",\n";
        os << "      \"numLoads\": " << summary->numLoads << ",\n";
        os << "      \"numStores\": " << summary->numStores << ",\n";
        os << "      \"maxLoopDepth\": " << summary->maxLoopDepth << ",\n";
        os << "      \"computeToMemRatio\": " << summary->computeToMemRatio << "\n";
        os << "    }";
      }
    }
  });
  
  os << "\n  ]\n";
  os << "}\n";
}

void EdtAnalysis::emitEdtsArray(func::FuncOp func, llvm::raw_ostream &os) const {
  os << "[\n";
  
  bool first = true;
  func.walk([&](Operation *op) {
    if (auto edt = dyn_cast<EdtOp>(op)) {
      const EdtTaskSummary *summary = getSummary(op);
      if (summary) {
        if (!first) os << ",\n";
        first = false;
        
        unsigned taskIndex = taskOrderIndex.lookup(op);
        os << "  {\n";
        os << "    \"id\": " << taskIndex << ",\n";
        os << "    \"ops\": " << summary->totalOps << ",\n";
        os << "    \"loads\": " << summary->numLoads << ",\n";
        os << "    \"stores\": " << summary->numStores << "\n";
        os << "  }";
      }
    }
  });
  
  os << "\n]\n";
}