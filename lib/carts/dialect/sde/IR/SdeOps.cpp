///==========================================================================///
/// File: SdeOps.cpp
/// Defines SDE dialect operation helpers and verifiers.
///==========================================================================///

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/dialect/sde/Utils/SdeCuStructure.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/OpImplementation.h"
#include "llvm/ADT/STLExtras.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::sde;

#define GET_OP_CLASSES
#include "carts/dialect/sde/IR/SdeOps.cpp.inc"

namespace {

static bool isAllowedSuIterateChild(Operation *op) {
  return isa<SdeYieldOp, SdeCuRegionOp, SdeCuAtomicOp>(op) ||
         isa<SdeArrayLayoutOp, SdeArrayLayoutRootOp, SdeSuBarrierOp>(op);
}

static bool suHasTypedArrayLayoutFact(SdeSuIterateOp op, int64_t arrayId,
                                      SdeAccessMode mode) {
  if (op.getBody().empty())
    return false;
  for (SdeArrayLayoutOp layout :
       op.getBody().front().getOps<SdeArrayLayoutOp>())
    if (layout.getArrayId() == arrayId && layout.getMode() == mode)
      return true;
  return false;
}

static bool suHasAnyTypedArrayLayoutFact(SdeSuIterateOp op) {
  if (op.getBody().empty())
    return false;
  return !op.getBody().front().getOps<SdeArrayLayoutOp>().empty();
}

static bool valueDefinedOutsideRegion(Value value, Region *region) {
  Operation *def = value.getDefiningOp();
  if (!def)
    return true;
  return !region->isAncestor(def->getParentRegion());
}

static bool hasExternalWriteRoot(SdeSuIterateOp op) {
  Region &body = op.getBody();
  for (SdeCuRegionOp cu : body.getOps<SdeCuRegionOp>()) {
    for (memref::StoreOp store : cu.getBody().getOps<memref::StoreOp>()) {
      if (valueDefinedOutsideRegion(store.getMemref(), &body))
        return true;
    }
  }
  return false;
}

static bool isAllowedSuDistributeChild(Operation *op) {
  return sde::isSuOp(op) || isa<SdeSuBarrierOp, SdeSuHaloOp,
                                SdeSuReduceScatterOp, SdeSuAllToAllOp>(op);
}

static LogicalResult verifyCuContainsNoScheduling(Operation *cu) {
  bool failed = false;
  cu->walk([&](Operation *nested) {
    if (nested == cu)
      return WalkResult::advance();
    if (!sde::isCuForbiddenSchedulingOp(nested))
      return WalkResult::advance();
    nested->emitOpError()
        << "is nested inside an " << cu->getName().getStringRef()
        << " body; compute units are executable leaves and SU scheduling must "
           "be represented outside the CU";
    failed = true;
    return WalkResult::advance();
  });
  return failure(failed);
}

static std::optional<SdeAccessMode> modeForLayoutRole(LayoutGraphRole role) {
  switch (role) {
  case LayoutGraphRole::read:
    return SdeAccessMode::read;
  case LayoutGraphRole::write:
    return SdeAccessMode::write;
  case LayoutGraphRole::unknown:
    return std::nullopt;
  }
  return std::nullopt;
}

static bool isKnownLayoutKind(StringRef value) {
  return value == AttrNames::LayoutGraph::BlockParallel ||
         value == AttrNames::LayoutGraph::BlockContraction ||
         value == AttrNames::LayoutGraph::Replicated;
}

static LogicalResult
parseRequiredI64ArrayLayoutField(SdeSuIterateOp op, DictionaryAttr dict,
                                 unsigned entryIndex, StringRef fieldName,
                                 SmallVectorImpl<int64_t> &values) {
  auto attr = dyn_cast_or_null<ArrayAttr>(dict.get(fieldName));
  if (!attr) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex << " field '"
                     << fieldName << "' must be an i64 array";
    return failure();
  }
  std::optional<SmallVector<int64_t, 4>> parsed = readI64ArrayAttr(attr);
  if (!parsed) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex << " field '"
                     << fieldName << "' must contain only integer attributes";
    return failure();
  }
  values.assign(parsed->begin(), parsed->end());
  return success();
}

static LogicalResult
parseOptionalI64ArrayLayoutField(SdeSuIterateOp op, DictionaryAttr dict,
                                 unsigned entryIndex, StringRef fieldName,
                                 SmallVectorImpl<int64_t> &values) {
  Attribute raw = dict.get(fieldName);
  if (!raw)
    return success();
  auto attr = dyn_cast<ArrayAttr>(raw);
  if (!attr) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex << " field '"
                     << fieldName << "' must be an i64 array";
    return failure();
  }
  std::optional<SmallVector<int64_t, 4>> parsed = readI64ArrayAttr(attr);
  if (!parsed) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex << " field '"
                     << fieldName << "' must contain only integer attributes";
    return failure();
  }
  values.assign(parsed->begin(), parsed->end());
  return success();
}

static LogicalResult verifyPositiveLayoutShape(SdeSuIterateOp op,
                                               unsigned entryIndex,
                                               StringRef fieldName,
                                               ArrayRef<int64_t> values) {
  if (values.empty()) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex << " field '"
                     << fieldName << "' must not be empty";
    return failure();
  }
  for (int64_t value : values) {
    if (value <= 0) {
      op.emitOpError() << "arrayLayout entry #" << entryIndex << " field '"
                       << fieldName << "' must contain positive extents";
      return failure();
    }
  }
  return success();
}

static LogicalResult verifyArrayLayoutEntry(SdeSuIterateOp op, Attribute entry,
                                            unsigned entryIndex,
                                            LayoutGraphFact &fact) {
  auto dict = dyn_cast<DictionaryAttr>(entry);
  if (!dict) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex
                     << " must be a dictionary attribute";
    return failure();
  }

  auto idAttr =
      dyn_cast_or_null<IntegerAttr>(dict.get(AttrNames::LayoutGraph::ArrayId));
  if (!idAttr || idAttr.getInt() < 0) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex
                     << " must include non-negative integer arrayId";
    return failure();
  }

  auto roleAttr =
      dyn_cast_or_null<StringAttr>(dict.get(AttrNames::LayoutGraph::Role));
  if (!roleAttr ||
      !modeForLayoutRole(parseLayoutGraphRole(roleAttr.getValue()))) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex
                     << " must include role \"read\" or \"write\"";
    return failure();
  }

  auto kindAttr =
      dyn_cast_or_null<StringAttr>(dict.get(AttrNames::LayoutGraph::Kind));
  if (!kindAttr || !isKnownLayoutKind(kindAttr.getValue())) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex
                     << " must include a known layout kind";
    return failure();
  }

  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
  SmallVector<int64_t, 4> budgetBlockShape;
  if (failed(parseRequiredI64ArrayLayoutField(op, dict, entryIndex,
                                              AttrNames::LayoutGraph::OwnerDims,
                                              ownerDims)) ||
      failed(parseRequiredI64ArrayLayoutField(
          op, dict, entryIndex, AttrNames::LayoutGraph::BlockShape,
          blockShape)) ||
      failed(parseOptionalI64ArrayLayoutField(
          op, dict, entryIndex, AttrNames::LayoutGraph::BudgetBlockShape,
          budgetBlockShape)))
    return failure();

  SmallVector<int64_t, 4> seenOwnerDims;
  for (int64_t dim : ownerDims) {
    if (dim < 0) {
      op.emitOpError() << "arrayLayout entry #" << entryIndex
                       << " owner dimensions must be non-negative";
      return failure();
    }
    if (llvm::is_contained(seenOwnerDims, dim)) {
      op.emitOpError() << "arrayLayout entry #" << entryIndex
                       << " owner dimensions must be unique";
      return failure();
    }
    seenOwnerDims.push_back(dim);
  }

  ArrayLayoutKind kind = parseLayoutKind(kindAttr.getValue());
  if (kind == ArrayLayoutKind::replicated && !ownerDims.empty()) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex
                     << " replicated layout must not carry owner dimensions";
    return failure();
  }
  if (kind == ArrayLayoutKind::blockParallel && ownerDims.empty()) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex
                     << " block_parallel layout must carry owner dimensions";
    return failure();
  }

  if (failed(verifyPositiveLayoutShape(
          op, entryIndex, AttrNames::LayoutGraph::BlockShape, blockShape)))
    return failure();
  if (!budgetBlockShape.empty() &&
      failed(verifyPositiveLayoutShape(op, entryIndex,
                                       AttrNames::LayoutGraph::BudgetBlockShape,
                                       budgetBlockShape)))
    return failure();

  if (Attribute rawMuBlockCount =
          dict.get(AttrNames::LayoutGraph::MuBlockCount)) {
    auto muBlockCount = dyn_cast<IntegerAttr>(rawMuBlockCount);
    if (!muBlockCount || muBlockCount.getInt() <= 0) {
      op.emitOpError() << "arrayLayout entry #" << entryIndex
                       << " muBlockCount must be a positive integer";
      return failure();
    }
  }

  std::optional<LayoutGraphFact> parsed = parseArrayLayoutFact(dict);
  if (!parsed) {
    op.emitOpError() << "arrayLayout entry #" << entryIndex
                     << " is not a parseable SDE layout fact";
    return failure();
  }
  fact = std::move(*parsed);
  return success();
}

static LogicalResult
verifyArrayLayoutAttr(SdeSuIterateOp op, ArrayAttr layout,
                      SmallVectorImpl<LayoutGraphFact> &facts) {
  facts.clear();
  if (!layout)
    return success();
  facts.reserve(layout.size());
  for (auto [index, entry] : llvm::enumerate(layout)) {
    LayoutGraphFact fact;
    if (failed(verifyArrayLayoutEntry(op, entry, index, fact)))
      return failure();
    facts.push_back(std::move(fact));
  }
  return success();
}

} // namespace

