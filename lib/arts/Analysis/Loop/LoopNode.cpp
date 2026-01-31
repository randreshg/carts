///==========================================================================///
/// File: LoopNode.cpp
///
/// Implementation of LoopNode for SCF loop operations with integrated metadata.
///==========================================================================///

#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
using namespace mlir::arts;

static Value getWhileInductionVar(scf::WhileOp whileOp) {
  if (!whileOp)
    return Value();

  Block &before = whileOp.getBefore().front();
  if (before.getNumArguments() == 0)
    return Value();

  auto condition = dyn_cast<scf::ConditionOp>(before.getTerminator());
  if (!condition)
    return before.getArgument(0);

  Value cond = condition.getCondition();
  for (BlockArgument arg : before.getArguments()) {
    SmallVector<Value> bounds;
    collectWhileBounds(cond, arg, bounds);
    if (!bounds.empty())
      return arg;
  }

  return before.getArgument(0);
}

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
  else if (isa<arts::ForOp>(loopOp))
    os << " arts.for";
  else if (isa<scf::ParallelOp>(loopOp))
    os << " scf.parallel";
  else if (isa<scf::WhileOp>(loopOp))
    os << " scf.while";

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
      .Case<arts::ForOp>([](auto op) -> Value {
        if (op.getRegion().empty())
          return Value();
        Block &body = op.getRegion().front();
        return body.getNumArguments() == 0 ? Value() : body.getArgument(0);
      })
      .Case<scf::WhileOp>([](auto op) { return getWhileInductionVar(op); })
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

bool LoopNode::dependsOnInductionVarNormalized(Value v) {
  return dependsOnInductionVar(ValueUtils::stripNumericCasts(v));
}

static bool dependsOnLoopInitImpl(Value value, Value base, unsigned depth) {
  if (!value || !base || depth > 8)
    return false;
  if (value == base)
    return true;

  if (auto blockArg = value.dyn_cast<BlockArgument>()) {
    Operation *parentOp = blockArg.getOwner()->getParentOp();
    if (auto forOp = dyn_cast_or_null<scf::ForOp>(parentOp)) {
      if (blockArg == forOp.getInductionVar())
        return dependsOnLoopInitImpl(forOp.getLowerBound(), base, depth + 1);
    } else if (auto whileOp = dyn_cast_or_null<scf::WhileOp>(parentOp)) {
      unsigned idx = blockArg.getArgNumber();
      if (idx < whileOp.getInits().size())
        return dependsOnLoopInitImpl(whileOp.getInits()[idx], base, depth + 1);
    } else if (auto artsFor = dyn_cast_or_null<arts::ForOp>(parentOp)) {
      unsigned idx = blockArg.getArgNumber();
      auto lbs = artsFor.getLowerBound();
      if (idx < lbs.size())
        return dependsOnLoopInitImpl(lbs[idx], base, depth + 1);
    } else if (auto parallelOp = dyn_cast_or_null<scf::ParallelOp>(parentOp)) {
      auto ivs = parallelOp.getInductionVars();
      for (auto it : llvm::enumerate(ivs)) {
        if (blockArg == it.value()) {
          auto lbs = parallelOp.getLowerBound();
          if (it.index() < lbs.size())
            return dependsOnLoopInitImpl(lbs[it.index()], base, depth + 1);
          break;
        }
      }
    }
    return false;
  }

  Operation *op = value.getDefiningOp();
  if (!op)
    return false;

  if (!isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp,
           arith::DivUIOp, arith::RemSIOp, arith::RemUIOp, arith::IndexCastOp,
           arith::IndexCastUIOp, arith::ExtSIOp, arith::ExtUIOp,
           arith::TruncIOp>(op))
    return false;

  for (Value operand : op->getOperands())
    if (dependsOnLoopInitImpl(operand, base, depth + 1))
      return true;

  return false;
}

