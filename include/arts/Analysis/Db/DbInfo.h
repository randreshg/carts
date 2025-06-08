///==========================================================================
/// File: DbInfo.h
///
/// This class provides comprehensive analysis of memory access
/// patterns.
///==========================================================================

#ifndef CARTS_ANALYSIS_DB_INFO_H
#define CARTS_ANALYSIS_DB_INFO_H

#include "arts/ArtsDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"
#include <cstdint>
#include <optional>
#include <string>

namespace mlir {
namespace arts {

class DbAnalysis;
class DbAccessNode;
class DbAllocNode;

///===----------------------------------------------------------------------===///
/// SymbolicExpr
///
/// Represents memory access expressions in canonical form for analysis that
/// - Normalizes equivalent expressions (e.g., i+5 vs 5+i)
/// - Enables systematic pattern recognition
/// - Supports both exact (Constant) and approximate (Affine/Unknown) analysis
///===----------------------------------------------------------------------===///
struct SymbolicExpr {
  enum class Kind {
    /// Compile-time known value: enables aggressive optimizations
    Constant,
    /// Linear expression: common case, good optimization potential
    Affine,
    /// Complex expression: conservative analysis, maintains safety
    Unknown
  } kind;

  /// Expression components
  int64_t constant;   /// for Constant
  Value base;         /// symbolic base for Affine
  int64_t multiplier; /// multiplier for Affine
  int64_t offset;     /// offset for Affine

  /// Default to Unknown - conservative and safe
  SymbolicExpr()
      : kind(Kind::Unknown), constant(0), base(nullptr), multiplier(0),
        offset(0) {}
  /// Constant constructor - enables exact analysis
  SymbolicExpr(int64_t c)
      : kind(Kind::Constant), constant(c), base(nullptr), multiplier(0),
        offset(0) {}
  /// Affine constructor: base * multiplier + offset - covers most loop patterns
  SymbolicExpr(Value b, int64_t m, int64_t o)
      : kind(Kind::Affine), constant(0), base(b), multiplier(m), offset(o) {}

  /// Convert to human-readable string
  std::string toString() const;
};

///===----------------------------------------------------------------------===///
/// ComplexExpr
/// Classifies and analyzes memory access patterns for optimization guidance.
///===----------------------------------------------------------------------===///
struct ComplexExpr {
  enum class Pattern {
    Constant,   /// Fixed offset: a[5]
    Sequential, /// Unit stride: a[i], a[i+1]
    Strided,    /// Fixed stride: a[2*i]
    Blocked,    /// Block access: a[block*64 + local_i]
    Indirect,   /// Indirect: a[index_array[i]]
    Complex     /// Unanalyzable
  } pattern;

  /// Core symbolic components
  SymbolicExpr baseExpr;   /// Base address expression
  SymbolicExpr strideExpr; /// For strided/sequential access
  SymbolicExpr blockSize;  /// For blocked access
  SymbolicExpr offsetExpr; /// Additional offset

  /// Conservative bounds
  /// Even for complex expressions, bounds are critical for:
  /// - Memory safety verification
  /// - Buffer overflow detection
  /// - Allocation size determination
  /// - Loop optimization legality checks
  SymbolicExpr conservativeMin, conservativeMax;
  bool hasValidBounds;

  /// Access properties - guide optimization strategy selection
  /// Does index always increase/decrease?
  bool isMonotonic;
  /// Adjacent elements accessed?
  bool hasSpatialReuse;
  /// Same elements re-accessed?
  bool hasTemporalReuse;
  /// Conservative estimate of the range size
  int64_t estimatedRangeSize;

  /// Raw expression for complex/unanalyzable cases
  Value rawExpr;

  /// Constructors
  ComplexExpr()
      : pattern(Pattern::Complex), hasValidBounds(false), isMonotonic(false),
        hasSpatialReuse(false), hasTemporalReuse(false), estimatedRangeSize(-1),
        rawExpr(nullptr) {}

  ComplexExpr(int64_t constant)
      : pattern(Pattern::Constant), baseExpr(constant), hasValidBounds(true),
        conservativeMin(constant), conservativeMax(constant), isMonotonic(true),
        hasSpatialReuse(false), hasTemporalReuse(true), estimatedRangeSize(1),
        rawExpr(nullptr) {}