//===----------------------------------------------------------------------===//
// SdeCuRegionOp — custom assembly format + verifier
//===----------------------------------------------------------------------===//

// Print: sde.cu_region <kind> [nowait]
//        [iter_args(%a = %init : type) -> (type) | -> (type)]
//        { body } [attr-dict]
void SdeCuRegionOp::print(OpAsmPrinter &p) {
  // Print kind enum in angle-bracket form: <parallel>, <single>, <task>
  p << " <" << stringifySdeCuKind(getKind()) << ">";
  if (getNowait())
    p << " nowait";
  if (!getIterArgs().empty()) {
    p << " iter_args(";
    Block &body = getBody().front();
    llvm::interleaveComma(
        llvm::zip(body.getArguments(), getIterArgs()), p, [&](auto pair) {
          p << std::get<0>(pair) << " = " << std::get<1>(pair);
        });
    p << " : ";
    llvm::interleaveComma(getIterArgs().getTypes(), p);
    p << ")";
    p << " -> (";
    llvm::interleaveComma(getResultTypes(), p);
    p << ")";
  } else if (getNumResults() != 0) {
    p << " -> (";
    llvm::interleaveComma(getResultTypes(), p);
    p << ")";
  }
  p << " ";
  p.printRegion(getBody(), /*printEntryBlockArgs=*/false,
                /*printBlockTerminators=*/getNumResults() != 0 ||
                    !getIterArgs().empty());
  p.printOptionalAttrDict((*this)->getAttrs(), {"kind", "nowait"});
}

// Parse: sde.cu_region <kind> [nowait]
//        [iter_args(%a = %init : type) -> (type) | -> (type)]
//        { body } [attr-dict]
ParseResult SdeCuRegionOp::parse(OpAsmParser &parser, OperationState &result) {
  MLIRContext *ctx = parser.getContext();

  // Parse kind: <parallel> | <single> | <task>
  {
    SdeCuKindAttr kindAttr;
    if (parser.parseCustomAttributeWithFallback(kindAttr, Type{}, "kind",
                                                result.attributes))
      return failure();
  }

  // Parse optional nowait
  if (succeeded(parser.parseOptionalKeyword("nowait")))
    result.addAttribute("nowait", UnitAttr::get(ctx));

  // Parse optional iter_args
  SmallVector<OpAsmParser::UnresolvedOperand> iterArgOperands;
  SmallVector<OpAsmParser::Argument> bodyArgs;
  SmallVector<Type> iterArgTypes;
  SmallVector<Type> resultTypes;
  bool hasIterArgs = false;

  if (succeeded(parser.parseOptionalKeyword("iter_args"))) {
    hasIterArgs = true;
    if (parser.parseLParen())
      return failure();

    if (failed(parser.parseOptionalRParen())) {
      do {
        OpAsmParser::Argument bodyArg;
        OpAsmParser::UnresolvedOperand initVal;
        if (parser.parseArgument(bodyArg) || parser.parseEqual() ||
            parser.parseOperand(initVal))
          return failure();
        bodyArgs.push_back(bodyArg);
        iterArgOperands.push_back(initVal);
      } while (succeeded(parser.parseOptionalComma()));

      if (parser.parseColon() || parser.parseTypeList(iterArgTypes) ||
          parser.parseRParen())
        return failure();

      if (iterArgTypes.size() != iterArgOperands.size())
        return parser.emitError(parser.getCurrentLocation(),
                                "iter_args type count mismatch");

      for (auto [arg, ty] : llvm::zip(bodyArgs, iterArgTypes))
        arg.type = ty;

      if (parser.resolveOperands(iterArgOperands, iterArgTypes,
                                 parser.getCurrentLocation(), result.operands))
        return failure();

      if (parser.parseArrow() || parser.parseLParen() ||
          parser.parseTypeList(resultTypes) || parser.parseRParen())
        return failure();

      result.addTypes(resultTypes);
    }
  } else if (succeeded(parser.parseOptionalArrow())) {
    if (parser.parseLParen() || parser.parseTypeList(resultTypes) ||
        parser.parseRParen())
      return failure();
    result.addTypes(resultTypes);
  }

  // Parse region
  Region *body = result.addRegion();
  if (parser.parseRegion(*body, bodyArgs,
                         /*enableNameShadowing=*/false))
    return failure();

  // Ensure block exists
  if (body->empty())
    body->push_back(new Block());

  // Match the custom assembly form that omits empty yields for no-result
  // regions.
  Block &entry = body->front();
  if (!hasIterArgs && result.types.empty() &&
      (entry.empty() || !entry.back().hasTrait<OpTrait::IsTerminator>())) {
    // Build the implicit yield from the parser context, not from the region:
    // during parse the body region is not yet attached to its op, so deriving a
    // builder context through the block's parent region (OpBuilder::atBlockEnd)
    // would assert. setInsertionPointToEnd only needs the block, not a context.
    OpBuilder endBuilder(parser.getContext());
    endBuilder.setInsertionPointToEnd(&entry);
    SdeYieldOp::create(endBuilder, result.location, ValueRange{});
  }

  // Parse optional attr-dict
  if (parser.parseOptionalAttrDict(result.attributes))
    return failure();

  return success();
}

LogicalResult SdeCuRegionOp::verify() {
  if (getBody().empty())
    return emitOpError() << "expects body to contain a single block";

  Block &entry = getBody().front();
  unsigned numIterArgs = getIterArgs().size();

  if (numIterArgs > 0) {
    if (entry.getNumArguments() != numIterArgs)
      return emitOpError() << "expects " << numIterArgs
                           << " block argument(s) for iter_args; got "
                           << entry.getNumArguments();
    for (auto [i, pair] :
         llvm::enumerate(llvm::zip(entry.getArguments(), getIterArgs()))) {
      auto [blockArg, iterArg] = pair;
      if (blockArg.getType() != iterArg.getType())
        return emitOpError()
               << "block argument #" << i << " type (" << blockArg.getType()
               << ") does not match iter_arg type (" << iterArg.getType()
               << ")";
    }

    if (getNumResults() != numIterArgs)
      return emitOpError() << "expects " << numIterArgs
                           << " result(s) matching iter_args; got "
                           << getNumResults();
  } else if (entry.getNumArguments() != 0) {
    return emitOpError() << "expects no block arguments without iter_args";
  }

  auto yield = dyn_cast_or_null<SdeYieldOp>(entry.getTerminator());
  if (!yield)
    return emitOpError() << "expects body to terminate with sde.yield";

  if (getNumResults() != 0 || numIterArgs > 0) {
    if (yield.getValues().size() != getNumResults())
      return emitOpError() << "sde.yield operand count ("
                           << yield.getValues().size()
                           << ") does not match result count ("
                           << getNumResults() << ")";
    for (auto [i, pair] :
         llvm::enumerate(llvm::zip(yield.getValues(), getResultTypes()))) {
      auto [yielded, resultTy] = pair;
      if (yielded.getType() != resultTy)
        return emitOpError()
               << "sde.yield operand #" << i << " type (" << yielded.getType()
               << ") does not match result type (" << resultTy << ")";
    }
  } else {
    if (!yield.getValues().empty())
      return emitOpError()
             << "sde.yield operands require matching cu_region results";
  }

  if (failed(verifyCuContainsNoScheduling(getOperation())))
    return failure();

  if (auto groupCounts = readI64ArrayAttr(getGroupBlockCountAttr())) {
    if (groupCounts->empty())
      return emitOpError()
             << "groupBlockCount must name at least one owner-dim entry";
    for (int64_t count : *groupCounts)
      if (count <= 0)
        return emitOpError()
               << "groupBlockCount entries must be positive whole block "
                  "multiples";
  }

  return success();
}

LogicalResult SdeCuTaskOp::verify() {
  return verifyCuContainsNoScheduling(getOperation());
}

LogicalResult SdeCuReduceOp::verify() {
  return verifyCuContainsNoScheduling(getOperation());
}

//===----------------------------------------------------------------------===//
// SdeSuIterateOp — custom assembly format + verifier
//===----------------------------------------------------------------------===//

// Assembly format (preserves the shape of the previous declarative format,
// adding optional `iter_args` / result types and always emitting sde.yield):
//
//   sde.su_iterate (%lb) to (%ub) step (%step)
//     [schedule(<kind> [, %chunk])]
//     [nowait]
//     [reduction [<kinds>] (%accs : types)]
//     [reduction_strategy(<strategy>)]
//     [classification(<class>)]
//     [iter_args(%a = %init : type) -> (type)]
//     { body }
//     [attr-dict]

