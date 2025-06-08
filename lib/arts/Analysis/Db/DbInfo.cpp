///==========================================================================
/// File: DbInfo.cpp
///
/// Base implementation of DbInfo class providing common functionality
/// for datablock allocation and access nodes.
///==========================================================================

#include "arts/Analysis/Db/DbInfo.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/Graph/DbNode.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "db-analysis"

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// ValueOrInt
///===----------------------------------------------------------------------===///

namespace llvm {
template <> struct DenseMapInfo<ValueOrInt> {
  static ValueOrInt getEmptyKey() {
    return ValueOrInt(::llvm::DenseMapInfo<int64_t>::getEmptyKey());
  }
  static ValueOrInt getTombstoneKey() {
    return ValueOrInt(::llvm::DenseMapInfo<int64_t>::getTombstoneKey());
  }
  static unsigned getHashValue(const ValueOrInt &vai) {
    if (vai.isValue) {
      return ::llvm::DenseMapInfo<mlir::Value>::getHashValue(vai.v_val);
    } else {
      return ::llvm::hash_value(vai.i_val);
    }
  }
  static bool isEqual(const ValueOrInt &L, const ValueOrInt &R) {
    if (L.isValue != R.isValue)
      return false;
    if (L.isValue)
      return ::llvm::DenseMapInfo<mlir::Value>::isEqual(L.v_val, R.v_val);
    return L.i_val == R.i_val;
  }
};

template <> struct DenseMapInfo<std::pair<mlir::Value, int64_t>> {
  using Pair = std::pair<mlir::Value, int64_t>;
  static inline Pair getEmptyKey() {
    return {DenseMapInfo<mlir::Value>::getEmptyKey(), 0};
  }
  static inline Pair getTombstoneKey() {
    return {DenseMapInfo<mlir::Value>::getTombstoneKey(), 0};
  }
  static unsigned getHashValue(const Pair &p) {
    return llvm::hash_combine(DenseMapInfo<mlir::Value>::getHashValue(p.first),
                              llvm::hash_value(p.second));
  }
  static bool isEqual(const Pair &LHS, const Pair &RHS) { return LHS == RHS; }
};
} // namespace llvm

///===----------------------------------------------------------------------===///
/// DbInfo Analysis Implementation
///===----------------------------------------------------------------------===///

