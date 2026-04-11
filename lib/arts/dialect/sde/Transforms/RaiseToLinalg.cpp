///==========================================================================///
/// File: RaiseToLinalg.cpp
///
/// Analyzes loop nests inside sde.su_iterate operations and computes
/// linalg-style structural classification: indexing maps, iterator types,
/// and pattern classification.
///
/// The pass stamps an "arts.linalg.classification" attribute on the
/// sde.su_iterate op. ConvertSdeToArts reads this classification and stamps
/// the corresponding ARTS pattern contract on the arts.for it creates.
///
/// Recognized patterns:
///   - elementwise : all-parallel iterators, identity/permutation maps
///   - matmul      : triple-nested, (m,k)*(k,n)->(m,n), reduction on k
///   - stencil     : all-parallel with constant-offset read maps
///   - reduction   : at least one reduction iterator
///
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_RAISETOLINALG
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"

#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
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
  sde::SdeSuIterateOp rootIterOp;
};

/// Recursively collect a perfectly nested scf.for chain from a body block.
static bool collectInner(Block &body, LoopNestInfo &info) {
  SmallVector<Operation *> ops;
  for (auto &op : body) {
    if (op.hasTrait<OpTrait::IsTerminator>())
      continue;
    if (isa<sde::SdeYieldOp>(op))
      continue;
    ops.push_back(&op);
  }

  if (ops.size() == 1) {
    if (auto innerFor = dyn_cast<scf::ForOp>(ops[0])) {
      if (innerFor.getInitArgs().empty()) {
        info.ivs.push_back(innerFor.getInductionVar());
        return collectInner(*innerFor.getBody(), info);
      }
    }
  }

  info.innermostBody = &body;
  return true;
}

/// Collect the full perfectly-nested loop structure from an sde.su_iterate.
static bool collectPerfectNest(sde::SdeSuIterateOp iterOp,
                               LoopNestInfo &info) {
  info.rootIterOp = iterOp;

  if (iterOp.getLowerBounds().size() != 1)
    return false;

  Region &region = iterOp.getBody();
  if (region.empty() || region.front().getNumArguments() < 1)
    return false;

  info.ivs.push_back(region.front().getArgument(0));
  return collectInner(region.front(), info);
}

//===----------------------------------------------------------------------===//
// Affine expression analysis
//===----------------------------------------------------------------------===//

static std::optional<AffineExpr>
tryGetAffineExpr(Value value, ArrayRef<Value> ivs, MLIRContext *ctx) {
  for (auto [idx, iv] : llvm::enumerate(ivs)) {
    if (value == iv)
      return getAffineDimExpr(idx, ctx);
  }

  auto *defOp = value.getDefiningOp();
  if (!defOp)
    return std::nullopt;

  if (auto cst = dyn_cast<arith::ConstantOp>(defOp)) {
    if (auto intAttr = dyn_cast<IntegerAttr>(cst.getValue()))
      return getAffineConstantExpr(intAttr.getInt(), ctx);
    return std::nullopt;
  }

  if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(addOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(addOp.getRhs(), ivs, ctx);
    if (lhs && rhs)
      return *lhs + *rhs;
  }

  if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(subOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(subOp.getRhs(), ivs, ctx);
    if (lhs && rhs)
      return *lhs - *rhs;
  }

  if (auto mulOp = dyn_cast<arith::MulIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(mulOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(mulOp.getRhs(), ivs, ctx);
    if (lhs && rhs) {
      if (isa<AffineConstantExpr>(*lhs) || isa<AffineConstantExpr>(*rhs))
        return *lhs * *rhs;
    }
  }

  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp))
    return tryGetAffineExpr(castOp.getIn(), ivs, ctx);

  return std::nullopt;
}

static std::optional<AffineMap> tryBuildIndexingMap(OperandRange indices,
                                                    ArrayRef<Value> ivs,
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
  bool isRead;
};

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

    if (isMemoryEffectFree(&op))
      continue;

    ARTS_DEBUG("  bail: unknown op in body: " << op.getName());
    return false;
  }

  return !reads.empty() || !writes.empty();
}

//===----------------------------------------------------------------------===//
// Iterator type determination
//===----------------------------------------------------------------------===//

static void computeIteratorTypes(unsigned numDims,
                                 ArrayRef<MemrefAccessEntry> writes,
                                 SmallVectorImpl<StringRef> &iterTypes) {
  llvm::SmallBitVector dimsInOutputs(numDims, false);
  for (auto &entry : writes) {
    for (unsigned i = 0; i < entry.indexingMap.getNumResults(); ++i) {
      entry.indexingMap.getResult(i).walk([&](AffineExpr expr) {
        if (auto dimExpr = dyn_cast<AffineDimExpr>(expr))
          dimsInOutputs.set(dimExpr.getPosition());
      });
    }
  }

  for (unsigned d = 0; d < numDims; ++d)
    iterTypes.push_back(dimsInOutputs.test(d) ? "parallel" : "reduction");
}

//===----------------------------------------------------------------------===//
// Pattern classification
//===----------------------------------------------------------------------===//

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

static StringRef classifyPattern(ArrayRef<MemrefAccessEntry> reads,
                                 ArrayRef<MemrefAccessEntry> writes,
                                 ArrayRef<StringRef> iterTypes,
                                 unsigned numDims) {
  unsigned numParallel = 0, numReduction = 0;
  for (auto t : iterTypes) {
    if (t == "parallel")
      ++numParallel;
    else
      ++numReduction;
  }

  if (numReduction == 0) {
    for (auto &entry : reads) {
      if (hasConstantOffsets(entry.indexingMap))
        return AttrNames::Operation::LinalgClassification::Stencil;
    }
    return AttrNames::Operation::LinalgClassification::Elementwise;
  }

  if (numParallel == 2 && numReduction == 1 && numDims == 3 &&
      reads.size() >= 2 && writes.size() >= 1)
    return AttrNames::Operation::LinalgClassification::Matmul;

  return AttrNames::Operation::LinalgClassification::Reduction;
}

//===----------------------------------------------------------------------===//
// Pass implementation
//===----------------------------------------------------------------------===//

struct RaiseToLinalgPass
    : public arts::impl::RaiseToLinalgBase<RaiseToLinalgPass> {

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    unsigned analyzed = 0, classified = 0;

    module.walk([&](sde::SdeSuIterateOp iterOp) {
      ++analyzed;

      LoopNestInfo nest;
      if (!collectPerfectNest(iterOp, nest))
        return;
      if (!nest.innermostBody || nest.ivs.empty())
        return;

      unsigned numDims = nest.ivs.size();

      SmallVector<MemrefAccessEntry> reads, writes;
      if (!collectMemrefAccesses(*nest.innermostBody, nest.ivs, reads, writes,
                                 ctx))
        return;

      if (writes.empty())
        return;

      SmallVector<StringRef> iterTypes;
      computeIteratorTypes(numDims, writes, iterTypes);

      StringRef pattern = classifyPattern(reads, writes, iterTypes, numDims);

      // Stamp classification for ConvertSdeToArts to read.
      iterOp->setAttr(AttrNames::Operation::LinalgClassification::AttrName,
                      StringAttr::get(ctx, pattern));
      ++classified;

      ARTS_DEBUG("classified sde.su_iterate as '"
                 << pattern << "' with " << numDims << " dims ("
                 << reads.size() << " ins, " << writes.size() << " outs)");
    });

    ARTS_INFO("RaiseToLinalg: analyzed " << analyzed << " loops, classified "
                                         << classified);
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::sde::createRaiseToLinalgPass() {
  return std::make_unique<RaiseToLinalgPass>();
}