void SdeSuIterateOp::print(OpAsmPrinter &p) {
  // (lb) to (ub) step (step)
  p << " (" << getLowerBounds() << ") to (" << getUpperBounds() << ") step ("
    << getSteps() << ")";

  // nowait
  if (getNowait())
    p << " nowait";

  // reduction[<kinds>](%accs : types)
  if (auto reductionKinds = getReductionKindsAttr()) {
    p << " reduction[";
    llvm::interleaveComma(reductionKinds, p);
    p << "](";
    llvm::interleaveComma(getReductionAccumulators(), p,
                          [&](Value v) { p << v; });
    p << " : ";
    llvm::interleaveComma(getReductionAccumulators().getTypes(), p);
    p << ")";
  }

  // classification(<class>)
  if (auto cls = getStructuredClassification()) {
    p << " classification(<" << stringifySdeStructuredClassification(*cls)
      << ">)";
  }

  // iter_args(%a = %init : type) -> (type)
  // Block arguments beyond the induction variables carry iter_args.
  unsigned numIVs = getLowerBounds().size();
  unsigned numResults = getNumResults();
  if (numResults > 0) {
    Block &body = getBody().front();
    p << " iter_args(";
    for (unsigned i = 0; i < numResults; ++i) {
      if (i > 0)
        p << ", ";
      p << body.getArgument(numIVs + i) << " = "
        << getReductionAccumulators()[i];
    }
    p << " : ";
    llvm::interleaveComma(getResultTypes(), p);
    p << ") -> (";
    llvm::interleaveComma(getResultTypes(), p);
    p << ")";
  }

  // body — hide entry block args; show yield only when results exist
  p << " ";
  p.printRegion(getBody(), /*printEntryBlockArgs=*/false,
                /*printBlockTerminators=*/numResults > 0);

  // attr-dict — elide attributes with dedicated syntax
  SmallVector<StringRef> elidedAttrs = {
      "schedule", "nowait", "reductionKinds", "structuredClassification",
      getOperandSegmentSizesAttrName().getValue()};
  p.printOptionalAttrDict((*this)->getAttrs(), elidedAttrs);
}

ParseResult SdeSuIterateOp::parse(OpAsmParser &parser, OperationState &result) {
  MLIRContext *ctx = parser.getContext();
  auto indexType = IndexType::get(ctx);

  // ---- (lb) to (ub) step (step) ----
  SmallVector<OpAsmParser::UnresolvedOperand> lbOps, ubOps, stepOps;
  if (parser.parseLParen() || parser.parseOperandList(lbOps) ||
      parser.parseRParen() || parser.parseKeyword("to") ||
      parser.parseLParen() || parser.parseOperandList(ubOps) ||
      parser.parseRParen() || parser.parseKeyword("step") ||
      parser.parseLParen() || parser.parseOperandList(stepOps) ||
      parser.parseRParen())
    return failure();

  unsigned numDims = lbOps.size();
  SmallVector<Type> indexTypes(numDims, indexType);
  if (parser.resolveOperands(lbOps, indexTypes, parser.getCurrentLocation(),
                             result.operands) ||
      parser.resolveOperands(ubOps, indexTypes, parser.getCurrentLocation(),
                             result.operands) ||
      parser.resolveOperands(stepOps, indexTypes, parser.getCurrentLocation(),
                             result.operands))
    return failure();

  // ---- optional nowait ----
  if (succeeded(parser.parseOptionalKeyword("nowait")))
    result.addAttribute("nowait", UnitAttr::get(ctx));

  // ---- optional reduction[<kinds>](%accs : types) ----
  SmallVector<OpAsmParser::UnresolvedOperand> redAccOps;
  SmallVector<Type> redAccTypes;
  if (succeeded(parser.parseOptionalKeyword("reduction"))) {
    Attribute kindsAttr;
    if (parser.parseLSquare())
      return failure();
    // Parse the array attribute (e.g. [#sde<reduction_kind<add>>])
    if (parser.parseAttribute(kindsAttr))
      return failure();
    if (parser.parseRSquare())
      return failure();
    result.addAttribute("reductionKinds", kindsAttr);

    if (parser.parseLParen() || parser.parseOperandList(redAccOps) ||
        parser.parseColon() || parser.parseTypeList(redAccTypes) ||
        parser.parseRParen() ||
        parser.resolveOperands(redAccOps, redAccTypes,
                               parser.getCurrentLocation(), result.operands))
      return failure();
  }

  // reduction_strategy(<strategy>) — retired Step 9; discard on parse for
  // round-trip.
  if (succeeded(parser.parseOptionalKeyword("reduction_strategy"))) {
    SdeReductionStrategyAttr stratAttr;
    NamedAttrList ignoredAttrs;
    if (parser.parseLParen() ||
        parser.parseCustomAttributeWithFallback(
            stratAttr, Type{}, "reductionStrategy", ignoredAttrs) ||
        parser.parseRParen())
      return failure();
  }

  // ---- optional classification(<class>) ----
  if (succeeded(parser.parseOptionalKeyword("classification"))) {
    SdeStructuredClassificationAttr classAttr;
    if (parser.parseLParen() ||
        parser.parseCustomAttributeWithFallback(
            classAttr, Type{}, "structuredClassification", result.attributes) ||
        parser.parseRParen())
      return failure();
  }

  // ---- optional iter_args(%a = %init : type) -> (type) ----
  SmallVector<OpAsmParser::Argument> iterBodyArgs;
  SmallVector<OpAsmParser::UnresolvedOperand> iterArgOperands;
  SmallVector<Type> iterArgTypes;

  if (succeeded(parser.parseOptionalKeyword("iter_args"))) {
    if (parser.parseLParen())
      return failure();

    if (failed(parser.parseOptionalRParen())) {
      do {
        OpAsmParser::Argument bodyArg;
        OpAsmParser::UnresolvedOperand initVal;
        if (parser.parseArgument(bodyArg) || parser.parseEqual() ||
            parser.parseOperand(initVal))
          return failure();
        iterBodyArgs.push_back(bodyArg);
        iterArgOperands.push_back(initVal);
      } while (succeeded(parser.parseOptionalComma()));

      if (parser.parseColon() || parser.parseTypeList(iterArgTypes) ||
          parser.parseRParen())
        return failure();

      if (iterArgTypes.size() != iterArgOperands.size())
        return parser.emitError(parser.getCurrentLocation(),
                                "iter_args type count mismatch");

      for (auto [arg, ty] : llvm::zip(iterBodyArgs, iterArgTypes))
        arg.type = ty;

      // iter_args init values go into the reductionAccumulators segment.
      if (parser.resolveOperands(iterArgOperands, iterArgTypes,
                                 parser.getCurrentLocation(), result.operands))
        return failure();

      SmallVector<Type> resultTypes;
      if (parser.parseArrow() || parser.parseLParen() ||
          parser.parseTypeList(resultTypes) || parser.parseRParen())
        return failure();
      result.addTypes(resultTypes);
    }
  }

  // ---- Operand segment sizes ----
  // Order: lowerBounds | upperBounds | steps | redAccs+iterArgs
  SmallVector<int32_t> segmentSizes = {
      static_cast<int32_t>(numDims), static_cast<int32_t>(numDims),
      static_cast<int32_t>(numDims),
      static_cast<int32_t>(redAccOps.size() + iterArgOperands.size())};
  result.addAttribute(SdeSuIterateOp::getOperandSegmentSizeAttr(),
                      parser.getBuilder().getDenseI32ArrayAttr(segmentSizes));

  // ---- Body region ----
  // Build block arguments: numDims IV args + iter_args body args.
  SmallVector<OpAsmParser::Argument> allBodyArgs;
  for (unsigned i = 0; i < numDims; ++i) {
    OpAsmParser::Argument ivArg;
    ivArg.type = indexType;
    allBodyArgs.push_back(ivArg);
  }
  for (auto &arg : iterBodyArgs)
    allBodyArgs.push_back(arg);

  Region *body = result.addRegion();
  if (parser.parseRegion(*body, allBodyArgs,
                         /*enableNameShadowing=*/false))
    return failure();

  if (body->empty())
    body->push_back(new Block());

  // Auto-insert empty sde.yield when body has no terminator.
  Block &entry = body->front();
  if (entry.empty() || !entry.back().hasTrait<OpTrait::IsTerminator>()) {
    // Build the implicit yield from the parser context, not from the region:
    // during parse the body region is not yet attached to its op, so deriving a
    // builder context through the block's parent region (OpBuilder::atBlockEnd)
    // would assert. setInsertionPointToEnd only needs the block, not a context.
    OpBuilder endBuilder(parser.getContext());
    endBuilder.setInsertionPointToEnd(&entry);
    SdeYieldOp::create(endBuilder, result.location, ValueRange{});
  }

  // ---- attr-dict ----
  if (parser.parseOptionalAttrDict(result.attributes))
    return failure();

  // Retired su_iterate attrs (Steps 8–9): drop on ingest so old IR round-trips.
  static constexpr StringRef kRetiredSuIterateAttrs[] = {
      "physicalOwnerDims", "physicalBlockShape", "physicalHaloShape",
      "iterationTopology", "distributionKind",   "reductionStrategy",
      "layoutsDisagree",   "logicalWorkerSlice",
  };
  for (StringRef name : kRetiredSuIterateAttrs)
    result.attributes.erase(name);

  return success();
}

