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

include "ArtsDialect.td"

include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/Interfaces/ViewLikeInterface.td"
include "mlir/Interfaces/ControlFlowInterfaces.td"
include "mlir/IR/SymbolInterfaces.td"

include "mlir/Dialect/LLVMIR/LLVMOpBase.td"
include "mlir/Dialect/LLVMIR/LLVMInterfaces.td"

/////////////////////////////////////////////////////////////////////////
// arts.edt
/////////////////////////////////////////////////////////////////////////
def EdtOp : ARTS_Op<"edt",
    [SingleBlockImplicitTerminator<"arts::YieldOp">]> {

  let summary = "Event-Driven Task in ARTS";
  let description = [{
    Defines an ARTS event-driven task (EDT) with:
      - Optional `parameters` that define task-specific inputs.
      - `dependencies` that specify input/output memory dependencies.

    The single-block region contains the task body. Parameters are provided
    as SSA values, and dependencies are described as structured attributes
    (e.g., memref, mode, indices).
  }];

  let regions = (region SizedRegion<1>:$body);
  let arguments = (ins
    // Task parameters as SSA values
    Variadic<AnyType>:$parameters,
    // Dependencies as structured attributes
    ArrayAttr:$dependencies
  );
  // let assemblyFormat = 
  //   "`edt` parameters(`$parameters`) dependencies(`$dependencies`) $body";
}

/////////////////////////////////////////////////////////////////////////
// arts.epoch
/////////////////////////////////////////////////////////////////////////
def EpochOp : ARTS_Op<"epoch", 
    [SingleBlockImplicitTerminator<"arts::YieldOp">]> {
  let summary = "Epoch region in ARTS";
  let description = [{
    An ARTS epoch region. It can spawn multiple tasks and
    synchronize them at the end of the epoch.
  }];
  let regions = (region SizedRegion<1>:$body);
}

/////////////////////////////////////////////////////////////////////////
// arts.parallel
/////////////////////////////////////////////////////////////////////////
def ParallelOp : ARTS_Op<"parallel", 
    [SingleBlockImplicitTerminator<"arts::YieldOp">]> {
  let summary = "Parallel region in ARTS";
  let description = [{
    An ARTS parallel region. It can spawn multiple tasks.
    Typically used to replicate something like `#pragma omp parallel`.
  }];
  let regions = (region SizedRegion<1>:$body);
  // let assemblyFormat = "`parallel` $body";
}

/////////////////////////////////////////////////////////////////////////
// arts.single
/////////////////////////////////////////////////////////////////////////
def SingleOp : ARTS_Op<"single",
    [SingleBlockImplicitTerminator<"arts::YieldOp">]> {
  let summary = "Single region in ARTS (like omp.master)";
  let description = [{
    A single region: only one thread/worker enters this region within a 
    parallel edt.
  }];
  let regions = (region SizedRegion<1>:$body);
  // let assemblyFormat = "`single` $body";
}

/////////////////////////////////////////////////////////////////////////
// arts.datablock.create
/////////////////////////////////////////////////////////////////////////
// def ARTS_DataBlockCreateOp : ARTS_Op<"datablock.create",
//     [DeclareOpInterfaceMethods<MemoryEffectsOpInterface>]> {
//   let summary = "Create a DataBlock in ARTS";
//   let description = [{
//     Allocates an ARTS DataBlock of a specified size or shape. 
//     This might produce a guid or a memref representing the allocated block.
//   }];
//   // If you produce a single GUID or memref, define the result:
//   let results = (outs AnyType:$result);

//   // If you want arguments:
//   // e.g. size or alignment
//   let arguments = (ins Variadic<Index>:$dims);
//   let assemblyFormat = "`datablock.create` $dims* `:` type($result)";
// }

/////////////////////////////////////////////////////////////////////////
// arts.terminator
/////////////////////////////////////////////////////////////////////////
def YieldOp : ARTS_Op<"arts_yield", 
  [Pure, ReturnLike, Terminator, 
   ParentOneOf<["EdtOp", "EpochOp", "ParallelOp", "SingleOp"]>]> {

  let summary = "ARTS dialect yield terminator";
  let description = [{
    The `arts.yield` operation serves as a terminator for various ARTS operations, 
    such as `arts.epoch`, `arts.parallel`, `arts.single`, and `arts.edt`.
    It ensures proper control flow within the ARTS dialect by marking the end of a region.
  }];
}

#endif // ARTS_OPS