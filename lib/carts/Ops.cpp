#include "arts/Ops.h"
#include "arts/Dialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/OpImplementation.h"  // for custom parse/print

// Auto-generated op definitions from ARTSOps.td:
#include "arts/ARTSOps.cpp.inc"

using namespace mlir;
using namespace mlir::arts;

//
// 1) Example: Implementation of a build function for EdtOp
//
void arts::buildEdtOp(OpBuilder &builder, OperationState &state,
                      ArrayAttr parameters, ArrayAttr dependencies) {
  // You could store these arrays as attributes:
  state.addAttribute("parameters", parameters);
  state.addAttribute("dependencies", dependencies);

  // The op has one region for the body:
  Region *bodyRegion = state.addRegion();
  // Typically, you might create a block in that region if you want an immediate body
  // but often you leave it to the user or a pattern to add the block later.
}

//
// 2) ARTS_EdtOp parse/print
//    If you declared "def ARTS_EdtOp : ARTS_Op<"edt">" in TableGen
//    and want to override the default parse/print, do it here.
//

// Custom Parser for EdtOp
ParseResult ARTS_EdtOp::parse(OpAsmParser &parser, OperationState &result) {
  // Syntax we desire: 
  //   arts.edt parameters(...stuff...) dependencies(...stuff...) {
  //       <body region>
  //   }

  // 1) parse literal "parameters("
  if (parser.parseKeyword("parameters"))
    return failure();
  if (parser.parseLParen())
    return failure();

  // 2) parse an optional list of anything you want to store in "parameters" 
  //    e.g. parse array of symbols, or parse a custom form. 
  //    For simplicity, let's parse an ArrayAttr of strings
  SmallVector<Attribute, 4> paramList;
  // You might do a loop or parseCommaSeparatedList. For example:
  if (parser.parseOptionalRParen()) {
    // we have something
    // parse a list of string-literals or symbol references
    while (true) {
      // parse a string attribute
      // e.g. parse a string-literal
      // (Below is a simplistic approach; real code might do more checks)
      Attribute strAttr;
      if (parser.parseAttribute(strAttr))
        return failure();
      paramList.push_back(strAttr);
      // check if we are done or see a comma
      if (succeeded(parser.parseOptionalComma()))
        continue; // parse next
      break;
    }
    // finally parse ')'
    if (parser.parseRParen())
      return failure();
  } else {
    // means we had an immediate ')', so no parameters
  }
  auto paramAttr = parser.getBuilder().getArrayAttr(paramList);
  result.addAttribute("parameters", paramAttr);

  // 3) parse literal "dependencies("
  if (parser.parseKeyword("dependencies"))
    return failure();
  if (parser.parseLParen())
    return failure();
  // parse array of dictionary attrs or something
  SmallVector<Attribute, 4> depsList;
  // let's say we parse dictionary attributes separated by commas
  if (failed(parser.parseOptionalRParen())) {
    // parse at least one
    while (true) {
      DictionaryAttr dictAttr;
      if (parser.parseAttribute(dictAttr))
        return failure();
      depsList.push_back(dictAttr);
      if (succeeded(parser.parseOptionalComma()))
        continue;
      break;
    }
    if (parser.parseRParen())
      return failure();
  }
  auto depsAttr = parser.getBuilder().getArrayAttr(depsList);
  result.addAttribute("dependencies", depsAttr);

  // 4) parse region body
  Region *body = result.addRegion();
  if (parser.parseRegion(*body))
    return failure();

  // Let the user optionally add a block in the body, or we can do it ourselves:
  ARTS_EdtOp::ensureTerminator(*body, parser.getBuilder(), result.location);

  return success();
}

// Custom Printer for EdtOp
void ARTS_EdtOp::print(OpAsmPrinter &printer) {
  // We want to print:
  //   arts.edt parameters(...) dependencies(...) <region>
  printer << "arts.edt parameters(";

  // Print the parameters array
  auto paramArray = (*this)->getAttrOfType<ArrayAttr>("parameters");
  if (paramArray && !paramArray.empty()) {
    // print them comma-separated
    bool first = true;
    for (auto attr : paramArray) {
      if (!first) printer << ", ";
      printer.printAttribute(attr);
      first = false;
    }
  }
  printer << ") dependencies(";

  // Print the dependencies array
  auto depsArray = (*this)->getAttrOfType<ArrayAttr>("dependencies");
  if (depsArray && !depsArray.empty()) {
    bool first = true;
    for (auto dict : depsArray) {
      if (!first) printer << ", ";
      printer.printAttribute(dict);
      first = false;
    }
  }
  printer << ") ";

  // Print the region
  printer.printRegion(getBodyRegion(), /*printEntryBlockArgs=*/false, 
                      /*printBlockTerminators=*/false);
}

// 
// 3) For other ops like ARTS_EpochOp, ARTS_SingleOp, ARTS_ParallelOp,
//    ARTS_DataBlockCreateOp, we might just rely on the default 
//    assemblyFormat from TableGen, or do similarly minimal parse/print if needed.
//

// Example: ARTS_DataBlockCreateOp parse/print
ParseResult ARTS_DataBlockCreateOp::parse(OpAsmParser &parser, OperationState &result) {
  // Suppose we do:   datablock.create size(<int>) align(<int>) : <type>
  // For brevity, skip thorough checks:
  if (parser.parseKeyword("size"))
    return failure();
  if (parser.parseLParen())
    return failure();
  IntegerAttr sizeAttr;
  if (parser.parseAttribute(sizeAttr))
    return failure();
  if (parser.parseRParen())
    return failure();

  if (parser.parseKeyword("align"))
    return failure();
  if (parser.parseLParen())
    return failure();
  IntegerAttr alignAttr;
  if (parser.parseAttribute(alignAttr))
    return failure();
  if (parser.parseRParen())
    return failure();

  Type resultType;
  if (parser.parseColonType(resultType))
    return failure();

  // store them
  result.addAttribute("size", sizeAttr);
  result.addAttribute("align", alignAttr);
  result.addTypes(resultType);
  return success();
}

void ARTS_DataBlockCreateOp::print(OpAsmPrinter &printer) {
  printer << "arts.datablock.create size(" << getSize() << ") align(" << getAlign() << ") : " 
          << getResult().getType();
}

//
// 4) Optionally you can define verifiers, e.g. verify() for EdtOp
//
LogicalResult ARTS_EdtOp::verify() {
  // For example, check that the 'dependencies' attr is an array of dictionaries
  auto deps = (*this)->getAttrOfType<ArrayAttr>("dependencies");
  if (!deps)
    return emitOpError() << "missing 'dependencies' attribute";
  for (auto d : deps) {
    if (!d.isa<DictionaryAttr>())
      return emitOpError() << "expected dictionary for dependencies, got " << d;
    // etc...
  }
  return success();
}

//
// 5) That’s it! The rest of the ops can remain with default parse/print
//    if your TableGen assemblyFormat is enough, or define them similarly here.
//