namespace {
/// Helper to decompose an index value into a symbolic base, a constant
/// multiplier, and a constant offset.
/// Returns {base, multiplier, offset}
std::tuple<Value, int64_t, int64_t> decomposeIndex(Value index) {
  if (!index)
    return {nullptr, 0, 0};

  /// Case 1: Constant
  if (auto cstOp = index.getDefiningOp<arith::ConstantIndexOp>()) {
    return {nullptr, 0, cstOp.value()};
  }

  Operation *op = index.getDefiningOp();
  /// It's a block argument or function argument, treat as a simple base.
  if (!op)
    return {index, 1, 0};

  /// Case 2: Addition
  if (auto addOp = dyn_cast<arith::AddIOp>(op)) {
    auto [lhs_base, lhs_mul, lhs_off] = decomposeIndex(addOp.getLhs());
    auto [rhs_base, rhs_mul, rhs_off] = decomposeIndex(addOp.getRhs());

    /// If one side is purely constant, add its offset to the other side.
    if (!lhs_base)
      return {rhs_base, rhs_mul, lhs_off + rhs_off};
    if (!rhs_base)
      return {lhs_base, lhs_mul, lhs_off + rhs_off};

    /// Two different symbolic bases, cannot simplify further.
    return {index, 1, 0};
  }

  /// Case 3: Subtraction
  if (auto subOp = dyn_cast<arith::SubIOp>(op)) {
    auto [lhs_base, lhs_mul, lhs_off] = decomposeIndex(subOp.getLhs());
    auto [rhs_base, rhs_mul, rhs_off] = decomposeIndex(subOp.getRhs());

    /// If RHS is purely constant, subtract its offset.
    if (!rhs_base)
      return {lhs_base, lhs_mul, lhs_off - rhs_off};

    /// If LHS is purely constant, negate the RHS.
    if (!lhs_base)
      return {rhs_base, -rhs_mul, lhs_off - rhs_off};

    /// Two different symbolic bases, cannot simplify further.
    return {index, 1, 0};
  }

  /// Case 4: Multiplication
  if (auto mulOp = dyn_cast<arith::MulIOp>(op)) {
    auto [lhs_base, lhs_mul, lhs_off] = decomposeIndex(mulOp.getLhs());
    auto [rhs_base, rhs_mul, rhs_off] = decomposeIndex(mulOp.getRhs());

    /// Only handle multiplication by a constant.
    if (!lhs_base)
      return {rhs_base, rhs_mul * lhs_off, rhs_off * lhs_off};

    if (!rhs_base)
      return {lhs_base, lhs_mul * rhs_off, lhs_off * rhs_off};

    /// Multiplication of two symbolic bases, cannot simplify.
    return {index, 1, 0};
  }

  /// Case 5: Shift Left (Multiplication by 2^C)
  if (auto shliOp = dyn_cast<arith::ShLIOp>(op)) {
    auto [lhs_base, lhs_mul, lhs_off] = decomposeIndex(shliOp.getLhs());
    if (auto cst = shliOp.getRhs().getDefiningOp<arith::ConstantIndexOp>()) {
      int64_t multiplier = 1LL << cst.value();
      return {lhs_base, lhs_mul * multiplier, lhs_off * multiplier};
    }
    /// Shifting by a symbolic amount is not supported.
    return {index, 1, 0};
  }

  /// Default case for unknown ops or unhandled patterns (e.g., divi).
  return {index, 1, 0};
}
} // namespace

///===----------------------------------------------------------------------===///
/// DbInfo Core Implementation
///===----------------------------------------------------------------------===///

// Static member initialization
unsigned DbInfo::nextGlobalId = 0;

DbInfo::DbInfo(Operation *op, bool isAlloc, DbAnalysis *analysis)
    : op(op), isAllocFlag(isAlloc), analysis(analysis) {
  this->id = nextGlobalId++;
  this->hierId = "";
  analyze();
}

void DbInfo::analyze() {
  if (auto result = op->getResult(0)) {
    if (auto memrefType = result.getType().dyn_cast<MemRefType>()) {
      elementType = memrefType.getElementType();
    }
  }

  if (auto parentOp = op->getParentOfType<arts::EdtOp>())
    edtParent = parentOp;

  collectUses();
  setAccessType();
  setMemoryLayout();

  if (isAllocFlag) {
    if (auto memEff = dyn_cast<MemoryEffectOpInterface>(op))
      memEff.getEffects(effects);
  } else {
    computeRegion();
  }
}

uint64_t DbInfo::getElementTypeSize() const {
  return elementType.getIntOrFloatBitWidth();
}

DbInfo *DbInfo::getParent() const { return parent.value_or(nullptr); }

EdtOp DbInfo::getEdtParent() const { return edtParent.value_or(nullptr); }

DbAllocNode *DbInfo::getParentAlloc() {
  return dyn_cast<DbAllocNode>(getParent());
}

DbAccessNode *DbInfo::getParentAccess() {
  return dyn_cast<DbAccessNode>(getParent());
}

bool DbInfo::isWriter() {
  return accessType == AccessType::Write || accessType == AccessType::ReadWrite;
}

bool DbInfo::isReader() {
  return accessType == AccessType::Read || accessType == AccessType::ReadWrite;
}

bool DbInfo::isOnlyReader() { return isReader() && !isWriter(); }

bool DbInfo::isOnlyWriter() { return isWriter() && !isReader(); }