bool LoopNode::dependsOnLoopInit(Value value, Value base) {
  return dependsOnLoopInitImpl(value, base, 0);
}

bool LoopNode::dependsOnLoopInitNormalized(Value value, Value base) {
  Value vStripped = ValueUtils::stripNumericCasts(value);
  Value bStripped = ValueUtils::stripNumericCasts(base);
  return dependsOnLoopInitImpl(vStripped, bStripped, 0);
}

bool LoopNode::isValueLoopInvariant(Value v) {
  if (!loopOp)
    return false;
  v = ValueUtils::stripNumericCasts(v);

  llvm::SmallPtrSet<Value, 16> visited;
  std::function<bool(Value)> isInvariant = [&](Value val) -> bool {
    if (!val)
      return false;
    if (!visited.insert(val).second)
      return true;

    if (ValueUtils::isValueConstant(val))
      return true;

    if (auto blockArg = val.dyn_cast<BlockArgument>()) {
      Block *owner = blockArg.getOwner();
      if (!owner)
        return false;
      Operation *parentOp = owner->getParentOp();
      if (parentOp && (parentOp == loopOp || loopOp->isAncestor(parentOp)))
        return false;
      return true;
    }

    Operation *defOp = val.getDefiningOp();
    if (!defOp)
      return false;

    if (!loopOp->isAncestor(defOp)) {
      for (Value operand : defOp->getOperands())
        if (!isInvariant(operand))
          return false;
      return true;
    }

    if (!isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp,
             arith::DivUIOp, arith::IndexCastOp, arith::ExtSIOp, arith::ExtUIOp,
             arith::TruncIOp>(defOp))
      return false;

    for (Value operand : defOp->getOperands())
      if (!isInvariant(operand))
        return false;
    return true;
  };

  return isInvariant(v);
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
    return ValueUtils::getConstantValue(v);
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
  } else if (auto whileOp = dyn_cast<scf::WhileOp>(loopOp)) {
    Value iv = getInductionVar();
    auto arg = iv.dyn_cast<BlockArgument>();
    if (!arg)
      return std::nullopt;
    unsigned argIndex = arg.getArgNumber();
    if (argIndex >= whileOp.getInits().size())
      return std::nullopt;
    Value init = ValueUtils::stripNumericCasts(whileOp.getInits()[argIndex]);
    return ValueUtils::getConstantValue(init);
  }
  return std::nullopt;
}

std::optional<int64_t> LoopNode::getUpperBoundConstant() const {
  if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
    if (auto constOp =
            forOp.getUpperBound().getDefiningOp<arith::ConstantIndexOp>())
      return constOp.value();
  } else if (auto whileOp = dyn_cast<scf::WhileOp>(loopOp)) {
    Value iv = getInductionVar();
    auto arg = iv.dyn_cast<BlockArgument>();
    if (!arg)
      return std::nullopt;

    Block &before = whileOp.getBefore().front();
    auto condition = dyn_cast<scf::ConditionOp>(before.getTerminator());
    if (!condition)
      return std::nullopt;

    SmallVector<Value> bounds;
    collectWhileBounds(condition.getCondition(), arg, bounds);
    if (bounds.empty())
      return std::nullopt;

    std::optional<int64_t> minBound;
    for (Value bound : bounds) {
      Value stripped = ValueUtils::stripNumericCasts(bound);
      auto cst = ValueUtils::getConstantValue(stripped);
      if (!cst)
        return std::nullopt;
      if (!minBound || *cst < *minBound)
        minBound = *cst;
    }
    return minBound;
  }
  return std::nullopt;
}

int LoopNode::getNestingDepth() const {
  int depth = 0;
  for (Operation *parent = loopOp->getParentOp(); parent;
       parent = parent->getParentOp()) {
    if (isa<scf::ForOp, scf::WhileOp, scf::ParallelOp, affine::AffineForOp>(
            parent))
      ++depth;
  }
  return depth;
}