LogicalResult SdeSuIterateOp::verify() {
  if (getBody().empty())
    return emitOpError() << "expects body to contain a single block";

  Block &entry = getBody().front();
  unsigned numDims = getLowerBounds().size();
  if (numDims == 0)
    return emitOpError() << "expects at least one loop dimension";
  if (getUpperBounds().size() != numDims)
    return emitOpError() << "expects " << numDims
                         << " upper bound(s) matching lower bounds; got "
                         << getUpperBounds().size();
  if (getSteps().size() != numDims)
    return emitOpError() << "expects " << numDims
                         << " step value(s) matching lower bounds; got "
                         << getSteps().size();

  unsigned expectedBlockArgs = numDims + getNumResults();
  if (entry.getNumArguments() != expectedBlockArgs)
    return emitOpError() << "expects " << expectedBlockArgs
                         << " body block argument(s) (" << numDims
                         << " induction + " << getNumResults()
                         << " iter_arg/result); got "
                         << entry.getNumArguments();
  for (unsigned dim = 0; dim < numDims; ++dim) {
    if (!entry.getArgument(dim).getType().isIndex())
      return emitOpError() << "induction block argument #" << dim
                           << " must have index type";
  }
  if (getNumResults() > getReductionAccumulators().size())
    return emitOpError() << "expects at least one iter_arg/init operand per "
                            "result-producing su_iterate";
  for (auto [i, pair] : llvm::enumerate(llvm::zip(
           entry.getArguments().drop_front(numDims), getResultTypes()))) {
    auto [blockArg, resultTy] = pair;
    if (blockArg.getType() != resultTy)
      return emitOpError() << "iter_arg block argument #" << i << " type ("
                           << blockArg.getType()
                           << ") does not match result type (" << resultTy
                           << ")";
  }

  // Body must have a terminator (sde.yield)
  auto yield = dyn_cast_or_null<SdeYieldOp>(entry.getTerminator());
  if (!yield)
    return emitOpError() << "expects body to terminate with sde.yield";

  // Result count must match yield operand count
  if (yield.getValues().size() != getNumResults())
    return emitOpError() << "sde.yield operand count ("
                         << yield.getValues().size()
                         << ") does not match result count (" << getNumResults()
                         << ")";

  // When results are present, check type consistency
  for (auto [i, pair] :
       llvm::enumerate(llvm::zip(yield.getValues(), getResultTypes()))) {
    auto [yielded, resultTy] = pair;
    if (yielded.getType() != resultTy)
      return emitOpError() << "sde.yield operand #" << i << " type ("
                           << yielded.getType()
                           << ") does not match result type (" << resultTy
                           << ")";
  }

  bool failed = false;
  for (Operation &child : entry) {
    if (isAllowedSuIterateChild(&child))
      continue;
    child.emitOpError()
        << "is directly inside an sde.su_iterate body; SU bodies are "
           "scheduling-only and may contain only direct-boundary CUs "
           "(sde.cu_region or sde.cu_atomic), sde.array_layout_root, "
           "sde.su_barrier, and the sde.yield terminator";
    failed = true;
  }

  bool hasCommittedPhysicalLayout = hasCommittedCuMuPartitionFacts(*this);

  SmallVector<SdeArrayLayoutRootOp, 4> roots;
  for (SdeArrayLayoutRootOp root : entry.getOps<SdeArrayLayoutRootOp>())
    roots.push_back(root);

  ArrayAttr layout = getArrayLayoutAttr();
  SmallVector<LayoutGraphFact, 4> facts;
  if (::mlir::failed(verifyArrayLayoutAttr(*this, layout, facts)))
    failed = true;

  SmallVector<SdeArrayLayoutOp, 4> typedLayouts;
  for (SdeArrayLayoutOp typedLayout : entry.getOps<SdeArrayLayoutOp>())
    typedLayouts.push_back(typedLayout);

  auto hasTypedLayoutFor = [&](int64_t arrayId, SdeAccessMode mode) {
    for (SdeArrayLayoutOp typedLayout : typedLayouts)
      if (static_cast<int64_t>(typedLayout.getArrayId()) == arrayId &&
          typedLayout.getMode() == mode)
        return true;
    return false;
  };

  bool hasLayoutFacts =
      (layout && !layout.empty()) || !typedLayouts.empty();

  auto layoutRootSupportedWithoutDict = [&]() {
    if (hasCommittedPhysicalLayout || suHasAnyTypedArrayLayoutFact(*this))
      return true;
    for (SdeArrayLayoutRootOp root : roots) {
      auto muType = dyn_cast<MemRefType>(root.getRoot().getType());
      if (muType && recoverMuPhysicalLayoutFromExpandedType(muType))
        return true;
    }
    return false;
  }();

  if (!hasLayoutFacts && !roots.empty() && !layoutRootSupportedWithoutDict) {
    roots.front().emitOpError()
        << "commits array root provenance but the enclosing sde.su_iterate "
           "has no arrayLayout or typed sde.array_layout fact";
    failed = true;
  }

  if (hasLayoutFacts) {
    for (SdeArrayLayoutRootOp root : roots) {
      auto muType = dyn_cast<MemRefType>(root.getRoot().getType());
      if (hasCommittedPhysicalLayout ||
          (muType && recoverMuPhysicalLayoutFromExpandedType(muType)))
        continue;
      bool matchesLayout = hasTypedLayoutFor(root.getArrayId(), root.getMode());
      for (const LayoutGraphFact &fact : facts) {
        std::optional<SdeAccessMode> mode = modeForLayoutRole(fact.role);
        if (mode && fact.id == static_cast<int64_t>(root.getArrayId()) &&
            *mode == root.getMode()) {
          matchesLayout = true;
          break;
        }
      }
      if (!matchesLayout &&
          suHasTypedArrayLayoutFact(*this, root.getArrayId(), root.getMode()))
        matchesLayout = true;
      if (!matchesLayout) {
        root.emitOpError()
            << "does not match any arrayLayout or sde.array_layout entry in "
               "the enclosing sde.su_iterate";
        failed = true;
      }
    }

    for (const LayoutGraphFact &fact : facts) {
      std::optional<SdeAccessMode> mode = modeForLayoutRole(fact.role);
      if (!mode)
        continue;
      bool found = hasTypedLayoutFor(fact.id, *mode);
      for (SdeArrayLayoutRootOp root : roots)
        if (static_cast<int64_t>(root.getArrayId()) == fact.id &&
            root.getMode() == *mode)
          found = true;
      if (!found) {
        if (fact.role == LayoutGraphRole::write && hasExternalWriteRoot(*this))
          continue;
        emitOpError() << "arrayLayout entry for arrayId " << fact.id
                      << " has no explicit sde.array_layout_root provenance; "
                         "refusing downstream root/order inference";
        failed = true;
      }
    }

    for (SdeArrayLayoutOp typedLayout : typedLayouts) {
      bool foundRoot = false;
      for (SdeArrayLayoutRootOp root : roots)
        if (static_cast<int64_t>(root.getArrayId()) ==
                static_cast<int64_t>(typedLayout.getArrayId()) &&
            root.getMode() == typedLayout.getMode())
          foundRoot = true;
      if (!foundRoot) {
        typedLayout.emitOpError()
            << "has no matching sde.array_layout_root provenance";
        failed = true;
      }
    }
  }

  for (auto [index, lhs] : llvm::enumerate(roots)) {
    for (SdeArrayLayoutRootOp rhs :
         ArrayRef<SdeArrayLayoutRootOp>(roots).drop_front(index + 1)) {
      if (lhs.getRoot() == rhs.getRoot() &&
          lhs.getArrayId() != rhs.getArrayId()) {
        rhs.emitOpError() << "maps one SDE array root to a conflicting arrayId";
        failed = true;
      }
      if (lhs.getArrayId() == rhs.getArrayId() &&
          lhs.getMode() == rhs.getMode() && lhs.getRoot() != rhs.getRoot()) {
        rhs.emitOpError()
            << "maps one arrayId/role to a different SDE array root";
        failed = true;
      }
    }
  }
  if (failed)
    return failure();

  return success();
}

LogicalResult SdeSuDistributeOp::verify() {
  if (getBody().empty())
    return emitOpError() << "expects body to contain a single block";

  bool failed = false;
  for (Operation &child : getBody().front()) {
    if (isAllowedSuDistributeChild(&child))
      continue;
    child.emitOpError()
        << "is directly inside an sde.su_distribute body; distribution "
           "wrappers may contain only nested SUs, SDE movement ops, or "
           "sde.su_barrier";
    failed = true;
  }
  return failure(failed);
}

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

//===----------------------------------------------------------------------===//
// SdeArrayLayoutRootOp — custom assembly format + verifier
//===----------------------------------------------------------------------===//

// Print: sde.array_layout_root <mode> %root : type(%root) array_id(<N>)
void SdeArrayLayoutRootOp::print(OpAsmPrinter &p) {
  p << " " << stringifySdeAccessMode(getMode()) << " " << getRoot() << " : "
    << getRoot().getType() << " array_id(" << getArrayId() << ")";
  p.printOptionalAttrDict((*this)->getAttrs(), {"mode", "arrayId"});
}

ParseResult SdeArrayLayoutRootOp::parse(OpAsmParser &parser,
                                        OperationState &result) {
  MLIRContext *ctx = parser.getContext();
  IntegerType i64 = IntegerType::get(ctx, 64);

  StringRef modeKw;
  if (parser.parseKeyword(&modeKw))
    return failure();
  std::optional<SdeAccessMode> mode = symbolizeSdeAccessMode(modeKw);
  if (!mode)
    return parser.emitError(parser.getNameLoc(),
                            "expected sde access mode (read|write)");
  result.addAttribute("mode", SdeAccessModeAttr::get(ctx, *mode));

  OpAsmParser::UnresolvedOperand rootOperand;
  Type rootType;
  if (parser.parseOperand(rootOperand) || parser.parseColon() ||
      parser.parseType(rootType) ||
      parser.resolveOperand(rootOperand, rootType, result.operands))
    return failure();

  int64_t arrayId = -1;
  if (parser.parseKeyword("array_id") || parser.parseLParen() ||
      parser.parseInteger(arrayId) || parser.parseRParen())
    return failure();
  result.addAttribute("arrayId", IntegerAttr::get(i64, arrayId));

  return parser.parseOptionalAttrDict(result.attributes);
}

LogicalResult SdeArrayLayoutRootOp::verify() {
  if (getMode() == SdeAccessMode::readwrite)
    return emitOpError("mode must be read or write; readwrite is not a "
                       "role-specific arrayLayout identity");
  if (!isa<MemRefType>(getRoot().getType()))
    return emitOpError("root operand must be a memref");
  if (getArrayId() < 0)
    return emitOpError("arrayId must be non-negative");
  return success();
}

