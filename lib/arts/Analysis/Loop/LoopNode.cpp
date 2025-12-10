///==========================================================================///
/// File: LoopNode.cpp
///
/// Implementation of LoopNode for SCF loop operations with integrated metadata.
///==========================================================================///

#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
using namespace mlir::arts;

LoopNode::LoopNode(Operation *loopOp, LoopAnalysis *loopAnalysis)
    : NodeBase(), LoopMetadata(loopOp), loopOp(loopOp) {
  bool hasMetadata = importFromOp();
  if (!hasMetadata && loopAnalysis) {
    auto &analysisManager = loopAnalysis->getAnalysisManager();
    auto &metadataManager = analysisManager.getMetadataManager();
    if (metadataManager.ensureLoopMetadata(loopOp))
      importFromOp();
  }
  hierId = "loop@" + std::to_string(reinterpret_cast<uintptr_t>(loopOp));
}

void LoopNode::print(llvm::raw_ostream &os) const {
  os << "LoopNode(" << hierId << ")";

  if (isa<scf::ForOp>(loopOp))
    os << " scf.for";
  else if (isa<scf::ParallelOp>(loopOp))
    os << " scf.parallel";

  if (tripCount.has_value())
    os << " trip=" << tripCount.value();
  if (nestingLevel.has_value())
    os << " depth=" << nestingLevel.value();
  os << " parallel=" << (potentiallyParallel ? "yes" : "no");
  os << "\n";
}

//===----------------------------------------------------------------------===//
// Induction Variable Analysis Implementation
//===----------------------------------------------------------------------===//

Value LoopNode::getInductionVar() const {
  return llvm::TypeSwitch<Operation *, Value>(loopOp)
      .Case<scf::ForOp>([](auto op) { return op.getInductionVar(); })
      .Case<scf::ParallelOp>([](auto op) -> Value {
        return op.getInductionVars().empty() ? Value()
                                             : op.getInductionVars()[0];
      })
      .Default([](Operation *) { return Value(); });
}

bool LoopNode::dependsOnInductionVar(Value v) {
  /// Check cache first
  auto it = ivDependencyCache.find(v);
  if (it != ivDependencyCache.end())
    return it->second;

  Value iv = getInductionVar();
  if (!iv) {
    ivDependencyCache[v] = false;
    return false;
  }

  /// Direct match
  if (v == iv) {
    ivDependencyCache[v] = true;
    return true;
  }

  /// Walk def-use chain
  Operation *defOp = v.getDefiningOp();
  if (!defOp) {
    ivDependencyCache[v] = false;
    return false;
  }

  /// Check operands recursively
  bool depends = llvm::any_of(defOp->getOperands(), [this](Value operand) {
    return dependsOnInductionVar(operand);
  });

  ivDependencyCache[v] = depends;
  return depends;
}

LoopNode::IVExpr LoopNode::analyzeIndexExpr(Value index) {
  IVExpr result;
  Value iv = getInductionVar();

  if (!iv || !dependsOnInductionVar(index))
    return result;

  result.dependsOnIV = true;

  /// Direct IV use: offset=0, multiplier=1
  if (index == iv) {
    result.offset = 0;
    result.multiplier = 1;
    return result;
  }

  Operation *defOp = index.getDefiningOp();
  if (!defOp)
    return result;

  /// Helper to get constant value from arith ops
  auto getConstantValue = [](Value v) -> std::optional<int64_t> {
    if (auto constOp = v.getDefiningOp<arith::ConstantIndexOp>())
      return constOp.value();
    if (auto constOp = v.getDefiningOp<arith::ConstantIntOp>())
      return constOp.value();
    return std::nullopt;
  };

  /// Handle arith::AddIOp: iv + constant or constant + iv
  if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
    Value lhs = addOp.getLhs();
    Value rhs = addOp.getRhs();

    /// Check if one operand is the IV and the other is constant
    if (lhs == iv || dependsOnInductionVar(lhs)) {
      /// Recursively analyze lhs if it's derived from IV
      if (lhs == iv) {
        if (auto constVal = getConstantValue(rhs)) {
          result.offset = *constVal;
          result.multiplier = 1;
        }
      }
    } else if (rhs == iv || dependsOnInductionVar(rhs)) {
      if (rhs == iv) {
        if (auto constVal = getConstantValue(lhs)) {
          result.offset = *constVal;
          result.multiplier = 1;
        }
      }
    }
    return result;
  }

  /// Handle arith::SubIOp: iv - constant
  if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
    Value lhs = subOp.getLhs();
    Value rhs = subOp.getRhs();

    if (lhs == iv) {
      if (auto constVal = getConstantValue(rhs)) {
        result.offset = -(*constVal);
        result.multiplier = 1;
      }
    }
    return result;
  }

  /// Handle arith::MulIOp: iv * constant or constant * iv
  if (auto mulOp = dyn_cast<arith::MulIOp>(defOp)) {
    Value lhs = mulOp.getLhs();
    Value rhs = mulOp.getRhs();

    if (lhs == iv) {
      if (auto constVal = getConstantValue(rhs)) {
        result.offset = 0;
        result.multiplier = *constVal;
      }
    } else if (rhs == iv) {
      if (auto constVal = getConstantValue(lhs)) {
        result.offset = 0;
        result.multiplier = *constVal;
      }
    }
    return result;
  }
  return result;
}

std::optional<int64_t> LoopNode::getLowerBoundConstant() const {
  if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
    if (auto constOp =
            forOp.getLowerBound().getDefiningOp<arith::ConstantIndexOp>())
      return constOp.value();
  }
  return std::nullopt;
}

std::optional<int64_t> LoopNode::getUpperBoundConstant() const {
  if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
    if (auto constOp =
            forOp.getUpperBound().getDefiningOp<arith::ConstantIndexOp>())
      return constOp.value();
  }
  return std::nullopt;
}