void DbInfo::analyzeIndexValue(mlir::Value index,
                               ::llvm::SetVector<ValueOrInt> &mins,
                               ::llvm::SetVector<ValueOrInt> &maxs) {
  if (!index)
    return;

  /// Basic analysis - try to extract constants
  IntegerAttr constAttr;
  if (matchPattern(index, m_Constant(&constAttr))) {
    int64_t value = constAttr.getInt();
    mins.insert(ValueOrInt(value));
    maxs.insert(ValueOrInt(value));
    return;
  }

  /// For block arguments (like loop induction variables)
  if (auto bbArg = index.dyn_cast<BlockArgument>()) {
    Operation *parentOp = bbArg.getOwner()->getParentOp();
    if (auto forOp = dyn_cast<scf::ForOp>(parentOp)) {
      if (bbArg == forOp.getInductionVar()) {
        /// Try to get loop bounds
        IntegerAttr lowerAttr, upperAttr;
        if (matchPattern(forOp.getLowerBound(), m_Constant(&lowerAttr))) {
          mins.insert(ValueOrInt(lowerAttr.getInt()));
        }
        if (matchPattern(forOp.getUpperBound(), m_Constant(&upperAttr))) {
          maxs.insert(
              ValueOrInt(upperAttr.getInt() - 1)); /// Exclusive upper bound
        }
        return;
      }
    }
  }

  /// Default: add the value itself (symbolic)
  mins.insert(ValueOrInt(index));
  maxs.insert(ValueOrInt(index));
}

bool DbInfo::computeRegion() {
  if (loads.empty() && stores.empty()) {
    LLVM_DEBUG(llvm::dbgs()
               << "DB #" << hierId << " (" << op->getLoc()
               << "): No loads/stores found, skipping region computation.\n");
    return false;
  }

  auto rank = sizes.size();
  if (rank == 0) {
    LLVM_DEBUG(llvm::dbgs()
               << "DB #" << hierId << " (" << op->getLoc()
               << "): Detected 0-rank access (scalar), region is trivial.\n");
    return true;
  }

  /// Initialize enhanced per-dimension analysis
  dimensionAnalysis.assign(rank, DimensionAnalysis());
  computedDimSizes.assign(rank, -1);

  SmallVector<::llvm::SetVector<ValueOrInt>> allDimMins(rank);
  SmallVector<::llvm::SetVector<ValueOrInt>> allDimMaxs(rank);

  auto processUser = [&](Operation *user) {
    ValueRange indices;
    if (auto ld = dyn_cast<memref::LoadOp>(user))
      indices = ld.getIndices();
    else if (auto st = cast<memref::StoreOp>(user))
      indices = st.getIndices();

    if (indices.empty())
      return;

    assert(indices.size() == rank &&
           "Number of indices must match the rank of the datablock");

    for (unsigned d = 0; d < rank; ++d) {
      /// Analyze each index expression using enhanced pattern analysis
      ComplexExpr expr = ComplexExpr::analyze(indices[d]);
      dimensionAnalysis[d].numAccesses++;

      /// Store for pattern combination analysis (simplified for now)
      if (dimensionAnalysis[d].overallPattern.pattern ==
          ComplexExpr::Pattern::Complex) {
        dimensionAnalysis[d].overallPattern = expr;
      }

      analyzeIndexValue(indices[d], allDimMins[d], allDimMaxs[d]);
    }
  };

  for (auto load : loads)
    processUser(load);
  for (auto store : stores)
    processUser(store);

  for (unsigned d = 0; d < rank; ++d) {
    auto &dimAnalysis = dimensionAnalysis[d];
    auto &minSet = allDimMins[d];
    auto &maxSet = allDimMaxs[d];

    bool allConstant = true;
    Value commonBase = nullptr;
    int64_t commonMultiplier = 1;
    SmallVector<int64_t> offsets;

    for (const auto &voi : minSet) {
      auto [base, mul, off] =
          voi.isValue ? decomposeIndex(voi.v_val) : decomposeIndex(nullptr);
      if (!voi.isValue)
        off += voi.i_val;

      if (base)
        allConstant = false;
      if (commonBase == nullptr && base != nullptr) {
        commonBase = base;
        commonMultiplier = mul;
      }
      if (base != nullptr && (commonBase != base || commonMultiplier != mul)) {
        /// Multiple different symbolic bases, analysis is too complex.
        commonBase = nullptr;
        break;
      }
      offsets.push_back(off);
    }
    if (!commonBase) /// If we broke out, check maxSet too.
    {
      for (const auto &voi : maxSet) {
        auto [base, mul, off] =
            voi.isValue ? decomposeIndex(voi.v_val) : decomposeIndex(nullptr);
        if (!voi.isValue)
          off += voi.i_val;

        if (base)
          allConstant = false;
        if (commonBase == nullptr && base != nullptr) {
          commonBase = base;
          commonMultiplier = mul;
        }
        if (base != nullptr &&
            (commonBase != base || commonMultiplier != mul)) {
          commonBase = nullptr;
          break;
        }
        offsets.push_back(off);
      }
    }

    if (allConstant) {
      int64_t minVal = *std::min_element(offsets.begin(), offsets.end());
      int64_t maxVal = *std::max_element(offsets.begin(), offsets.end());
      dimAnalysis.overallPattern.conservativeMin = SymbolicExpr(minVal);
      dimAnalysis.overallPattern.conservativeMax = SymbolicExpr(maxVal);
      dimAnalysis.overallPattern.hasValidBounds = true;
      computedDimSizes[d] = maxVal - minVal + 1;
      dimAnalysis.isUniformPattern = true;
      dimAnalysis.patternConfidence = 1.0;
    } else if (commonBase) {
      int64_t minOff = *std::min_element(offsets.begin(), offsets.end());
      int64_t maxOff = *std::max_element(offsets.begin(), offsets.end());
      computedDimSizes[d] = maxOff - minOff + 1;
      /// High confidence for affine patterns
      dimAnalysis.patternConfidence = 0.8;

      /// If the base is a loop IV, we can find the symbolic bounds.
      if (auto bbArg = commonBase.dyn_cast<BlockArgument>()) {
        if (auto forOp =
                dyn_cast<scf::ForOp>(bbArg.getOwner()->getParentOp())) {
          if (bbArg == forOp.getInductionVar()) {
            /// Symbolic bounds are `loop_bound * multiplier + offset`.
            dimAnalysis.overallPattern.hasValidBounds = true;
          }
        }
      }
    } else {
      /// Complex pattern - lower confidence
      dimAnalysis.patternConfidence = 0.3;
    }
  }

  return true;
}

