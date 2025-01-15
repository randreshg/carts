//===- ARTSOps.td - ARTS Dialect Operations ----------------*- tablegen -*-===//
//
// This file defines multiple operations for the ARTS Dialect.
//
//===----------------------------------------------------------------------===//

#ifndef ARTS_OPS
#define ARTS_OPS

include "mlir/IR/OpBase.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/Interfaces/ViewLikeInterface.td"
include "mlir/Interfaces/ControlFlowInterfaces.td"
include "mlir/IR/SymbolInterfaces.td"

include "mlir/Dialect/LLVMIR/LLVMOpBase.td"
include "mlir/Dialect/LLVMIR/LLVMInterfaces.td"

/// ARTS Dialect
include "arts/ArtsDialect.td"
include "arts/ArtsTypes.td"

//===----------------------------------------------------------------------===//
// arts.undef
//===----------------------------------------------------------------------===//
def UndefOp: Arts_Op<"undef", [Pure]> {
  let summary = "More flexible undef op";
  let skipDefaultBuilders = 1;
  let results = (outs AnyType:$result);
  let builders = [
    OpBuilder<(ins "Type":$type), [{
      $_state.types.push_back(type);
    }]>];
  let hasCanonicalizer = true;
}

//===----------------------------------------------------------------------===//
// arts.make_dep
//===----------------------------------------------------------------------===//
def MakeDepOp : Arts_Op<"make_dep", []> {
  let summary = "Creates a dependency for arts.edt or arts.parallel";
  let description = [{
    This operation creates a dependency for the arts.edt operation.
    It can include an affine map + operands, or omit them entirely
    when no indexing is required.
  }];

  // Note the use of `OptionalAttr<AffineMapAttr>` for affine_map.
  let arguments = (ins
    StrAttr:$mode,                            // e.g., "in", "out", "inout"
    AnyType:$val                              // dependent value
  );

  let results = (outs DepType:$dep);

  // Use brackets as anchors for the optional group
  let assemblyFormat = [{
    $mode `,` $val `:` type($val)
    attr-dict `:` type($dep)
  }];

  let skipDefaultBuilders = 1;

  // We provide two builders:
  //  1) one that doesn't take a map or operands,
  //  2) one that takes the map and operands.
  // The user can choose based on whether they need affine indexing or not.
  let builders = [
    // e.g. make_dep("in", %A) => no map or affine operands
    OpBuilder<(ins "StringRef":$mode, "Value":$val)>
  ];
}

//===----------------------------------------------------------------------===//
// arts.edt
//===----------------------------------------------------------------------===//
def EdtOp : Arts_Op<"edt",
  [SingleBlockImplicitTerminator<"arts::YieldOp">, AttrSizedOperandSegments]> {

  let summary = "Event-Driven Task in ARTS";
  let description = [{
  Defines an ARTS event-driven task (EDT) with:
    - `parameters` that define task-specific inputs.
    - `dependencies` that specify input/output memory dependencies.
  The single-block region contains the task body.
  }];
  let arguments = (ins
    Variadic<AnyTypeOf<[AnyInteger, Index, AnyFloat]>>:$parameters,
    Variadic<DepType>:$dependencies
  );
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = [{
    `parameters` `(` $parameters `)` `:` `(` type($parameters) `)` `,` 
    `dependencies` `(` $dependencies `)` `:` `(` type($dependencies) `)`
    attr-dict-with-keyword $body
  }];
  let extraClassDeclaration = [{
    SmallVector<Value> getParametersValues();
    SmallVector<Value> getDependenciesValues();
    unsigned getNumParameters();
    unsigned getNumDependencies();
  }];
}

//===----------------------------------------------------------------------===//
// arts.parallel
//===----------------------------------------------------------------------===//
def ParallelOp : Arts_Op<"parallel", 
    [SingleBlockImplicitTerminator<"arts::YieldOp">, AttrSizedOperandSegments]> {
  let summary = "Parallel region in ARTS";
  let description = [{
    An ARTS parallel region. It can spawn multiple tasks.
    Typically used to replicate something like `#pragma omp parallel`.
    It has
      - `parameters` that define task-specific inputs.
      - `dependencies` that specify input/output memory dependencies.
  }];
  let arguments = (ins
    Variadic<AnyTypeOf<[AnyInteger, Index, AnyFloat]>>:$parameters,
    Variadic<DepType>:$dependencies
  );
  let regions = (region AnyRegion:$body);
  let assemblyFormat = [{
    `parameters` `(` $parameters `)` `:` `(` type($parameters) `)` `,` 
    `dependencies` `(` $dependencies `)` `:` `(` type($dependencies) `)`
    attr-dict-with-keyword $body
  }];
  let extraClassDeclaration = [{
    SmallVector<Value> getParametersValues();
    SmallVector<Value> getDependenciesValues();
    unsigned getNumParameters();
    unsigned getNumDependencies();
  }];
}

//===----------------------------------------------------------------------===//
// arts.epoch
//===----------------------------------------------------------------------===//
def EpochOp : Arts_Op<"epoch", 
    [SingleBlockImplicitTerminator<"arts::YieldOp">]> {
  let summary = "Epoch region in ARTS";
  let description = [{
    An ARTS epoch region. It can spawn multiple tasks and
    synchronize them at the end of the epoch.
  }];
  let regions = (region AnyRegion:$body);
  let assemblyFormat = [{
    attr-dict-with-keyword $body
  }];
}



//===----------------------------------------------------------------------===//
// arts.single
//===----------------------------------------------------------------------===//
def SingleOp : Arts_Op<"single",
    [SingleBlockImplicitTerminator<"arts::YieldOp">]> {
  let summary = "Single region in ARTS (like omp.master)";
  let description = [{
    A single region: only one thread/worker enters this region within a 
    parallel edt.
  }];
  let regions = (region AnyRegion:$body);
  let assemblyFormat = [{
    attr-dict-with-keyword $body
  }];
}

//===----------------------------------------------------------------------===//
// arts.yield
//===----------------------------------------------------------------------===//
def YieldOp : Arts_Op<"yield", 
  [Pure, ReturnLike, Terminator, 
   ParentOneOf<["EdtOp", "EpochOp", "ParallelOp", "SingleOp"]>]> {

  let summary = "ARTS dialect yield terminator";
  let description = [{
    The `arts.yield` operation serves as a terminator for various ARTS operations, 
    such as `arts.epoch`, `arts.parallel`, `arts.single`, and `arts.edt`.
    It ensures proper control flow within the ARTS dialect by marking the end of a region.
  }];
  let assemblyFormat = [{
    attr-dict-with-keyword
  }];
}

//===----------------------------------------------------------------------===//
// arts.barrier
//===----------------------------------------------------------------------===//
def BarrierOp : Arts_Op<"barrier"> {
  let summary = "Barrier synchronization in ARTS";
  let description = [{
    Barrier synchronization operation in ARTS. It synchronizes all previous
    spawned EDTs of the parent region.
  }];
  let assemblyFormat = "attr-dict";
}

#endif // ARTS_OPS