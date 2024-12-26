//===- ARTSOps.td - ARTS Dialect Operations ----------------------*- tablegen -*-===//
//
// This file defines multiple operations for the ARTS Dialect:
//   1) arts.epoch
//   2) arts.single
//   3) arts.edt
//   4) arts.datablock.create
//
//===----------------------------------------------------------------------===//

#ifndef ARTS_OPS
#define ARTS_OPS

#include "mlir/IR/OpBase.td"
#include "mlir/IR/Dialect.h.td"
#include "mlir/Interfaces/SideEffectInterfaces.td"

// Include the dialect definition if you have it in a separate .td file.
// Or define the dialect in ARTSOpsDialect.td.
#include "ARTSDialect.td"

/////////////////////////////////////////////////////////////////////////
// 1) arts.epoch
/////////////////////////////////////////////////////////////////////////

def ARTS_EpochOp : ARTS_Op<"epoch", [HasSingleBlockImplicitTerminator]> {
  let summary = "Start a finish scope for tasks in the region.";
  let description = [{
    The `arts.epoch` operation starts a region that collectively finishes
    only when all tasks (EDTs) spawned inside have completed. 
    It has a single region for the body.
  }];

  let regions = (region AnyRegion:$body);

  // Example custom assembly format:
  //   arts.epoch { <region body> }
  // If you don't define a custom parser/printer, you can rely on default.
  let assemblyFormat = "`epoch` $body";
}

/////////////////////////////////////////////////////////////////////////
// 2) arts.single
/////////////////////////////////////////////////////////////////////////

def ARTS_SingleOp : ARTS_Op<"single", [HasSingleBlockImplicitTerminator]> {
  let summary = "A region executed by exactly one worker.";
  let description = [{
    Similar to a single or master region in parallel frameworks. Only one 
    worker enters this region. 
  }];

  let regions = (region AnyRegion:$body);

  let assemblyFormat = "`single` $body";
}

/////////////////////////////////////////////////////////////////////////
// 3) arts.edt
/////////////////////////////////////////////////////////////////////////

def ARTS_EdtOp : ARTS_Op<"edt", [HasSingleBlockImplicitTerminator]> {
  let summary = "Define or create an ARTS Event-Driven Task (EDT)";
  let description = [{
    Creates a new ARTS event-driven task with optional parameters 
    and dependencies. The body region is the code executed by the task.
    
    * 'parameters' might be a list of captured values or static parameters.
    * 'dependencies' might be a list of dictionary attributes specifying
       (mode = in/out, memref/subview, index, etc.).
  }];

  // Single region for the task body.
  let regions = (region AnyRegion:$body);

  // We’ll store the actual list of parameters/dependencies in attributes or operands.
  // The assembly format below is just an example of DSL-like printing.
  let assemblyFormat = [
    "`edt`",
    "parameters(",
    "$parameters^",
    ")",
    "` dependencies(`",
    "$dependencies^",
    "`)`",
    "$body"
  ];
}

/////////////////////////////////////////////////////////////////////////
// 4) arts.datablock.create
/////////////////////////////////////////////////////////////////////////

def ARTS_DataBlockCreateOp : ARTS_Op<"datablock.create"> {
  let summary = "Create an ARTS DataBlock with the given size or type";
  let description = [{
    This is an example op for allocating a DataBlock in the ARTS runtime.
    Typically you might have attributes for size, alignment, or type.
  }];

  // If you want this op to produce a result representing the DataBlock GUID:
  let results = (outs AnyType:$db);

  // Example attributes: we can store 'size', 'element_type', etc.
  let assemblyFormat = [
    "`datablock.create`",
    "size(",
    "$sizeAttr",
    ")",
    // If you have an optional alignment:
    "` align(` $alignAttr `)`",
    " `:` type($db)"
  ];
}

#endif // ARTS_OPS