void DbInfo::collectUses() {
  for (auto result : op->getResults()) {
    for (auto *user : result.getUsers()) {
      /// Collect loads and stores
      if (auto loadOp = dyn_cast<memref::LoadOp>(user))
        loads.push_back(loadOp);
      else if (auto storeOp = dyn_cast<memref::StoreOp>(user))
        stores.push_back(storeOp);
    }
  }
}

void DbInfo::setAccessType() {
  bool hasReads = !loads.empty();
  bool hasWrites = !stores.empty();

  if (hasReads && hasWrites)
    accessType = AccessType::ReadWrite;
  else if (hasReads)
    accessType = AccessType::Read;
  else if (hasWrites)
    accessType = AccessType::Write;
  else
    accessType = AccessType::Unknown;
}

void DbInfo::setMemoryLayout() {
  if (isAllocFlag) {
    /// For allocation nodes, the layout is the full buffer
    layout.offset = 0;
    layout.stride = 1;
    auto memrefType = getResultType().cast<MemRefType>();
    if (memrefType.hasStaticShape()) {
      uint64_t elementSize = getElementTypeSize() / 8;
      if (elementSize == 0)
        elementSize = 1;
      layout.size = memrefType.getNumElements() * elementSize;
      layout.isStatic = true;
    } else {
      layout.size = -1;
      layout.isStatic = false;
    }
    return;
  }

  /// For access nodes, extract layout from the operation
  if (auto subviewOp = dyn_cast<memref::SubViewOp>(op)) {
    auto getConstant = [&](Value v) -> std::optional<int64_t> {
      if (auto c = v.getDefiningOp<arith::ConstantOp>()) {
        if (auto i = c.getValue().dyn_cast<IntegerAttr>()) {
          return i.getInt();
        }
      }
      return std::nullopt;
    };

    offsets = subviewOp.getOffsets();
    sizes = subviewOp.getSizes();
    strides = subviewOp.getStrides();

    bool allStatic = true;
    for (Value v : offsets)
      if (!getConstant(v))
        allStatic = false;
    for (Value v : sizes)
      if (!getConstant(v))
        allStatic = false;
    for (Value v : strides)
      if (!getConstant(v))
        allStatic = false;

    /// Compute linearized offset and size
    auto sourceMemRefType = subviewOp.getSource().getType().cast<MemRefType>();
    int64_t sourceOffset;
    SmallVector<int64_t> sourceStrides;
    if (succeeded(getStridesAndOffset(sourceMemRefType, sourceStrides,
                                      sourceOffset))) {
      int64_t linearizedOffset = sourceOffset;
      bool offsetStatic = true;
      for (auto it : llvm::zip(offsets, sourceStrides)) {
        if (auto cst = getConstant(std::get<0>(it))) {
          linearizedOffset += (*cst) * std::get<1>(it);
        } else {
          offsetStatic = false;
          break;
        }
      }
      if (offsetStatic)
        layout.offset = linearizedOffset;
      else
        allStatic = false;
    } else {
      allStatic = false;
    }

    auto subviewType = subviewOp.getType().cast<MemRefType>();
    if (subviewType.hasStaticShape()) {
      uint64_t elementSize = getElementTypeSize() / 8;
      if (elementSize == 0)
        elementSize = 1;
      layout.size = subviewType.getNumElements() * elementSize;
    } else {
      allStatic = false;
      layout.size = -1;
    }
    layout.isStatic = allStatic;
  }
}