static LogicalResult verifyPositiveDenseI64Shape(SdeArrayLayoutOp op,
                                                 StringRef fieldName,
                                                 ArrayRef<int64_t> values) {
  if (values.empty())
    return op.emitOpError() << fieldName << " must not be empty";
  for (int64_t value : values) {
    if (value <= 0)
      return op.emitOpError()
             << fieldName << " entries must be positive; got " << value;
  }
  return success();
}

static LogicalResult parseDenseI64Array(OpAsmParser &parser,
                                        DenseI64ArrayAttr &attr) {
  if (parser.parseLSquare())
    return failure();
  SmallVector<int64_t> values;
  if (failed(parser.parseOptionalRSquare())) {
    if (parser.parseCommaSeparatedList([&]() -> ParseResult {
          int64_t value = 0;
          if (parser.parseInteger(value))
            return failure();
          values.push_back(value);
          return success();
        }))
      return failure();
    if (parser.parseRSquare())
      return failure();
  }
  attr = DenseI64ArrayAttr::get(parser.getContext(), values);
  return success();
}

static void printDenseI64Array(OpAsmPrinter &p, StringRef keyword,
                               DenseI64ArrayAttr attr) {
  p << keyword << " [";
  llvm::interleaveComma(attr.asArrayRef(), p);
  p << "]";
}

//===----------------------------------------------------------------------===//
// SdeArrayLayoutOp — custom assembly format + verifier
//===----------------------------------------------------------------------===//

// Print: sde.array_layout write array_id(0) owner [0] block [2, 4] logical [8, 4]
void SdeArrayLayoutOp::print(OpAsmPrinter &p) {
  p << " " << stringifySdeAccessMode(getMode()) << " array_id(" << getArrayId()
    << ") ";
  if (getOwnerDimsAttr())
    printDenseI64Array(p, "owner", getOwnerDimsAttr());
  else if (!getOwnerDimValues().empty()) {
    p << "owner (";
    p.printOperands(getOwnerDimValues());
    p << " : ";
    llvm::interleaveComma(getOwnerDimValues(), p,
                          [&](Value v) { p << v.getType(); });
    p << ")";
  }
  p << " ";
  printDenseI64Array(p, "block", getBlockShapeAttr());
  p << " ";
  printDenseI64Array(p, "logical", getLogicalShapeAttr());
  p.printOptionalAttrDict((*this)->getAttrs(),
                          {"mode", "arrayId", "ownerDims", "blockShape",
                           "logicalShape", "operandSegmentSizes"});
}

ParseResult SdeArrayLayoutOp::parse(OpAsmParser &parser,
                                    OperationState &result) {
  MLIRContext *ctx = parser.getContext();
  IntegerType i64 = IntegerType::get(ctx, 64);

  StringRef modeKw;
  if (parser.parseKeyword(&modeKw))
    return failure();
  std::optional<SdeAccessMode> mode = symbolizeSdeAccessMode(modeKw);
  if (!mode)
    return parser.emitError(parser.getNameLoc(),
                            "expected sde access mode (read|write|readwrite)");
  result.addAttribute("mode", SdeAccessModeAttr::get(ctx, *mode));

  int64_t arrayId = -1;
  if (parser.parseKeyword("array_id") || parser.parseLParen() ||
      parser.parseInteger(arrayId) || parser.parseRParen())
    return failure();
  result.addAttribute("arrayId", IntegerAttr::get(i64, arrayId));

  unsigned ownerDimValueCount = 0;
  if (parser.parseKeyword("owner"))
    return failure();
  if (succeeded(parser.parseOptionalLSquare())) {
    SmallVector<int64_t> ownerDims;
    if (failed(parser.parseOptionalRSquare())) {
      if (parser.parseCommaSeparatedList([&]() -> ParseResult {
            int64_t value = 0;
            if (parser.parseInteger(value))
              return failure();
            ownerDims.push_back(value);
            return success();
          }))
        return failure();
      if (parser.parseRSquare())
        return failure();
    }
    result.addAttribute("ownerDims",
                        DenseI64ArrayAttr::get(ctx, ownerDims));
  } else {
    SmallVector<OpAsmParser::UnresolvedOperand> ownerOperands;
    SmallVector<Type> ownerTypes;
    if (parser.parseLParen() ||
        parser.parseOperandList(ownerOperands) || parser.parseColonTypeList(ownerTypes) ||
        parser.parseRParen())
      return failure();
    if (ownerOperands.size() != ownerTypes.size())
      return parser.emitError(parser.getNameLoc(),
                              "owner dim operand/type count mismatch");
    if (parser.resolveOperands(ownerOperands, ownerTypes, parser.getNameLoc(),
                               result.operands))
      return failure();
    ownerDimValueCount = ownerOperands.size();
  }

  DenseI64ArrayAttr blockShape;
  if (parser.parseKeyword("block"))
    return failure();
  if (failed(parseDenseI64Array(parser, blockShape)))
    return failure();
  result.addAttribute("blockShape", blockShape);

  DenseI64ArrayAttr logicalShape;
  if (parser.parseKeyword("logical"))
    return failure();
  if (failed(parseDenseI64Array(parser, logicalShape)))
    return failure();
  result.addAttribute("logicalShape", logicalShape);

  if (parser.parseOptionalAttrDict(result.attributes))
    return failure();
  result.addAttribute(
      "operandSegmentSizes",
      parser.getBuilder().getDenseI32ArrayAttr(
          {static_cast<int32_t>(ownerDimValueCount)}));
  return success();
}

LogicalResult SdeArrayLayoutOp::verify() {
  if (getMode() == SdeAccessMode::readwrite)
    return emitOpError("mode must be read or write; readwrite is not a "
                       "role-specific arrayLayout identity");
  if (getArrayId() < 0)
    return emitOpError("arrayId must be non-negative");

  const bool hasOwnerAttr = static_cast<bool>(getOwnerDimsAttr());
  const bool hasOwnerValues = !getOwnerDimValues().empty();
  if (hasOwnerAttr == hasOwnerValues)
    return emitOpError("exactly one of ownerDims attribute or ownerDimValues "
                       "operands must be present");

  if (hasOwnerAttr) {
    SmallVector<int64_t, 4> seenOwnerDims;
    for (int64_t dim : getOwnerDimsAttr().asArrayRef()) {
      if (dim < 0)
        return emitOpError("ownerDims entries must be non-negative; got ")
               << dim;
      if (llvm::is_contained(seenOwnerDims, dim))
        return emitOpError("ownerDims entries must be unique; duplicate ")
               << dim;
      seenOwnerDims.push_back(dim);
    }
  }

  if (failed(verifyPositiveDenseI64Shape(*this, "blockShape",
                                         getBlockShapeAttr().asArrayRef())))
    return failure();
  if (failed(verifyPositiveDenseI64Shape(*this, "logicalShape",
                                         getLogicalShapeAttr().asArrayRef())))
    return failure();
  return success();
}

///===----------------------------------------------------------------------===///
/// SdeMuAllocOp verifier — dynamic dim count must match `?` in result memref.
///===----------------------------------------------------------------------===///
LogicalResult SdeMuAllocOp::verify() {
  auto memrefTy = cast<MemRefType>(getMemref().getType());
  int64_t numDynamic = memrefTy.getNumDynamicDims();
  if (static_cast<int64_t>(getDynamicSizes().size()) != numDynamic)
    return emitOpError() << "expects " << numDynamic
                         << " dynamic size(s) for result type " << memrefTy
                         << "; got " << getDynamicSizes().size();

  // Folded from the former verify-sde-mu-layout pass (R1c): once this MU is
  // rank-expanded into a block grid, no user may take a rank-reducing view
  // (subview/collapse/cast) that reintroduces a stale logical-rank handle onto
  // the converted block-grid MU. The load/store arity arm of that pass is
  // dropped as redundant with the core memref.load/store verifiers (their index
  // arity must already equal the rank of the same MU memref). Conservative
  // (flat) MUs are skipped: recognizeExpandedBlockGridMu returns nullopt.
  if (recognizeExpandedBlockGridMu(*this)) {
    const int64_t muRank = memrefTy.getRank();
    for (Operation *user : getMemref().getUsers()) {
      if (isa<memref::LoadOp, memref::StoreOp>(user))
        continue;
      for (Value res : user->getResults()) {
        auto resType = dyn_cast<MemRefType>(res.getType());
        if (resType && resType.getRank() < muRank)
          return user->emitOpError()
                 << "rank-reducing view of a rank-expanded block-grid MU "
                    "reintroduces a stale logical-rank access";
      }
    }
  }
  return success();
}

static bool moduleUsesCommittedLayoutFacts(Operation *scope) {
  bool found = false;
  scope->walk([&](SdeSuIterateOp su) {
    if (su.getArrayLayoutAttr())
      found = true;
  });
  return found;
}

static Operation *movementVerificationScope(Operation *op) {
  if (auto func = op->getParentOfType<func::FuncOp>())
    return func;
  if (auto module = op->getParentOfType<ModuleOp>())
    return module.getOperation();
  return op;
}

static LogicalResult
verifyMovementArrayIdIfCommittedLayout(Operation *movement,
                                       IntegerAttr arrayIdAttr) {
  if (arrayIdAttr)
    return success();
  if (!moduleUsesCommittedLayoutFacts(movementVerificationScope(movement)))
    return success();
  return movement->emitOpError()
         << "movement op in committed-layout scope must carry array_id for "
            "SDE array provenance";
}