  /// Analyze and classify a raw index expression
  static ComplexExpr analyze(Value index);

  /// Convert to human-readable string for debugging/printing
  std::string toString() const;

  /// Get pattern name as string
  std::string getPatternName() const;
};

///===----------------------------------------------------------------------===///
/// DimensionAnalysis
/// Aggregates access pattern analysis results for a single array dimension.
///
/// Multi-dimensional arrays often exhibit different access patterns in
/// different dimensions. For example, in a 2D matrix:
/// - Dimension 0 (rows): Sequential access for row-major traversal
/// - Dimension 1 (cols): Strided access for column operations
///===----------------------------------------------------------------------===///
struct DimensionAnalysis {
  /// Dominant access pattern for this dimension
  ComplexExpr overallPattern;

  /// Summary statistics
  /// Total accesses observed
  size_t numAccesses;
  /// All accesses follow same pattern?
  bool isUniformPattern;
  /// How confident we are (0.0-1.0)
  double patternConfidence;

  /// Constructor
  DimensionAnalysis()
      : numAccesses(0), isUniformPattern(false), patternConfidence(0.0) {}
};

///===----------------------------------------------------------------------===///
/// DbInfo
/// Comprehensive analysis of datablock allocation and access patterns.
///===----------------------------------------------------------------------===///
class DbInfo {
public:
  enum class AccessType { Read, Write, ReadWrite, Unknown };

protected:
  /// Core identification and relationships
  static unsigned nextGlobalId;
  unsigned id;
  std::string hierId;
  std::optional<DbInfo *> parent;

  Operation *op;
  bool isAllocFlag;
  DbAnalysis *analysis;
  std::optional<EdtOp> edtParent;
  Type elementType;

  AccessType accessType;
  SmallVector<memref::LoadOp, 4> loads;
  SmallVector<memref::StoreOp, 4> stores;

  struct {
    int64_t offset;
    int64_t size;
    int64_t stride;
    bool isStatic;
  } layout;

  ValueRange offsets, sizes, strides;

  SmallVector<DimensionAnalysis, 4> dimensionAnalysis;
  SmallVector<int64_t, 4> computedDimSizes;
  SmallVector<MemoryEffects::EffectInstance, 4> effects;

public:
  DbInfo(Operation *op, bool isAlloc, DbAnalysis *analysis);
  virtual ~DbInfo() = default;

  unsigned getId() const { return id; }
  std::string getHierId() const { return hierId; }
  void setHierId(const std::string &newId) { hierId = newId; }

  bool isAlloc() const { return isAllocFlag; }
  virtual bool isAccess() const { return !isAllocFlag; }
  Type getElementType() const { return elementType; }
  uint64_t getElementTypeSize() const;

  DbInfo *getParent() const;
  EdtOp getEdtParent() const;
  DbAllocNode *getParentAlloc();
  DbAccessNode *getParentAccess();

  Operation *getOp() const { return op; }
  template <typename T> T getOp() const { return cast<T>(op); }
  Value getResult() const { return op->getResult(0); }
  Value getPtr() const { return op->getOperand(0); }
  SmallVector<MemoryEffects::EffectInstance, 4> &getEffects() {
    return effects;
  }
  Type getResultType() const { return op->getResult(0).getType(); }

  bool isWriter();
  bool isReader();
  bool isOnlyReader();
  bool isOnlyWriter();

  const SmallVector<DimensionAnalysis, 4> &getDimensionAnalysis() const {
    return dimensionAnalysis;
  }
  const SmallVector<int64_t, 4> &getComputedDimSizes() const {
    return computedDimSizes;
  }

  virtual void print(llvm::raw_ostream &os) const;

protected:
  void analyze();
  void collectUses();
  void setAccessType();
  void setMemoryLayout();

  bool computeRegion();

  void analyzeIndexValue(mlir::Value index, llvm::SetVector<ValueOrInt> &mins,
                         llvm::SetVector<ValueOrInt> &maxs);
};

} // namespace arts
} // namespace mlir
#endif /// CARTS_ANALYSIS_DB_INFO_H