// Default print implementation
void DbInfo::print(llvm::raw_ostream &os) const {
  if (isAllocFlag) {
    os << " [Allocation]";
  } else {
    os << " [Access]";
  }
  os << "\n";

  if (!loads.empty()) {
    os << "  Loads: " << loads.size() << "\n";
  }
  if (!stores.empty()) {
    os << "  Stores: " << stores.size() << "\n";
  }
  for (unsigned i = 0; i < computedDimSizes.size(); ++i) {
    if (computedDimSizes[i] != -1) {
      os << "  Computed Size[" << i << "]: " << computedDimSizes[i] << "\n";
    } else {
      /// Print enhanced access pattern analysis
      if (i < dimensionAnalysis.size()) {
        const auto &analysis = dimensionAnalysis[i];
        os << "  Pattern[" << i << "]: " << analysis.overallPattern.toString()
           << " (confidence: " << analysis.patternConfidence
           << ", accesses: " << analysis.numAccesses << ")\n";
      }
    }
  }
}

///===----------------------------------------------------------------------===///
/// SymbolicExpr Implementation
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {

std::string SymbolicExpr::toString() const {
  std::string s;
  llvm::raw_string_ostream os(s);
  switch (kind) {
  case Kind::Constant:
    os << constant;
    break;
  case Kind::Affine:
    os << base << "*" << multiplier;
    if (offset >= 0)
      os << "+" << offset;
    else
      os << offset;
    break;
  case Kind::Unknown:
    os << "Unknown";
    break;
  }
  return os.str();
}

} // namespace arts
} // namespace mlir

///===----------------------------------------------------------------------===///
/// ComplexExpr Implementation
///===----------------------------------------------------------------------===///