static bool hasNegative(ArrayRef<int64_t> values) {
  return llvm::any_of(values, [](int64_t value) { return value < 0; });
}

static bool hasNonPositive(ArrayRef<int64_t> values) {
  return llvm::any_of(values, [](int64_t value) { return value <= 0; });
}

static bool ownerDimsFitRank(ArrayRef<int64_t> ownerDims, unsigned rank) {
  return llvm::all_of(ownerDims, [&](int64_t dim) {
    return dim >= 0 && static_cast<unsigned>(dim) < rank;
  });
}

static bool blockShapeFitsType(ArrayRef<int64_t> blockShape,
                               MemRefType memrefType) {
  if (blockShape.size() != static_cast<size_t>(memrefType.getRank()) ||
      hasNonPositive(blockShape))
    return false;
  if (!memrefType.hasStaticShape())
    return true;
  ArrayRef<int64_t> shape = memrefType.getShape();
  for (auto [extent, dimExtent] : llvm::zip_equal(blockShape, shape))
    if (ShapedType::isStatic(dimExtent) && extent > dimExtent)
      return false;
  return true;
}

static LogicalResult verifyMovementEndpointGeometry(
    Operation *movement, Value root, ArrayAttr ownerDimsAttr,
    ArrayAttr blockShapeAttr, std::optional<ArrayAttr> haloShapeAttr) {
  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return movement->emitOpError("redistribution root is not a memref");
  unsigned rank = static_cast<unsigned>(memrefType.getRank());
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(ownerDimsAttr);
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(blockShapeAttr);
  if (!ownerDims)
    return movement->emitOpError("owner dimensions are not a static i64 array");
  if (!blockShape)
    return movement->emitOpError("block shape is not a static i64 array");
  if (!ownerDimsFitRank(*ownerDims, rank))
    return movement->emitOpError(
        "owner dimensions do not fit the redistribution root rank");
  if (!blockShapeFitsType(*blockShape, memrefType))
    return movement->emitOpError(
        "block shape does not fit the redistribution root type");
  if (haloShapeAttr) {
    std::optional<SmallVector<int64_t, 4>> haloShape =
        readI64ArrayAttr(*haloShapeAttr);
    if (!haloShape)
      return movement->emitOpError("haloShape is not a static i64 array");
    if (haloShape->size() != rank || hasNegative(*haloShape))
      return movement->emitOpError(
          "haloShape is not a non-negative rank-length i64 array");
  }
  return success();
}

static bool hasNonZero(ArrayAttr attr) {
  if (!attr)
    return false;
  return llvm::any_of(attr, [](Attribute value) {
    auto integer = dyn_cast<IntegerAttr>(value);
    return integer && integer.getInt() != 0;
  });
}

static bool consumerHasRoot(SdeSuIterateOp consumer, int64_t arrayId,
                            Value movementRoot, SdeAccessMode mode) {
  Value root = carts::ValueAnalysis::stripMemrefViewOps(movementRoot);
  for (SdeArrayLayoutRootOp provenance :
       consumer.getBody().getOps<SdeArrayLayoutRootOp>()) {
    if (static_cast<int64_t>(provenance.getArrayId()) != arrayId ||
        provenance.getMode() != mode)
      continue;
    Value consumerRoot =
        carts::ValueAnalysis::stripMemrefViewOps(provenance.getRoot());
    if (carts::ValueAnalysis::sameMemrefRoot(root, consumerRoot))
      return true;
  }
  return false;
}

static std::optional<LayoutGraphFact>
findConsumerLayoutFact(SdeSuIterateOp consumer, int64_t arrayId,
                       LayoutGraphRole role) {
  if (ArrayAttr layout = consumer.getArrayLayoutAttr())
    for (const LayoutGraphFact &fact : parseArrayLayoutFacts(layout))
      if (fact.id == arrayId && fact.role == role)
        return fact;
  return std::nullopt;
}

static bool consumerHasOwnerReductionAccess(SdeSuIterateOp consumer,
                                            Value movementRoot,
                                            ArrayRef<int64_t> ownerDims) {
  Operation *scope = consumer->getParentOfType<ModuleOp>();
  if (!scope || ownerDims.empty())
    return false;

  ModuleSuAccessRelations relations = buildModuleSuAccessRelations(scope);
  std::optional<unsigned> consumerId;
  for (auto [id, su] : llvm::enumerate(relations.schedulingUnits)) {
    if (su == consumer) {
      consumerId = id;
      break;
    }
  }
  if (!consumerId)
    return false;

  Value root = carts::ValueAnalysis::stripMemrefViewOps(movementRoot);
  const ArrayAccessProfile *profile = nullptr;
  auto direct = relations.profiles.find(root);
  if (direct != relations.profiles.end()) {
    profile = &direct->second;
  } else {
    for (const auto &entry : relations.profiles) {
      if (carts::ValueAnalysis::sameMemrefRoot(entry.first, root)) {
        profile = &entry.second;
        break;
      }
    }
  }
  if (!profile)
    return false;

  for (int64_t ownerDim : ownerDims) {
    if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= profile->rank)
      continue;
    for (const ArrayPositionUse &use : profile->positionUses[ownerDim]) {
      if (use.suId == *consumerId && !use.isWrite &&
          use.kind == ArrayDimKind::reductionIndexed)
        return true;
    }
  }
  return false;
}

static bool consumerHasStorageLocalReduceScatterRead(
    SdeSuIterateOp consumer, int64_t arrayId, Value movementRoot,
    ArrayRef<int64_t> movementOwnerDims, ArrayRef<int64_t> movementBlockShape) {
  std::optional<LayoutGraphFact> readFact =
      findConsumerLayoutFact(consumer, arrayId, LayoutGraphRole::read);
  if (!readFact || readFact->ownerDims.empty() ||
      readFact->blockShape.empty() ||
      movementOwnerDims.size() != readFact->ownerDims.size())
    return false;

  auto rootType = dyn_cast<MemRefType>(movementRoot.getType());
  if (!rootType || !rootType.hasStaticShape() ||
      movementBlockShape.size() != static_cast<size_t>(rootType.getRank()))
    return false;

  const unsigned ownerRank = movementOwnerDims.size();
  if (readFact->blockShape.size() + ownerRank !=
      static_cast<size_t>(rootType.getRank()))
    return false;

  ArrayRef<int64_t> rootShape = rootType.getShape();
  for (unsigned slot = 0; slot < ownerRank; ++slot) {
    if (movementOwnerDims[slot] != static_cast<int64_t>(slot) ||
        rootShape[slot] != 1 || movementBlockShape[slot] != 1)
      return false;
  }
  for (auto [dim, committedExtent] : llvm::enumerate(readFact->blockShape)) {
    unsigned payloadDim = ownerRank + static_cast<unsigned>(dim);
    if (rootShape[payloadDim] != committedExtent ||
        movementBlockShape[payloadDim] != committedExtent)
      return false;
  }
  return true;
}

static SdeSuIterateOp findAnchoredConsumer(Operation *movement) {
  for (Operation *next = movement->getNextNode(); next;
       next = next->getNextNode()) {
    if (isa<SdeSuBarrierOp, SdeSuHaloOp, SdeSuReduceScatterOp, SdeSuAllToAllOp>(
            next))
      continue;
    return dyn_cast<SdeSuIterateOp>(next);
  }
  return {};
}

static bool partialReductionFactsMatchMovement(SdeSuIterateOp consumer,
                                               ArrayRef<int64_t> ownerDims) {
  if (!consumer.getPartialReductionAttr())
    return false;
  std::optional<SmallVector<int64_t, 4>> partialDims =
      readI64ArrayAttr(consumer.getPartialReductionDimsAttr());
  std::optional<SmallVector<int64_t, 4>> partialOwnerDims =
      readI64ArrayAttr(consumer.getPartialReductionOwnerDimsAttr());
  if (!partialDims || partialDims->empty() || !partialOwnerDims ||
      partialOwnerDims->empty())
    return false;
  if (ArrayRef<int64_t>(*partialOwnerDims) != ownerDims)
    return false;

  unsigned scheduleRank = consumer.getLowerBounds().size();
  auto nestedForDepth = [](auto &self, Block *block) -> unsigned {
    if (!block)
      return 0;
    unsigned depth = 0;
    for (Operation &op : block->without_terminator())
      if (auto forOp = dyn_cast<scf::ForOp>(op))
        depth = std::max(depth, 1 + self(self, forOp.getBody()));
    return depth;
  };
  SdeCuRegionOp cu = findSuComputeCuRegion(consumer);
  unsigned loopRank =
      scheduleRank + nestedForDepth(nestedForDepth, cu && !cu.getBody().empty()
                                                        ? &cu.getBody().front()
                                                        : nullptr);
  return llvm::all_of(*partialDims, [&](int64_t dim) {
    return dim >= static_cast<int64_t>(scheduleRank) &&
           dim < static_cast<int64_t>(loopRank);
  });
}

