///==========================================================================///
/// File: SdeOps.cpp
/// Defines SDE dialect operation helpers and verifiers.
///==========================================================================///

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdeCuStructure.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/OpImplementation.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::sde;

#define GET_OP_CLASSES
#include "carts/dialect/sde/IR/SdeOps.cpp.inc"

namespace {

static bool isAllowedSuIterateChild(Operation *op) {
  return isa<SdeYieldOp, SdeCuRegionOp, SdeCuAtomicOp>(op) ||
         isa<SdeArrayLayoutRootOp, SdeSuBarrierOp>(op);
}

static bool isAllowedSuDistributeChild(Operation *op) {
  return sde::isSuOp(op) || isa<SdeRedistOp, SdeSuBarrierOp>(op);
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

  bool failed = false;
  getBody().walk([&](Operation *nested) {
    if (nested == getOperation())
      return WalkResult::advance();
    if (!sde::isCuForbiddenSchedulingOp(nested))
      return WalkResult::advance();
    nested->emitOpError()
        << "is nested inside an sde.cu_region body; compute units are "
           "executable leaves and SU scheduling must be represented outside "
           "the CU";
    failed = true;
    return WalkResult::advance();
  });
  if (failed)
    return failure();

  return success();
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

  // schedule(<kind>[, %chunk])
  if (auto sched = getSchedule()) {
    p << " schedule(<" << stringifySdeScheduleKind(*sched) << ">";
    if (getChunkSize())
      p << ", " << getChunkSize();
    p << ")";
  }

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

  // reduction_strategy(<strategy>)
  if (auto strategy = getReductionStrategy()) {
    p << " reduction_strategy(<" << stringifySdeReductionStrategy(*strategy)
      << ">)";
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
      "schedule",
      "nowait",
      "reductionKinds",
      "reductionStrategy",
      "structuredClassification",
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

  // ---- optional schedule(<kind>[, %chunk]) ----
  bool hasChunkSize = false;
  if (succeeded(parser.parseOptionalKeyword("schedule"))) {
    SdeScheduleKindAttr schedAttr;
    if (parser.parseLParen() ||
        parser.parseCustomAttributeWithFallback(schedAttr, Type{}, "schedule",
                                                result.attributes))
      return failure();
    if (succeeded(parser.parseOptionalComma())) {
      OpAsmParser::UnresolvedOperand chunkOp;
      if (parser.parseOperand(chunkOp) ||
          parser.resolveOperand(chunkOp, indexType, result.operands))
        return failure();
      hasChunkSize = true;
    }
    if (parser.parseRParen())
      return failure();
  }

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

  // ---- optional reduction_strategy(<strategy>) ----
  if (succeeded(parser.parseOptionalKeyword("reduction_strategy"))) {
    SdeReductionStrategyAttr stratAttr;
    if (parser.parseLParen() ||
        parser.parseCustomAttributeWithFallback(
            stratAttr, Type{}, "reductionStrategy", result.attributes) ||
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
  // Order: lowerBounds | upperBounds | steps | chunkSize? | redAccs+iterArgs
  SmallVector<int32_t> segmentSizes = {
      static_cast<int32_t>(numDims), static_cast<int32_t>(numDims),
      static_cast<int32_t>(numDims), hasChunkSize ? 1 : 0,
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
  if (getChunkSize() && !getScheduleAttr())
    return emitOpError() << "chunk size requires an explicit schedule";

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

  ArrayAttr layout = getArrayLayoutAttr();
  SmallVector<LayoutGraphFact, 4> facts;
  if (layout)
    facts = parseArrayLayoutFacts(layout);

  SmallVector<SdeArrayLayoutRootOp, 4> roots;
  for (SdeArrayLayoutRootOp root : entry.getOps<SdeArrayLayoutRootOp>())
    roots.push_back(root);

  if (!layout && !roots.empty()) {
    roots.front().emitOpError()
        << "commits array root provenance but the enclosing sde.su_iterate "
           "has no arrayLayout";
    failed = true;
  }

  for (SdeArrayLayoutRootOp root : roots) {
    bool matchesLayout = false;
    for (const LayoutGraphFact &fact : facts) {
      std::optional<SdeAccessMode> mode = modeForLayoutRole(fact.role);
      if (mode && fact.id == static_cast<int64_t>(root.getArrayId()) &&
          *mode == root.getMode()) {
        matchesLayout = true;
        break;
      }
    }
    if (!matchesLayout) {
      root.emitOpError()
          << "does not match any arrayLayout entry in the enclosing "
             "sde.su_iterate";
      failed = true;
    }
  }

  for (const LayoutGraphFact &fact : facts) {
    std::optional<SdeAccessMode> mode = modeForLayoutRole(fact.role);
    if (!mode)
      continue;
    bool found = false;
    for (SdeArrayLayoutRootOp root : roots)
      if (static_cast<int64_t>(root.getArrayId()) == fact.id &&
          root.getMode() == *mode)
        found = true;
    if (!found) {
      emitOpError() << "arrayLayout entry for arrayId " << fact.id
                    << " has no explicit sde.array_layout_root provenance; "
                       "refusing downstream root/order inference";
      failed = true;
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

  if (auto topology = getIterationTopology();
      topology && (*topology == SdeIterationTopology::owner_tile ||
                   *topology == SdeIterationTopology::owner_tile_2d)) {
    if (getLowerBounds().size() < 2 || getUpperBounds().size() < 2 ||
        getSteps().size() < 2)
      return emitOpError()
             << "owner-tile iteration topology requires at least two SDE "
                "loop dimensions";

    auto logicalSlice = readI64ArrayAttr(getLogicalWorkerSliceAttr());
    if (!logicalSlice)
      return emitOpError()
             << "owner-tile iteration topology requires logicalWorkerSlice";
    unsigned positiveSliceDims = 0;
    for (int64_t extent : *logicalSlice)
      if (extent > 0)
        ++positiveSliceDims;
    if (positiveSliceDims < 2)
      return emitOpError()
             << "owner-tile iteration topology requires at least two positive "
                "logicalWorkerSlice entries";

    auto physicalOwnerDims = readI64ArrayAttr(getPhysicalOwnerDimsAttr());
    if (!physicalOwnerDims || physicalOwnerDims->size() < 2)
      return emitOpError()
             << "owner-tile iteration topology requires at least two "
                "physicalOwnerDims entries";
  }

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
           "wrappers may contain only nested SUs, sde.redist, or "
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
  if (IntegerAttr arrayId = getArrayIdAttr())
    if (arrayId.getInt() < 0)
      return emitOpError("arrayId must be non-negative when present");
  return success();
}

//===----------------------------------------------------------------------===//
// SdeMuAccessWindowOp — custom assembly format + verifier
//===----------------------------------------------------------------------===//

// Print: sde.mu_access_window <mode> %mu : type(%mu)
//        owner_dims(<N>) block_lo [..] block_hi [..] valid [..] [attr-dict]
void SdeMuAccessWindowOp::print(OpAsmPrinter &p) {
  p << " " << stringifySdeAccessMode(getMode()) << " " << getMu() << " : "
    << getMu().getType();
  if (IntegerAttr arrayId = getArrayIdAttr())
    p << " array_id(" << arrayId.getInt() << ")";
  p << " owner_dims(" << getOwnerDimCount() << ")";
  auto printArr = [&](StringRef kw, ArrayAttr arr) {
    p << " " << kw << " [";
    llvm::interleaveComma(
        arr, p, [&](Attribute a) { p << cast<IntegerAttr>(a).getInt(); });
    p << "]";
  };
  printArr("block_lo", getBlockLo());
  printArr("block_hi", getBlockHi());
  printArr("valid", getValidExtents());
  p.printOptionalAttrDict((*this)->getAttrs(),
                          {"mode", "arrayId", "ownerDimCount", "blockLo",
                           "blockHi", "validExtents"});
}

ParseResult SdeMuAccessWindowOp::parse(OpAsmParser &parser,
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

  OpAsmParser::UnresolvedOperand muOperand;
  Type muType;
  if (parser.parseOperand(muOperand) || parser.parseColon() ||
      parser.parseType(muType) ||
      parser.resolveOperand(muOperand, muType, result.operands))
    return failure();

  if (succeeded(parser.parseOptionalKeyword("array_id"))) {
    int64_t arrayId = -1;
    if (parser.parseLParen() || parser.parseInteger(arrayId) ||
        parser.parseRParen())
      return failure();
    result.addAttribute("arrayId", IntegerAttr::get(i64, arrayId));
  }

  int64_t ownerDimCount = 0;
  if (parser.parseKeyword("owner_dims") || parser.parseLParen() ||
      parser.parseInteger(ownerDimCount) || parser.parseRParen())
    return failure();
  result.addAttribute("ownerDimCount", IntegerAttr::get(i64, ownerDimCount));

  auto parseArr = [&](StringRef kw, StringRef attrName) -> ParseResult {
    if (parser.parseKeyword(kw) || parser.parseLSquare())
      return failure();
    SmallVector<Attribute> vals;
    if (failed(parser.parseOptionalRSquare())) {
      do {
        int64_t v;
        if (parser.parseInteger(v))
          return failure();
        vals.push_back(IntegerAttr::get(i64, v));
      } while (succeeded(parser.parseOptionalComma()));
      if (parser.parseRSquare())
        return failure();
    }
    result.addAttribute(attrName, ArrayAttr::get(ctx, vals));
    return success();
  };
  if (parseArr("block_lo", "blockLo") || parseArr("block_hi", "blockHi") ||
      parseArr("valid", "validExtents"))
    return failure();

  return parser.parseOptionalAttrDict(result.attributes);
}

LogicalResult SdeMuAccessWindowOp::verify() {
  auto muType = dyn_cast<MemRefType>(getMu().getType());
  if (!muType)
    return emitOpError("sde.mu_access_window: mu operand must be a memref");
  if (!getMu().getDefiningOp<SdeMuAllocOp>())
    return emitOpError(
        "sde.mu_access_window: mu operand must be defined by sde.mu_alloc");
  if (IntegerAttr arrayId = getArrayIdAttr())
    if (arrayId.getInt() < 0)
      return emitOpError(
          "sde.mu_access_window: arrayId must be non-negative when present");

  int64_t ownerDimCount = static_cast<int64_t>(getOwnerDimCount());
  if (ownerDimCount < 1)
    return emitOpError("sde.mu_access_window: ownerDimCount must be >= 1");
  if (ownerDimCount >= muType.getRank())
    return emitOpError("sde.mu_access_window: ownerDimCount must be < MU rank");

  std::optional<SmallVector<int64_t, 4>> blockLo =
      readI64ArrayAttr(getBlockLo());
  std::optional<SmallVector<int64_t, 4>> blockHi =
      readI64ArrayAttr(getBlockHi());
  std::optional<SmallVector<int64_t, 4>> valid =
      readI64ArrayAttr(getValidExtents());
  if (!blockLo || !blockHi || !valid)
    return emitOpError("sde.mu_access_window: block_lo/block_hi/valid must be "
                       "i64 array attributes");

  if (static_cast<int64_t>(blockLo->size()) != ownerDimCount ||
      static_cast<int64_t>(blockHi->size()) != ownerDimCount)
    return emitOpError("sde.mu_access_window: blockLo/blockHi length must "
                       "equal ownerDimCount");

  int64_t logicalRank = muType.getRank() - ownerDimCount;
  if (static_cast<int64_t>(valid->size()) != logicalRank)
    return emitOpError(
        "sde.mu_access_window: validExtents length must equal logicalRank");

  ArrayRef<int64_t> eshape = muType.getShape();
  for (int64_t k = 0; k < ownerDimCount; ++k) {
    if ((*blockLo)[k] < 0 || (*blockLo)[k] >= (*blockHi)[k])
      return emitOpError()
             << "sde.mu_access_window: empty or inverted block range on owner "
                "dim "
             << k;
    if ((*blockHi)[k] > eshape[k])
      return emitOpError() << "sde.mu_access_window: blockHi[" << k
                           << "]=" << (*blockHi)[k] << " exceeds MU grid dim "
                           << eshape[k];
  }
  for (int64_t d = 0; d < logicalRank; ++d) {
    int64_t tileDim = eshape[ownerDimCount + d];
    if ((*valid)[d] < 1 || (*valid)[d] > tileDim)
      return emitOpError() << "sde.mu_access_window: validExtents[" << d
                           << "]=" << (*valid)[d] << " out of range [1, "
                           << tileDim << "]";
  }

  // The window describes one CU: it must sit directly in a sde.cu_region body.
  if (Block *blk = getOperation()->getBlock())
    if (!isa_and_nonnull<SdeCuRegionOp>(blk->getParentOp()))
      return emitOpError("sde.mu_access_window: op must be directly inside a "
                         "sde.cu_region body");
  return success();
}

//===----------------------------------------------------------------------===//
// SdeRedistOp — custom assembly format + verifier
//===----------------------------------------------------------------------===//

// Print: sde.redist <family> %mu : type(%mu)
//        from owner [..] block [..] to owner [..] block [..]
//        [halo [..]] [cost N] [attr-dict]
void SdeRedistOp::print(OpAsmPrinter &p) {
  auto printArr = [&](StringRef kw, ArrayAttr arr) {
    p << " " << kw << " [";
    llvm::interleaveComma(
        arr, p, [&](Attribute a) { p << cast<IntegerAttr>(a).getInt(); });
    p << "]";
  };
  p << " <" << stringifySdeMovementFamily(getFamily()) << "> " << getMu()
    << " : " << getMu().getType();
  if (IntegerAttr arrayId = getArrayIdAttr())
    p << " array_id(" << arrayId.getInt() << ")";
  p << " from";
  printArr("owner", getSourceOwnerDims());
  printArr("block", getSourceBlockShape());
  p << " to";
  printArr("owner", getTargetOwnerDims());
  printArr("block", getTargetBlockShape());
  if (ArrayAttr halo = getHaloShapeAttr())
    printArr("halo", halo);
  if (IntegerAttr cost = getCommVolumeBytesAttr())
    p << " cost " << cost.getInt();
  p.printOptionalAttrDict((*this)->getAttrs(),
                          {"family", "arrayId", "sourceOwnerDims",
                           "sourceBlockShape", "targetOwnerDims",
                           "targetBlockShape", "haloShape", "commVolumeBytes"});
}

ParseResult SdeRedistOp::parse(OpAsmParser &parser, OperationState &result) {
  MLIRContext *ctx = parser.getContext();
  IntegerType i64 = IntegerType::get(ctx, 64);

  StringRef famKw;
  if (parser.parseLess() || parser.parseKeyword(&famKw) ||
      parser.parseGreater())
    return failure();
  std::optional<SdeMovementFamily> fam = symbolizeSdeMovementFamily(famKw);
  if (!fam)
    return parser.emitError(parser.getNameLoc(),
                            "expected sde movement family");
  result.addAttribute("family", SdeMovementFamilyAttr::get(ctx, *fam));

  OpAsmParser::UnresolvedOperand muOperand;
  Type muType;
  if (parser.parseOperand(muOperand) || parser.parseColon() ||
      parser.parseType(muType) ||
      parser.resolveOperand(muOperand, muType, result.operands))
    return failure();

  if (succeeded(parser.parseOptionalKeyword("array_id"))) {
    int64_t arrayId = -1;
    if (parser.parseLParen() || parser.parseInteger(arrayId) ||
        parser.parseRParen())
      return failure();
    result.addAttribute("arrayId", IntegerAttr::get(i64, arrayId));
  }

  // Parse a bare `[i64, ...]` list (no leading keyword).
  auto parseList = [&](SmallVectorImpl<Attribute> &vals) -> ParseResult {
    if (parser.parseLSquare())
      return failure();
    if (failed(parser.parseOptionalRSquare())) {
      do {
        int64_t v;
        if (parser.parseInteger(v))
          return failure();
        vals.push_back(IntegerAttr::get(i64, v));
      } while (succeeded(parser.parseOptionalComma()));
      if (parser.parseRSquare())
        return failure();
    }
    return success();
  };
  auto parseKwArr = [&](StringRef kw, StringRef name) -> ParseResult {
    SmallVector<Attribute> vals;
    if (parser.parseKeyword(kw) || parseList(vals))
      return failure();
    result.addAttribute(name, ArrayAttr::get(ctx, vals));
    return success();
  };

  if (parser.parseKeyword("from") || parseKwArr("owner", "sourceOwnerDims") ||
      parseKwArr("block", "sourceBlockShape"))
    return failure();
  if (parser.parseKeyword("to") || parseKwArr("owner", "targetOwnerDims") ||
      parseKwArr("block", "targetBlockShape"))
    return failure();

  if (succeeded(parser.parseOptionalKeyword("halo"))) {
    SmallVector<Attribute> vals;
    if (parseList(vals))
      return failure();
    result.addAttribute("haloShape", ArrayAttr::get(ctx, vals));
  }
  if (succeeded(parser.parseOptionalKeyword("cost"))) {
    int64_t cost;
    if (parser.parseInteger(cost))
      return failure();
    result.addAttribute("commVolumeBytes", IntegerAttr::get(i64, cost));
  }

  return parser.parseOptionalAttrDict(result.attributes);
}

LogicalResult SdeRedistOp::verify() {
  auto muType = dyn_cast<MemRefType>(getMu().getType());
  if (!muType)
    return emitOpError("sde.redist: mu operand must be a memref");
  if (!muType.hasStaticShape())
    return emitOpError(
        "sde.redist: dynamic MU shape has no static redistribution layout");
  if (IntegerAttr arrayId = getArrayIdAttr()) {
    if (arrayId.getInt() < 0)
      return emitOpError(
          "sde.redist: arrayId must be non-negative when present");
  }
  int64_t rank = muType.getRank();
  ArrayRef<int64_t> shape = muType.getShape();

  // One endpoint: owner dims in range + unique; blockShape either rank-length
  // (full extent on non-owner dims) or owner-dim-length. Empty owner dims is a
  // replicated endpoint.
  auto checkEndpoint = [&](StringRef side, ArrayAttr ownerAttr,
                           ArrayAttr blockAttr) -> LogicalResult {
    std::optional<SmallVector<int64_t, 4>> owner = readI64ArrayAttr(ownerAttr);
    std::optional<SmallVector<int64_t, 4>> block = readI64ArrayAttr(blockAttr);
    if (!owner || !block)
      return emitOpError() << "sde.redist: " << side
                           << " owner/block must be i64 array attributes";
    SmallVector<bool, 4> isOwner(rank, false);
    for (int64_t d : *owner) {
      if (d < 0 || d >= rank)
        return emitOpError() << "sde.redist: " << side << " owner dim " << d
                             << " out of range";
      if (isOwner[d])
        return emitOpError()
               << "sde.redist: " << side << " owner dim " << d << " duplicated";
      isOwner[d] = true;
    }
    if (static_cast<int64_t>(block->size()) != rank &&
        static_cast<int64_t>(block->size()) !=
            static_cast<int64_t>(owner->size()))
      return emitOpError() << "sde.redist: " << side
                           << " blockShape length must equal MU rank or "
                              "owner-dim count";
    if (static_cast<int64_t>(block->size()) == rank) {
      for (int64_t d = 0; d < rank; ++d) {
        int64_t b = (*block)[d];
        if (b <= 0 || b > shape[d])
          return emitOpError() << "sde.redist: " << side << " block extent "
                               << b << " out of range on dim " << d;
        if (!isOwner[d] && b != shape[d])
          return emitOpError() << "sde.redist: " << side
                               << " non-owner block extent must equal full "
                                  "extent on dim "
                               << d;
      }
    } else {
      for (auto [i, d] : llvm::enumerate(*owner)) {
        int64_t b = (*block)[i];
        if (b <= 0 || b > shape[d])
          return emitOpError() << "sde.redist: " << side << " block extent "
                               << b << " out of range on owner dim " << d;
      }
    }
    return success();
  };
  if (failed(checkEndpoint("source", getSourceOwnerDims(),
                           getSourceBlockShape())) ||
      failed(
          checkEndpoint("target", getTargetOwnerDims(), getTargetBlockShape())))
    return failure();

  bool sourceReplicated = getSourceOwnerDims().empty();
  bool targetReplicated = getTargetOwnerDims().empty();
  SdeMovementFamily fam = getFamily();

  // Halo is the halo_like family's signature and nothing else's.
  if (ArrayAttr halo = getHaloShapeAttr()) {
    if (fam != SdeMovementFamily::halo_like)
      return emitOpError(
          "sde.redist: haloShape is only valid for the halo_like family");
    std::optional<SmallVector<int64_t, 4>> h = readI64ArrayAttr(halo);
    if (!h || static_cast<int64_t>(h->size()) != rank)
      return emitOpError("sde.redist: haloShape length must equal MU rank");
    for (int64_t v : *h)
      if (v < 0)
        return emitOpError(
            "sde.redist: haloShape entries must be non-negative");
  } else if (fam == SdeMovementFamily::halo_like) {
    return emitOpError("sde.redist: halo_like family requires a haloShape");
  }

  // Family <-> geometry consistency. Reduction families may keep an identical
  // layout (the movement is the reduction); re-layout families must differ.
  bool sameLayout = getSourceOwnerDims() == getTargetOwnerDims() &&
                    getSourceBlockShape() == getTargetBlockShape();
  switch (fam) {
  case SdeMovementFamily::broadcast_like:
  case SdeMovementFamily::all_gather_like:
    if (!targetReplicated)
      return emitOpError("sde.redist: broadcast_like/all_gather_like require a "
                         "replicated target (no owner dims)");
    break;
  case SdeMovementFamily::reduce_scatter_like:
  case SdeMovementFamily::allreduce_like:
    if (sourceReplicated)
      return emitOpError(
          "sde.redist: reduction families require a partitioned source");
    break;
  case SdeMovementFamily::all_to_all_like:
    if (sourceReplicated || targetReplicated)
      return emitOpError(
          "sde.redist: all_to_all_like requires partitioned source and target");
    if (getSourceOwnerDims() == getTargetOwnerDims())
      return emitOpError(
          "sde.redist: all_to_all_like requires differing owner dims");
    break;
  case SdeMovementFamily::halo_like:
    if (getSourceOwnerDims() != getTargetOwnerDims())
      return emitOpError(
          "sde.redist: halo_like requires identical source/target owner dims");
    break;
  case SdeMovementFamily::phase_redist:
    if (sameLayout)
      return emitOpError(
          "sde.redist: phase_redist requires source and target layouts to "
          "differ");
    break;
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