ComplexExpr ComplexExpr::analyze(Value index) {
  if (!index)
    return ComplexExpr();

  /// Try to decompose using existing logic first
  auto [base, mul, off] = decomposeIndex(index);

  ComplexExpr result;
  result.rawExpr = index;

  if (!base) {
    /// Pure constant
    result.pattern = Pattern::Constant;
    result.baseExpr = SymbolicExpr(off);
    result.conservativeMin = SymbolicExpr(off);
    result.conservativeMax = SymbolicExpr(off);
    result.hasValidBounds = true;
    result.isMonotonic = true;
    result.hasTemporalReuse = true;
    result.estimatedRangeSize = 1;
    return result;
  }

  /// Check for sequential pattern (multiplier = 1)
  if (mul == 1) {
    result.pattern = Pattern::Sequential;
    result.baseExpr = SymbolicExpr(base, 1, 0);
    result.offsetExpr = SymbolicExpr(off);
    result.strideExpr = SymbolicExpr(1);
    result.isMonotonic = true;
    result.hasSpatialReuse = true;

    /// Try to infer bounds if base is a loop IV
    if (auto bbArg = base.dyn_cast<BlockArgument>()) {
      if (auto forOp = dyn_cast<scf::ForOp>(bbArg.getOwner()->getParentOp())) {
        if (bbArg == forOp.getInductionVar()) {
          /// We have loop bounds - can set conservative bounds
          result.hasValidBounds = true;
          /// Conservative: assume loop runs from lb to ub-1
          /// Actual bounds would be: lb + off to (ub-1) + off
        }
      }
    }
    return result;
  }

  /// Check for strided pattern (multiplier != 1)
  if (mul != 0) {
    result.pattern = Pattern::Strided;
    result.baseExpr = SymbolicExpr(base, 1, 0);
    result.strideExpr = SymbolicExpr(mul);
    result.offsetExpr = SymbolicExpr(off);
    result.isMonotonic = (mul > 0);
    /// Heuristic for spatial locality
    result.hasSpatialReuse = (std::abs(mul) <= 4);

    /// Try to infer bounds for strided access
    if (auto bbArg = base.dyn_cast<BlockArgument>()) {
      if (auto forOp = dyn_cast<scf::ForOp>(bbArg.getOwner()->getParentOp())) {
        if (bbArg == forOp.getInductionVar()) {
          result.hasValidBounds = true;
          /// Conservative bounds for strided access
        }
      }
    }
    return result;
  }

  /// Complex/unanalyzable pattern
  result.pattern = Pattern::Complex;
  result.hasValidBounds = false;
  result.estimatedRangeSize = -1;
  return result;
}

std::string ComplexExpr::toString() const {
  std::string s;
  llvm::raw_string_ostream os(s);

  os << getPatternName() << ": ";

  switch (pattern) {
  case Pattern::Constant:
    os << baseExpr.toString();
    break;
  case Pattern::Sequential:
    os << baseExpr.toString() << "+" << offsetExpr.toString();
    break;
  case Pattern::Strided:
    os << baseExpr.toString() << "*" << strideExpr.toString() << "+"
       << offsetExpr.toString();
    break;
  case Pattern::Blocked:
    os << "block=" << blockSize.toString() << ", local=" << baseExpr.toString();
    break;
  case Pattern::Indirect:
    os << "indirect[" << baseExpr.toString() << "]";
    break;
  case Pattern::Complex:
    os << "complex(" << rawExpr << ")";
    break;
  }

  if (hasValidBounds) {
    os << " bounds=[" << conservativeMin.toString() << ","
       << conservativeMax.toString() << "]";
  }

  if (estimatedRangeSize > 0)
    os << " size=" << estimatedRangeSize;

  return os.str();
}

std::string ComplexExpr::getPatternName() const {
  switch (pattern) {
  case Pattern::Constant:
    return "Constant";
  case Pattern::Sequential:
    return "Sequential";
  case Pattern::Strided:
    return "Strided";
  case Pattern::Blocked:
    return "Blocked";
  case Pattern::Indirect:
    return "Indirect";
  case Pattern::Complex:
    return "Complex";
  }
  return "Unknown";
}
