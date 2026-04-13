///==========================================================================///
/// File: SdeOps.cpp
/// Defines SDE dialect operation helpers and verifiers.
///==========================================================================///

#include "arts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/OpImplementation.h"

using namespace mlir;
using namespace mlir::arts::sde;

#define GET_OP_CLASSES
#include "arts/dialect/sde/IR/SdeOps.cpp.inc"

SmallVector<Region *> SdeSuIterateOp::getLoopRegions() { return {&getBody()}; }

std::optional<SmallVector<Value>> SdeSuIterateOp::getLoopInductionVars() {
  auto numIVs = getLowerBounds().size();
  if (getBody().getNumArguments() < numIVs)
    return std::nullopt;
  SmallVector<Value> ivs;
  for (unsigned i = 0; i < numIVs; ++i)
    ivs.push_back(getBody().getArgument(i));
  return ivs;
}

std::optional<SmallVector<OpFoldResult>> SdeSuIterateOp::getLoopLowerBounds() {
  return SmallVector<OpFoldResult>(getLowerBounds().begin(),
                                   getLowerBounds().end());
}

std::optional<SmallVector<OpFoldResult>> SdeSuIterateOp::getLoopUpperBounds() {
  return SmallVector<OpFoldResult>(getUpperBounds().begin(),
                                   getUpperBounds().end());
}

std::optional<SmallVector<OpFoldResult>> SdeSuIterateOp::getLoopSteps() {
  return SmallVector<OpFoldResult>(getSteps().begin(), getSteps().end());
}

///===----------------------------------------------------------------------===///
/// SdeMuTokenOp verifier (rules V1-V3 from raise-memref-to-tensor RFC §4.4).
///===----------------------------------------------------------------------===///
LogicalResult SdeMuTokenOp::verify() {
  // V1: `source` is an `AnyTensor` (enforced by the TableGen operand
  //     constraint, so no runtime check required here).
  auto tensorTy = llvm::dyn_cast<RankedTensorType>(getSource().getType());
  if (!tensorTy) {
    // Unranked tensor: skip rank-specific checks.
    return success();
  }

  auto offsets = getOffsets();
  auto sizes = getSizes();

  // Offsets and sizes must either both be empty (whole-tensor token) or both
  // be supplied.
  if (offsets.size() != sizes.size()) {
    return emitOpError()
           << "expects offsets and sizes to have the same count (got "
           << offsets.size() << " offsets, " << sizes.size() << " sizes)";
  }

  if (offsets.empty())
    return success();

  // V2: rank match between offsets/sizes and source tensor.
  int64_t rank = tensorTy.getRank();
  if (static_cast<int64_t>(offsets.size()) != rank) {
    return emitOpError()
           << "expects offsets/sizes count (" << offsets.size()
           << ") to match source tensor rank (" << rank << ")";
  }

  // V3: static sizes must be non-negative; when both offset and size are
  //     constants for a dimension, offset + size <= source_dim.
  auto shape = tensorTy.getShape();
  for (int64_t i = 0; i < rank; ++i) {
    APInt sizeVal;
    bool sizeIsConst = matchPattern(sizes[i], m_ConstantInt(&sizeVal));
    if (sizeIsConst && sizeVal.isNegative()) {
      return emitOpError() << "expects non-negative size at dimension " << i
                           << " (got " << sizeVal.getSExtValue() << ")";
    }

    APInt offsetVal;
    bool offsetIsConst = matchPattern(offsets[i], m_ConstantInt(&offsetVal));

    if (sizeIsConst && offsetIsConst && !ShapedType::isDynamic(shape[i])) {
      int64_t off = offsetVal.getSExtValue();
      int64_t sz = sizeVal.getSExtValue();
      if (off < 0) {
        return emitOpError() << "expects non-negative offset at dimension "
                             << i << " (got " << off << ")";
      }
      if (off + sz > shape[i]) {
        return emitOpError()
               << "slice at dimension " << i << " (offset=" << off
               << ", size=" << sz
               << ") exceeds source tensor bound (dim=" << shape[i] << ")";
      }
    }
  }

  return success();
}

