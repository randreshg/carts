///==========================================================================///
/// File: RaiseToLinalg.cpp
///
/// Analyzes loop nests inside arts.for operations and computes linalg-style
/// structural information: indexing maps, iterator types, and pattern
/// classification.  Stamps the results as attributes on the arts.for op so
/// that PatternDiscovery (and downstream passes) can leverage structural
/// classification instead of manual access-pattern heuristics.
///
/// Recognized patterns:
///   - elementwise : all-parallel iterators, identity/permutation maps
///   - matmul      : triple-nested, (m,k)*(k,n)->(m,n), reduction on k
///   - stencil     : all-parallel with constant-offset read maps
///   - reduction   : at least one reduction iterator
///
///==========================================================================///

#include "arts/Dialect.h"
#define GEN_PASS_DEF_RAISETOLINALG
#include "arts/passes/Passes.h"
#include "arts/dialect/sde/Transforms/Passes.h.inc"
#include "arts/utils/OperationAttributes.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallVector.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(raise_to_linalg);

using namespace mlir;
using namespace mlir::arts;

namespace {

//===----------------------------------------------------------------------===//
// Loop nest collection
//===----------------------------------------------------------------------===//

struct LoopNestInfo {
  SmallVector<Value> ivs;
  Block *innermostBody = nullptr;
  arts::ForOp rootForOp;
};

/// Recursively collect a perfectly nested scf.for chain from a body block.
/// A block is "perfectly nested" if its only non-terminator op is a single
/// scf.for (with no iter_args — we skip reduction-carrying loops for now).
static bool collectInner(Block &body, LoopNestInfo &info) {
  SmallVector<Operation *> ops;
  for (auto &op : body) {
    if (op.hasTrait<OpTrait::IsTerminator>())
      continue;
    ops.push_back(&op);
  }

  // If the only op is a simple scf.for, descend into it.
  if (ops.size() == 1) {
    if (auto innerFor = dyn_cast<scf::ForOp>(ops[0])) {
      if (innerFor.getInitArgs().empty()) {
        info.ivs.push_back(innerFor.getInductionVar());
        return collectInner(*innerFor.getBody(), info);
      }
    }
  }

  // Otherwise this block IS the innermost body.
  info.innermostBody = &body;
  return true;
}

/// Collect the full perfectly-nested loop structure starting from an arts.for.
static bool collectPerfectNest(arts::ForOp forOp, LoopNestInfo &info) {
  info.rootForOp = forOp;

  // Only handle single-dimension arts.for for now.
  if (forOp.getLowerBound().size() != 1)
    return false;

  Region &region = forOp.getRegion();
  if (region.empty() || region.front().getNumArguments() < 1)
    return false;

  info.ivs.push_back(region.front().getArgument(0));
  return collectInner(region.front(), info);
}

//===----------------------------------------------------------------------===//
// Affine expression analysis
//===----------------------------------------------------------------------===//

/// Try to express @p value as an affine expression of @p ivs.
/// Returns nullopt when the value cannot be expressed as an affine function.
static std::optional<AffineExpr> tryGetAffineExpr(Value value,
                                                  ArrayRef<Value> ivs,
                                                  MLIRContext *ctx) {
  // Check if value is one of the induction variables.
  for (auto [idx, iv] : llvm::enumerate(ivs)) {
    if (value == iv)
      return getAffineDimExpr(idx, ctx);
  }

  auto *defOp = value.getDefiningOp();
  if (!defOp)
    return std::nullopt;

  // Constant.
  if (auto cst = dyn_cast<arith::ConstantOp>(defOp)) {
    if (auto intAttr = dyn_cast<IntegerAttr>(cst.getValue()))
      return getAffineConstantExpr(intAttr.getInt(), ctx);
    return std::nullopt;
  }

  // Addition.
  if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(addOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(addOp.getRhs(), ivs, ctx);
    if (lhs && rhs)
      return *lhs + *rhs;
  }

  // Subtraction.
  if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(subOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(subOp.getRhs(), ivs, ctx);
    if (lhs && rhs)
      return *lhs - *rhs;
  }

  // Multiplication (at least one side must be constant for affine).
  if (auto mulOp = dyn_cast<arith::MulIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(mulOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(mulOp.getRhs(), ivs, ctx);
    if (lhs && rhs) {
      if (isa<AffineConstantExpr>(*lhs) || isa<AffineConstantExpr>(*rhs))
        return *lhs * *rhs;
    }
  }

  // Index cast — pass through.
  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp))
    return tryGetAffineExpr(castOp.getIn(), ivs, ctx);