static LogicalResult verifyMovementAnchoredInConsumer(
    Operation *movement, int64_t arrayId, Value movementRoot,
    ArrayRef<int64_t> movementOwnerDims, ArrayRef<int64_t> movementBlockShape,
    bool requireHaloBacking, bool requireReductionBacking) {
  SdeSuIterateOp consumer = findAnchoredConsumer(movement);
  if (!consumer)
    return movement->emitOpError()
           << "movement op is not anchored before a consumer sde.su_iterate";
  bool hasReadRoot =
      consumerHasRoot(consumer, arrayId, movementRoot, SdeAccessMode::read);
  bool hasReductionWriteRoot =
      requireReductionBacking &&
      consumerHasRoot(consumer, arrayId, movementRoot, SdeAccessMode::write);
  if (!hasReadRoot && !hasReductionWriteRoot)
    return movement->emitOpError()
           << "anchored consumer has no matching "
           << (requireReductionBacking ? "read/write" : "read")
           << " provenance for this redistribution root";
  std::optional<LayoutGraphFact> readFact =
      findConsumerLayoutFact(consumer, arrayId, LayoutGraphRole::read);
  std::optional<LayoutGraphFact> writeFact =
      requireReductionBacking
          ? findConsumerLayoutFact(consumer, arrayId, LayoutGraphRole::write)
          : std::nullopt;
  if (!readFact && !writeFact)
    return movement->emitOpError()
           << "anchored consumer has no committed "
           << (requireReductionBacking ? "read/write" : "read")
           << " layout for this "
              "redistribution array";
  if (requireHaloBacking) {
    if (!deriveCommittedHaloShape(consumer) &&
        !hasNonZero(consumer.getAccessMinOffsetsAttr()) &&
        !hasNonZero(consumer.getAccessMaxOffsetsAttr()))
      return movement->emitOpError()
             << "halo movement is not backed by consumer halo/access-window "
                "facts";
  }
  if (requireReductionBacking) {
    const LayoutGraphFact &backingFact = readFact ? *readFact : *writeFact;
    if (backingFact.layoutKind == ArrayLayoutKind::blockContraction ||
        consumerHasOwnerReductionAccess(consumer, movementRoot,
                                        backingFact.ownerDims) ||
        consumerHasStorageLocalReduceScatterRead(
            consumer, arrayId, movementRoot, movementOwnerDims,
            movementBlockShape))
      return success();
    if (consumer.getPartialReductionAttr()) {
      if (partialReductionFactsMatchMovement(consumer, movementOwnerDims))
        return success();
      return movement->emitOpError()
             << "reduce-scatter movement is not backed by committed "
                "partial-reduction dims/owner dims";
    }
    return movement->emitOpError()
           << "reduce-scatter movement is not backed by a contraction/"
              "reduction consumer";
  }
  return success();
}

static LogicalResult verifyMovementGroundedAndAnchored(
    Operation *movement, int64_t arrayId, Value movementRoot,
    ArrayAttr ownerDimsAttr, ArrayAttr blockShapeAttr,
    std::optional<ArrayAttr> haloShapeAttr, bool allowExpandedFull,
    bool requireHaloBacking, bool requireReductionBacking) {
  Operation *scope = movementVerificationScope(movement);
  if (!moduleUsesCommittedLayoutFacts(scope))
    return success();
  auto muType = dyn_cast<MemRefType>(movementRoot.getType());
  if (!muType || !muType.hasStaticShape())
    return success();
  if (failed(verifyMovementEndpointGeometry(movement, movementRoot,
                                            ownerDimsAttr, blockShapeAttr,
                                            haloShapeAttr)))
    return failure();
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(ownerDimsAttr);
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(blockShapeAttr);
  if (!ownerDims || !blockShape)
    return failure();
  std::string reason;
  if (!movementEndpointGroundedInCommittedLayout(scope, movementRoot, arrayId,
                                                 *ownerDims, *blockShape,
                                                 allowExpandedFull, reason))
    return movement->emitOpError()
           << "is not grounded in committed SDE layout: " << reason;
  return verifyMovementAnchoredInConsumer(movement, arrayId, movementRoot,
                                          *ownerDims, *blockShape,
                                          requireHaloBacking,
                                          requireReductionBacking);
}

// Shared endpoint check for SU-scope movement ops: owner dims in range +
// unique; blockShape rank-length (full extent on non-owner dims) or
// owner-dim-length. Movement endpoints must be partitioned (non-empty owner).
static LogicalResult verifySuMovementEndpoint(Operation *op, StringRef name,
                                              MemRefType muType,
                                              ArrayAttr ownerAttr,
                                              ArrayAttr blockAttr) {
  int64_t rank = muType.getRank();
  ArrayRef<int64_t> shape = muType.getShape();
  std::optional<SmallVector<int64_t, 4>> owner = readI64ArrayAttr(ownerAttr);
  std::optional<SmallVector<int64_t, 4>> block = readI64ArrayAttr(blockAttr);
  if (!owner || !block)
    return op->emitOpError()
           << name << ": owner/block must be i64 array attributes";
  if (owner->empty())
    return op->emitOpError()
           << name << ": movement requires a partitioned (non-empty) owner";
  SmallVector<bool, 4> isOwner(rank, false);
  for (int64_t d : *owner) {
    if (d < 0 || d >= rank)
      return op->emitOpError()
             << name << ": owner dim " << d << " out of range";
    if (isOwner[d])
      return op->emitOpError() << name << ": owner dim " << d << " duplicated";
    isOwner[d] = true;
  }
  if (static_cast<int64_t>(block->size()) != rank &&
      static_cast<int64_t>(block->size()) !=
          static_cast<int64_t>(owner->size()))
    return op->emitOpError()
           << name
           << ": blockShape length must equal MU rank or owner-dim count";
  if (static_cast<int64_t>(block->size()) == rank) {
    for (int64_t d = 0; d < rank; ++d) {
      int64_t b = (*block)[d];
      if (b <= 0 || b > shape[d])
        return op->emitOpError() << name << ": block extent " << b
                                 << " out of range on dim " << d;
      if (!isOwner[d] && b != shape[d])
        return op->emitOpError()
               << name
               << ": non-owner block extent must equal full extent on dim "
               << d;
    }
  } else {
    for (auto [i, d] : llvm::enumerate(*owner)) {
      int64_t b = (*block)[i];
      if (b <= 0 || b > shape[d])
        return op->emitOpError() << name << ": block extent " << b
                                 << " out of range on owner dim " << d;
    }
  }
  return success();
}

LogicalResult SdeSuHaloOp::verify() {
  if (!isa_and_nonnull<SdeSuDistributeOp>(getOperation()->getParentOp()))
    return emitOpError(
        "sde.su_halo: movement op must be a direct child of sde.su_distribute");
  auto muType = dyn_cast<MemRefType>(getMu().getType());
  if (!muType)
    return emitOpError("sde.su_halo: mu operand must be a memref");
  if (!muType.hasStaticShape())
    return emitOpError("sde.su_halo: mu must be a static-shape memref");
  if (failed(verifyMovementArrayIdIfCommittedLayout(getOperation(),
                                                    getArrayIdAttr())))
    return failure();
  if (IntegerAttr arrayId = getArrayIdAttr())
    if (arrayId.getInt() < 0)
      return emitOpError(
          "sde.su_halo: arrayId must be non-negative when present");
  if (failed(verifySuMovementEndpoint(*this, "sde.su_halo", muType,
                                      getOwnerDims(), getBlockShape())))
    return failure();
  int64_t rank = muType.getRank();
  std::optional<SmallVector<int64_t, 4>> halo =
      readI64ArrayAttr(getHaloShape());
  if (!halo || static_cast<int64_t>(halo->size()) != rank)
    return emitOpError("sde.su_halo: haloShape length must equal MU rank");
  auto owner = readI64ArrayAttr(getOwnerDims());
  SmallVector<bool, 4> isOwner(rank, false);
  for (int64_t d : *owner)
    isOwner[d] = true;
  bool anyGhost = false;
  for (int64_t d = 0; d < rank; ++d) {
    int64_t v = (*halo)[d];
    if (v < 0)
      return emitOpError("sde.su_halo: haloShape entries must be non-negative");
    if (v != 0 && !isOwner[d])
      return emitOpError() << "sde.su_halo: non-owner dim " << d
                           << " must carry zero ghost width";
    anyGhost |= v != 0;
  }
  if (!anyGhost)
    return emitOpError("sde.su_halo: a zero-radius halo is an identity move");
  if (IntegerAttr arrayId = getArrayIdAttr()) {
    if (failed(verifyMovementGroundedAndAnchored(
            getOperation(), arrayId.getInt(), getMu(), getOwnerDims(),
            getBlockShape(), getHaloShape(),
            /*allowExpandedFull=*/true, /*requireHaloBacking=*/true,
            /*requireReductionBacking=*/false)))
      return failure();
  }
  return success();
}

LogicalResult SdeSuReduceScatterOp::verify() {
  if (!isa_and_nonnull<SdeSuDistributeOp>(getOperation()->getParentOp()))
    return emitOpError("sde.su_reduce_scatter: movement op must be a direct "
                       "child of sde.su_distribute");
  auto muType = dyn_cast<MemRefType>(getMu().getType());
  if (!muType)
    return emitOpError("sde.su_reduce_scatter: mu operand must be a memref");
  if (!muType.hasStaticShape())
    return emitOpError(
        "sde.su_reduce_scatter: mu must be a static-shape memref");
  if (failed(verifyMovementArrayIdIfCommittedLayout(getOperation(),
                                                    getArrayIdAttr())))
    return failure();
  if (IntegerAttr arrayId = getArrayIdAttr())
    if (arrayId.getInt() < 0)
      return emitOpError(
          "sde.su_reduce_scatter: arrayId must be non-negative when present");
  if (failed(verifySuMovementEndpoint(*this, "sde.su_reduce_scatter", muType,
                                      getOwnerDims(), getBlockShape())))
    return failure();
  auto owner = readI64ArrayAttr(getOwnerDims());
  int64_t reduceDim = getReduceDim();
  if (reduceDim < 0 || reduceDim >= static_cast<int64_t>(owner->size()))
    return emitOpError() << "sde.su_reduce_scatter: reduceDim " << reduceDim
                         << " is outside the owner-dim range [0, "
                         << owner->size() << ")";
  if (IntegerAttr arrayId = getArrayIdAttr()) {
    if (failed(verifyMovementGroundedAndAnchored(
            getOperation(), arrayId.getInt(), getMu(), getOwnerDims(),
            getBlockShape(), std::nullopt,
            /*allowExpandedFull=*/false, /*requireHaloBacking=*/false,
            /*requireReductionBacking=*/true)))
      return failure();
  }
  return success();
}