///===----------------------------------------------------------------------===///
/// SdeCuCodeletOp verifier (rules V4-V11 from raise-memref-to-tensor RFC §4.4).
///
/// Deferred: V12 (single-use token) — follow-up pattern, not a trait.
/// Automatic: V13 (no captures) — enforced by IsolatedFromAbove trait.
///===----------------------------------------------------------------------===///
LogicalResult SdeCuCodeletOp::verify() {
  auto tokens = getTokens();

  // V4: every operand is a `!sde.token<Ti>` for ranked Ti.
  //     (The variadic operand type is SdeTokenType, so the dyn_cast suffices.)
  SmallVector<TokenType> tokenTypes;
  tokenTypes.reserve(tokens.size());
  for (auto token : tokens) {
    auto tt = llvm::dyn_cast<TokenType>(token.getType());
    if (!tt)
      return emitOpError()
             << "expects every operand to be of type !sde.token";
    tokenTypes.push_back(tt);
  }

  // V5: body has exactly one block. Enforced by SingleBlock trait already,
  //     but SizedRegion<1> only checks block count; make sure a block exists.
  if (getBody().empty())
    return emitOpError() << "expects body to contain a single block";
  Block &entry = getBody().front();

  // V6: block-arg count == operand count; block-arg types == token slice_type.
  if (entry.getNumArguments() != tokens.size()) {
    return emitOpError() << "expects " << tokens.size()
                         << " block argument(s) (one per token); got "
                         << entry.getNumArguments();
  }
  for (auto [idx, tt] : llvm::enumerate(tokenTypes)) {
    Type argTy = entry.getArgument(idx).getType();
    Type sliceTy = tt.getSliceType();
    if (argTy != sliceTy) {
      return emitOpError()
             << "block argument #" << idx << " type (" << argTy
             << ") does not match token slice type (" << sliceTy << ")";
    }
  }

  // Collect writable (write / readwrite) tokens for V7-V10.
  SmallVector<unsigned> writableIndices;
  unsigned readOnlyCount = 0;
  for (auto [idx, token] : llvm::enumerate(tokens)) {
    auto muToken = token.getDefiningOp<SdeMuTokenOp>();
    if (!muToken) {
      // Without knowing the mode we cannot statically classify writability;
      // fall back to treating as read-only for result-count purposes. The
      // producer should almost always be `sde.mu_token`.
      ++readOnlyCount;
      continue;
    }
    SdeAccessMode mode = muToken.getMode();
    if (mode == SdeAccessMode::write || mode == SdeAccessMode::readwrite) {
      writableIndices.push_back(idx);
    } else {
      ++readOnlyCount;
    }
  }
  (void)readOnlyCount;

  // V7: result count equals writable-token count.
  if (getOutputs().size() != writableIndices.size()) {
    return emitOpError()
           << "expects one result per writable token: got "
           << getOutputs().size() << " result(s) and "
           << writableIndices.size()
           << " writable (write / readwrite) token(s); a <read> token must "
              "not have a yielded counterpart (RFC §4.4 V10)";
  }

  // V8: terminator is sde.yield; operand count matches result count; types
  //     match the matching writable token's slice_type (destination-passing
  //     style).
  auto yield = llvm::dyn_cast_or_null<SdeYieldOp>(entry.getTerminator());
  if (!yield)
    return emitOpError() << "expects body to terminate with sde.yield";
  if (yield.getValues().size() != getOutputs().size()) {
    return emitOpError() << "sde.yield operand count ("
                         << yield.getValues().size()
                         << ") does not match codelet result count ("
                         << getOutputs().size() << ")";
  }
  for (auto [i, writableIdx] : llvm::enumerate(writableIndices)) {
    Type yieldedTy = yield.getValues()[i].getType();
    Type sliceTy = tokenTypes[writableIdx].getSliceType();
    if (yieldedTy != sliceTy) {
      return emitOpError()
             << "sde.yield operand #" << i << " type (" << yieldedTy
             << ") does not match writable token slice type (" << sliceTy
             << ")";
    }

    // V9: the result type matches the parent tensor type of the writable
    //     token's producer (mu_token source).
    auto muToken = tokens[writableIdx].getDefiningOp<SdeMuTokenOp>();
    if (muToken) {
      Type parentTy = muToken.getSource().getType();
      if (getOutputs()[i].getType() != parentTy) {
        return emitOpError()
               << "result #" << i << " type (" << getOutputs()[i].getType()
               << ") does not match parent tensor type (" << parentTy
               << ") of the corresponding writable token";
      }
    }
  }

  // V11: best-effort check for conflicting modes on statically-overlapping
  //      slices of the same source tensor. Non-constant slices are delegated
  //      to runtime analysis.
  auto modeKind = [](SdeAccessMode m) {
    // 0 = read, 1 = write-ish (write or readwrite).
    return (m == SdeAccessMode::read) ? 0 : 1;
  };

  auto constantSlice = [](SdeMuTokenOp op,
                          SmallVectorImpl<int64_t> &offs,
                          SmallVectorImpl<int64_t> &szs) -> bool {
    if (op.getOffsets().size() != op.getSizes().size())
      return false;
    for (auto [off, sz] : llvm::zip(op.getOffsets(), op.getSizes())) {
      APInt offV, szV;
      if (!matchPattern(off, m_ConstantInt(&offV)))
        return false;
      if (!matchPattern(sz, m_ConstantInt(&szV)))
        return false;
      offs.push_back(offV.getSExtValue());
      szs.push_back(szV.getSExtValue());
    }
    return true;
  };

  auto slicesOverlap = [](ArrayRef<int64_t> aOff, ArrayRef<int64_t> aSz,
                          ArrayRef<int64_t> bOff, ArrayRef<int64_t> bSz) {
    // Whole-tensor tokens (empty offsets/sizes) overlap everything from the
    // same parent.
    if (aOff.empty() || bOff.empty())
      return true;
    if (aOff.size() != bOff.size())
      return true; // degenerate; be conservative.
    for (size_t i = 0; i < aOff.size(); ++i) {
      int64_t aEnd = aOff[i] + aSz[i];
      int64_t bEnd = bOff[i] + bSz[i];
      if (aEnd <= bOff[i] || bEnd <= aOff[i])
        return false;
    }
    return true;
  };

  for (size_t i = 0; i < tokens.size(); ++i) {
    auto ai = tokens[i].getDefiningOp<SdeMuTokenOp>();
    if (!ai)
      continue;
    SmallVector<int64_t> aOff, aSz;
    bool aConst = constantSlice(ai, aOff, aSz);
    for (size_t j = i + 1; j < tokens.size(); ++j) {
      auto bi = tokens[j].getDefiningOp<SdeMuTokenOp>();
      if (!bi)
        continue;
      if (ai.getSource() != bi.getSource())
        continue;
      if (modeKind(ai.getMode()) == modeKind(bi.getMode()) &&
          ai.getMode() == bi.getMode())
        continue; // identical mode: always fine.
      // read+read already filtered above; read+write or write+write require
      // disjointness.
      SmallVector<int64_t> bOff, bSz;
      bool bConst = constantSlice(bi, bOff, bSz);
      if (!aConst || !bConst)
        continue; // delegate to runtime.
      if (slicesOverlap(aOff, aSz, bOff, bSz)) {
        return emitOpError()
               << "tokens #" << i << " and #" << j
               << " have conflicting access modes on statically-overlapping "
                  "slices of the same source tensor (RFC §4.4 V11)";
      }
    }
  }

  // Deferred: V12 single-use token. Will be enforced by a dedicated pattern
  // in a follow-up; see raise-memref-to-tensor-rfc.md §4.4.
  return success();
}

LogicalResult SdeMuReductionDeclOp::verify() {
  if (auto identity = getIdentity()) {
    auto typedIdentity = dyn_cast<TypedAttr>(*identity);
    if (!typedIdentity)
      return emitOpError() << "expects identity to be a typed attribute";
    if (typedIdentity.getType() != getType())
      return emitOpError() << "expects identity type to match reduction type";
  }

  if (getReductionKind() != SdeReductionKind::custom)
    return success();

  if (getCombiner().empty())
    return emitOpError() << "expects a combiner region for custom reductions";

  Block &entry = getCombiner().front();
  if (entry.getNumArguments() != 2)
    return emitOpError()
           << "expects custom combiner region with two block arguments";
  if (entry.getArgumentTypes()[0] != getType() ||
      entry.getArgumentTypes()[1] != getType())
    return emitOpError()
           << "expects custom combiner arguments to match reduction type";

  auto yield = dyn_cast<SdeYieldOp>(entry.getTerminator());
  if (!yield || yield.getValues().size() != 1 ||
      yield.getValues().front().getType() != getType())
    return emitOpError() << "expects custom combiner to terminate with "
                            "arts_sde.yield of the reduction type";

  return success();
}