  return std::nullopt;
}

/// Build an AffineMap mapping loop IVs to a memref's access indices.
static std::optional<AffineMap>
tryBuildIndexingMap(OperandRange indices, ArrayRef<Value> ivs,
                   MLIRContext *ctx) {
  SmallVector<AffineExpr> exprs;
  for (Value idx : indices) {
    auto expr = tryGetAffineExpr(idx, ivs, ctx);
    if (!expr)
      return std::nullopt;
    exprs.push_back(*expr);
  }
  return AffineMap::get(/*dimCount=*/ivs.size(), /*symbolCount=*/0, exprs, ctx);
}

//===----------------------------------------------------------------------===//
// Memref access collection
//===----------------------------------------------------------------------===//

struct MemrefAccessEntry {
  Value memref;
  AffineMap indexingMap;
  bool isRead; // true = memref.load, false = memref.store
};

/// Collect all memref.load / memref.store ops in @p body and build their
/// indexing maps.  Returns false if any access cannot be expressed as an affine
/// function of @p ivs or if the body contains an op with unknown side effects.
static bool collectMemrefAccesses(Block &body, ArrayRef<Value> ivs,
                                  SmallVectorImpl<MemrefAccessEntry> &reads,
                                  SmallVectorImpl<MemrefAccessEntry> &writes,
                                  MLIRContext *ctx) {
  for (auto &op : body) {
    if (op.hasTrait<OpTrait::IsTerminator>())
      continue;

    if (auto loadOp = dyn_cast<memref::LoadOp>(&op)) {
      auto map = tryBuildIndexingMap(loadOp.getIndices(), ivs, ctx);
      if (!map)
        return false;
      reads.push_back({loadOp.getMemref(), *map, /*isRead=*/true});
      continue;
    }

    if (auto storeOp = dyn_cast<memref::StoreOp>(&op)) {
      auto map = tryBuildIndexingMap(storeOp.getIndices(), ivs, ctx);
      if (!map)
        return false;
      writes.push_back({storeOp.getMemref(), *map, /*isRead=*/false});
      continue;
    }

    // Any pure (side-effect-free) op is safe — arithmetic, math, index casts.
    if (isMemoryEffectFree(&op))
      continue;

    // Unknown op with potential side effects — bail.
    ARTS_DEBUG("  bail: unknown op in body: " << op.getName());
    return false;
  }

  return !reads.empty() || !writes.empty();
}

//===----------------------------------------------------------------------===//
// Iterator type determination
//===----------------------------------------------------------------------===//

/// Determine whether each loop dimension is "parallel" or "reduction".
/// A dimension is parallel if it appears in at least one output (write) map;
/// otherwise it is a reduction dimension.
static void computeIteratorTypes(unsigned numDims,
                                 ArrayRef<MemrefAccessEntry> writes,
                                 SmallVectorImpl<Attribute> &iterTypes,
                                 MLIRContext *ctx) {
  llvm::SmallBitVector dimsInOutputs(numDims, false);
  for (auto &entry : writes) {
    for (unsigned i = 0; i < entry.indexingMap.getNumResults(); ++i) {
      entry.indexingMap.getResult(i).walk([&](AffineExpr expr) {
        if (auto dimExpr = dyn_cast<AffineDimExpr>(expr))
          dimsInOutputs.set(dimExpr.getPosition());
      });
    }
  }

  auto parallel = StringAttr::get(ctx, AttrNames::Operation::Linalg::IterParallel);
  auto reduction = StringAttr::get(ctx, AttrNames::Operation::Linalg::IterReduction);
  for (unsigned d = 0; d < numDims; ++d)
    iterTypes.push_back(dimsInOutputs.test(d) ? parallel : reduction);
}

//===----------------------------------------------------------------------===//
// Pattern classification
//===----------------------------------------------------------------------===//

/// Return true if @p map contains any result with a constant additive offset
/// on a dimension (e.g. d0 + 1, d1 - 2).
static bool hasConstantOffsets(AffineMap map) {
  for (auto result : map.getResults()) {
    auto binOp = dyn_cast<AffineBinaryOpExpr>(result);
    if (!binOp || binOp.getKind() != AffineExprKind::Add)
      continue;
    bool lhsDim = isa<AffineDimExpr>(binOp.getLHS());
    bool rhsDim = isa<AffineDimExpr>(binOp.getRHS());
    bool lhsCst = isa<AffineConstantExpr>(binOp.getLHS());
    bool rhsCst = isa<AffineConstantExpr>(binOp.getRHS());
    if ((lhsDim && rhsCst) || (lhsCst && rhsDim))
      return true;
  }
  return false;
}