LogicalResult SdeSuAllToAllOp::verify() {
  if (!isa_and_nonnull<SdeSuDistributeOp>(getOperation()->getParentOp()))
    return emitOpError(
        "sde.su_all_to_all: movement op must be a direct child of "
        "sde.su_distribute");
  auto muType = dyn_cast<MemRefType>(getMu().getType());
  if (!muType)
    return emitOpError("sde.su_all_to_all: mu operand must be a memref");
  if (!muType.hasStaticShape())
    return emitOpError("sde.su_all_to_all: mu must be a static-shape memref");
  if (failed(verifyMovementArrayIdIfCommittedLayout(getOperation(),
                                                    getArrayIdAttr())))
    return failure();
  if (IntegerAttr arrayId = getArrayIdAttr())
    if (arrayId.getInt() < 0)
      return emitOpError(
          "sde.su_all_to_all: arrayId must be non-negative when present");
  if (failed(verifySuMovementEndpoint(*this, "sde.su_all_to_all", muType,
                                      getSourceOwnerDims(),
                                      getSourceBlockShape())))
    return failure();
  auto targetOwner = readI64ArrayAttr(getTargetOwnerDims());
  auto targetBlock = readI64ArrayAttr(getTargetBlockShape());
  if (!targetOwner || !targetBlock || targetOwner->empty())
    return emitOpError("sde.su_all_to_all: target owner/block geometry is "
                       "malformed");
  if (failed(verifySuMovementEndpoint(*this, "sde.su_all_to_all", muType,
                                      getTargetOwnerDims(),
                                      getTargetBlockShape())))
    return failure();
  auto sourceOwner = readI64ArrayAttr(getSourceOwnerDims());
  auto sourceBlock = readI64ArrayAttr(getSourceBlockShape());
  if (!sourceOwner || !sourceBlock || sourceOwner->empty())
    return failure();
  if (*sourceOwner == *targetOwner && *sourceBlock == *targetBlock)
    return emitOpError("sde.su_all_to_all: source and target geometry must "
                       "differ for a genuine repartition");
  if (IntegerAttr arrayId = getArrayIdAttr()) {
    if (failed(verifyMovementGroundedAndAnchored(
            getOperation(), arrayId.getInt(), getMu(), getSourceOwnerDims(),
            getSourceBlockShape(), std::nullopt,
            /*allowExpandedFull=*/true, /*requireHaloBacking=*/false,
            /*requireReductionBacking=*/false)))
      return failure();
  }
  return success();
}

///===----------------------------------------------------------------------===///
/// SdeMuTokenOp verifier.
///===----------------------------------------------------------------------===///
LogicalResult SdeMuTokenOp::verify() {
  auto sourceTy = cast<MemRefType>(getSource().getType());
  auto tokenTy = cast<TokenType>(getToken().getType());
  MemRefType sliceTy = tokenTy.getSliceType();

  if (sourceTy.getElementType() != sliceTy.getElementType()) {
    return emitOpError()
           << "expects token slice_type element type to match source type ("
           << sourceTy << " vs " << sliceTy << ")";
  }

  auto offsets = getOffsets();
  auto sizes = getSizes();

  // Offsets and sizes must either both be empty (whole-storage token) or both
  // be supplied.
  if (offsets.size() != sizes.size()) {
    return emitOpError()
           << "expects offsets and sizes to have the same count (got "
           << offsets.size() << " offsets, " << sizes.size() << " sizes)";
  }

  int64_t rank = sourceTy.getRank();
  if (sliceTy.getRank() != rank) {
    return emitOpError() << "expects token slice_type rank ("
                         << sliceTy.getRank() << ") to match source rank ("
                         << rank << ")";
  }

  if (offsets.empty()) {
    if (sliceTy != sourceTy) {
      return emitOpError()
             << "expects whole-storage token slice_type to match source type ("
             << sourceTy << "), got " << sliceTy;
    }
    return success();
  }

  // Rank match between offsets/sizes and source storage.
  if (static_cast<int64_t>(offsets.size()) != rank) {
    return emitOpError() << "expects offsets/sizes count (" << offsets.size()
                         << ") to match source rank (" << rank << ")";
  }

  // Static sizes must be non-negative; when both offset and size are constants
  // for a dimension, offset + size <= source_dim.
  ArrayRef<int64_t> sourceShape = sourceTy.getShape();
  ArrayRef<int64_t> sliceShape = sliceTy.getShape();
  for (int64_t i = 0; i < rank; ++i) {
    APInt sizeVal;
    bool sizeIsConst = matchPattern(sizes[i], m_ConstantInt(&sizeVal));
    if (sizeIsConst && sizeVal.isNegative()) {
      return emitOpError() << "expects non-negative size at dimension " << i
                           << " (got " << sizeVal.getSExtValue() << ")";
    }

    APInt offsetVal;
    bool offsetIsConst = matchPattern(offsets[i], m_ConstantInt(&offsetVal));

    if (sizeIsConst && !ShapedType::isDynamic(sliceShape[i]) &&
        sliceShape[i] != sizeVal.getSExtValue()) {
      return emitOpError() << "expects token slice_type dimension " << i << " ("
                           << sliceShape[i] << ") to match static token size "
                           << sizeVal.getSExtValue();
    }

    if (sizeIsConst && offsetIsConst &&
        !ShapedType::isDynamic(sourceShape[i])) {
      int64_t off = offsetVal.getSExtValue();
      int64_t sz = sizeVal.getSExtValue();
      if (off < 0) {
        return emitOpError() << "expects non-negative offset at dimension " << i
                             << " (got " << off << ")";
      }
      if (off + sz > sourceShape[i]) {
        return emitOpError()
               << "slice at dimension " << i << " (offset=" << off
               << ", size=" << sz
               << ") exceeds source bound (dim=" << sourceShape[i] << ")";
      }
    }
  }

  return success();
}

///===----------------------------------------------------------------------===///
/// SdeCuWorkOp verifier.
///===----------------------------------------------------------------------===///
LogicalResult SdeCuWorkOp::verify() {
  auto tokens = getTokens();
  auto captures = getCaptures();

  auto isScalarCaptureType = [](Type type) {
    return type.isIntOrIndexOrFloat();
  };

  // Every token operand must be `!sde.token<memref<...>>`.
  SmallVector<TokenType> tokenTypes;
  tokenTypes.reserve(tokens.size());
  for (auto token : tokens) {
    auto tt = llvm::dyn_cast<TokenType>(token.getType());
    if (!tt)
      return emitOpError() << "expects every operand to be of type !sde.token";
    tokenTypes.push_back(tt);
  }

  // Body has exactly one block.
  if (getBody().empty())
    return emitOpError() << "expects body to contain a single block";
  Block &entry = getBody().front();

  // Block arguments first mirror token slice types, then scalar captures.
  unsigned expectedArgs = tokens.size() + captures.size();
  if (entry.getNumArguments() != expectedArgs) {
    return emitOpError() << "expects " << expectedArgs << " block argument(s) ("
                         << tokens.size() << " token + " << captures.size()
                         << " capture); got " << entry.getNumArguments();
  }
  for (auto [idx, tt] : llvm::enumerate(tokenTypes)) {
    Type argTy = entry.getArgument(idx).getType();
    Type sliceTy = tt.getSliceType();
    if (argTy != sliceTy) {
      return emitOpError() << "block argument #" << idx << " type (" << argTy
                           << ") does not match token slice type (" << sliceTy
                           << ")";
    }
  }
  for (auto [idx, capture] : llvm::enumerate(captures)) {
    if (!isScalarCaptureType(capture.getType()))
      return emitOpError()
             << "capture operand #" << idx
             << " must be an integer, index, or float scalar; got "
             << capture.getType();

    unsigned argIdx = tokens.size() + idx;
    Type argTy = entry.getArgument(argIdx).getType();
    if (argTy != capture.getType()) {
      return emitOpError() << "capture block argument #" << idx << " type ("
                           << argTy << ") does not match capture operand type ("
                           << capture.getType() << ")";
    }
  }

  // Terminator is sde.yield with no values. Memref compute units update through
  // token block arguments directly.
  auto yield = llvm::dyn_cast_or_null<SdeYieldOp>(entry.getTerminator());
  if (!yield)
    return emitOpError() << "expects body to terminate with sde.yield";
  if (!yield.getValues().empty())
    return emitOpError()
           << "expects memref compute-unit yield to carry no values";
  if (failed(verifyCuContainsNoScheduling(getOperation())))
    return failure();

  // Best-effort check for conflicting modes on statically-overlapping slices
  // of the same source storage value. Non-constant slices are delegated to
  // runtime.
  auto modeKind = [](SdeAccessMode m) {
    // 0 = read, 1 = write-ish (write or readwrite).
    return (m == SdeAccessMode::read) ? 0 : 1;
  };

  auto constantSlice = [](SdeMuTokenOp op, SmallVectorImpl<int64_t> &offs,
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
    // Whole-storage tokens (empty offsets/sizes) overlap everything from the
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
                  "slices of the same source storage";
      }
    }
  }

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
                            "sde.yield of the reduction type";

  return success();
}