/// Classify the access pattern from collected reads, writes, and iterator
/// types.
static StringRef classifyPattern(ArrayRef<MemrefAccessEntry> reads,
                                 ArrayRef<MemrefAccessEntry> writes,
                                 ArrayRef<Attribute> iterTypes,
                                 unsigned numDims) {
  unsigned numParallel = 0, numReduction = 0;
  for (auto attr : iterTypes) {
    if (cast<StringAttr>(attr).getValue() ==
        AttrNames::Operation::Linalg::IterParallel)
      ++numParallel;
    else
      ++numReduction;
  }

  // All-parallel case: elementwise or stencil.
  if (numReduction == 0) {
    for (auto &entry : reads) {
      if (hasConstantOffsets(entry.indexingMap))
        return AttrNames::Operation::Linalg::PatternStencil;
    }
    return AttrNames::Operation::Linalg::PatternElementwise;
  }

  // 2 parallel + 1 reduction in 3D → potential matmul.
  if (numParallel == 2 && numReduction == 1 && numDims == 3 &&
      reads.size() >= 2 && writes.size() >= 1)
    return AttrNames::Operation::Linalg::PatternMatmul;

  return AttrNames::Operation::Linalg::PatternReduction;
}

//===----------------------------------------------------------------------===//
// Pass implementation
//===----------------------------------------------------------------------===//

struct RaiseToLinalgPass
    : public impl::RaiseToLinalgBase<RaiseToLinalgPass> {

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    unsigned analyzed = 0, classified = 0;

    module.walk([&](arts::ForOp forOp) {
      ++analyzed;

      // Skip if already classified.
      if (forOp->hasAttr(AttrNames::Operation::Linalg::Pattern))
        return;

      // 1. Collect the perfectly nested loop structure.
      LoopNestInfo nest;
      if (!collectPerfectNest(forOp, nest))
        return;
      if (!nest.innermostBody || nest.ivs.empty())
        return;

      unsigned numDims = nest.ivs.size();

      // 2. Collect memref accesses and build indexing maps.
      SmallVector<MemrefAccessEntry> reads, writes;
      if (!collectMemrefAccesses(*nest.innermostBody, nest.ivs, reads, writes,
                                 ctx))
        return;

      // Must have at least one write (output) for a complete computation.
      if (writes.empty())
        return;

      // 3. Compute iterator types.
      SmallVector<Attribute> iterTypes;
      computeIteratorTypes(numDims, writes, iterTypes, ctx);

      // 4. Build the combined indexing-map list: reads first, then writes.
      SmallVector<Attribute> indexingMapAttrs;
      for (auto &entry : reads)
        indexingMapAttrs.push_back(AffineMapAttr::get(entry.indexingMap));
      for (auto &entry : writes)
        indexingMapAttrs.push_back(AffineMapAttr::get(entry.indexingMap));

      int64_t numInputs = static_cast<int64_t>(reads.size());
      int64_t numOutputs = static_cast<int64_t>(writes.size());

      // 5. Classify.
      StringRef pattern =
          classifyPattern(reads, writes, iterTypes, numDims);

      // 6. Stamp attributes on the arts.for.
      auto i64Ty = IntegerType::get(ctx, 64);
      forOp->setAttr(AttrNames::Operation::Linalg::Pattern,
                      StringAttr::get(ctx, pattern));
      forOp->setAttr(AttrNames::Operation::Linalg::IteratorTypes,
                      ArrayAttr::get(ctx, iterTypes));
      forOp->setAttr(AttrNames::Operation::Linalg::IndexingMaps,
                      ArrayAttr::get(ctx, indexingMapAttrs));
      forOp->setAttr(AttrNames::Operation::Linalg::NumInputs,
                      IntegerAttr::get(i64Ty, numInputs));
      forOp->setAttr(AttrNames::Operation::Linalg::NumOutputs,
                      IntegerAttr::get(i64Ty, numOutputs));

      ARTS_DEBUG("classified arts.for as '" << pattern << "' with " << numDims
                 << " dims (" << numInputs << " ins, " << numOutputs
                 << " outs)");
      ++classified;
    });

    ARTS_INFO("RaiseToLinalg: analyzed " << analyzed << " loops, classified "
                                         << classified);
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::sde::createRaiseToLinalgPass() {
  return std::make_unique<RaiseToLinalgPass>();
}
