///==========================================================================///
/// File: SdeOps.td
/// This file defines the SDE dialect operations: CU, SU, MU, and yield.
///==========================================================================///

#ifndef ARTS_SDE_OPS
#define ARTS_SDE_OPS

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/EnumAttr.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/Interfaces/ControlFlowInterfaces.td"
include "mlir/Interfaces/LoopLikeInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

include "arts/dialect/sde/IR/SdeDialect.td"
include "arts/dialect/sde/IR/SdeTypes.td"

def SdeDepType
    : Type<CPred<"::llvm::isa<::mlir::arts::sde::DepType>($_self)">,
           "SDE dependency token">;
def SdeCompletionType
    : Type<CPred<"::llvm::isa<::mlir::arts::sde::CompletionType>($_self)">,
           "SDE completion token">;

//===----------------------------------------------------------------------===//
// Enum helpers
//===----------------------------------------------------------------------===//

class ArtsSde_I32Enum<string name, string description,
                      list<I32EnumAttrCase> cases>
    : I32EnumAttr<name, description, cases> {
  let cppNamespace = "::mlir::arts::sde";
  let genSpecializedAttr = 0;
}

class ArtsSde_I32EnumAttr<string mnemonic, ArtsSde_I32Enum enumInfo>
    : EnumAttr<ArtsSdeDialect, enumInfo, mnemonic> {
  let assemblyFormat = "`<` $value `>`";
}

//===----------------------------------------------------------------------===//
// Enums
//===----------------------------------------------------------------------===//

def SdeCuKind
    : ArtsSde_I32Enum<"SdeCuKind", "Compute unit kind", [
      I32EnumAttrCase<"parallel", 0, "parallel">,
      I32EnumAttrCase<"single", 1, "single">,
      I32EnumAttrCase<"task", 2, "task">
    ]>;
def SdeCuKindAttr : ArtsSde_I32EnumAttr<"cu_kind", SdeCuKind>;

def SdeAccessMode
    : ArtsSde_I32Enum<"SdeAccessMode", "Memory access mode", [
      I32EnumAttrCase<"read", 0, "read">,
      I32EnumAttrCase<"write", 1, "write">,
      I32EnumAttrCase<"readwrite", 2, "readwrite">
    ]>;
def SdeAccessModeAttr : ArtsSde_I32EnumAttr<"access_mode", SdeAccessMode>;

def SdeReductionKind
    : ArtsSde_I32Enum<"SdeReductionKind", "Reduction operation kind", [
      I32EnumAttrCase<"add", 0, "add">,
      I32EnumAttrCase<"mul", 1, "mul">,
      I32EnumAttrCase<"min", 2, "min">,
      I32EnumAttrCase<"max", 3, "max">,
      I32EnumAttrCase<"land", 4, "and">,
      I32EnumAttrCase<"lor", 5, "or">,
      I32EnumAttrCase<"lxor", 6, "xor">,
      I32EnumAttrCase<"custom", 7, "custom">
    ]>;
def SdeReductionKindAttr : ArtsSde_I32EnumAttr<"reduction_kind", SdeReductionKind>;

def SdeReductionStrategy
    : ArtsSde_I32Enum<"SdeReductionStrategy",
                      "Reduction execution strategy", [
      I32EnumAttrCase<"atomic", 0, "atomic">,
      I32EnumAttrCase<"tree", 1, "tree">,
      I32EnumAttrCase<"local_accumulate", 2, "local_accumulate">
    ]>;
def SdeReductionStrategyAttr
    : ArtsSde_I32EnumAttr<"reduction_strategy", SdeReductionStrategy>;

def SdeLinalgClassification
    : ArtsSde_I32Enum<"SdeLinalgClassification",
                      "Structured loop classification", [
      I32EnumAttrCase<"elementwise", 0, "elementwise">,
      I32EnumAttrCase<"stencil", 1, "stencil">,
      I32EnumAttrCase<"matmul", 2, "matmul">,
      I32EnumAttrCase<"reduction", 3, "reduction">
    ]>;
def SdeLinalgClassificationAttr
    : ArtsSde_I32EnumAttr<"linalg_classification", SdeLinalgClassification>;

def SdeScheduleKind
    : ArtsSde_I32Enum<"SdeScheduleKind", "Scheduling policy", [
      I32EnumAttrCase<"static_", 0, "static">,
      I32EnumAttrCase<"dynamic", 1, "dynamic">,
      I32EnumAttrCase<"guided", 2, "guided">,
      I32EnumAttrCase<"auto_", 3, "auto">,
      I32EnumAttrCase<"runtime", 4, "runtime">
    ]>;
def SdeScheduleKindAttr : ArtsSde_I32EnumAttr<"schedule", SdeScheduleKind>;

def SdeConcurrencyScope
    : ArtsSde_I32Enum<"SdeConcurrencyScope", "Concurrency scope", [
      I32EnumAttrCase<"local", 0, "local">,
      I32EnumAttrCase<"distributed", 1, "distributed">
    ]>;
def SdeConcurrencyScopeAttr
    : ArtsSde_I32EnumAttr<"concurrency_scope", SdeConcurrencyScope>;

def SdeDistributionKind
    : ArtsSde_I32Enum<"SdeDistributionKind", "Distribution advisory hint", [
      I32EnumAttrCase<"owner_compute", 0, "owner_compute">,
      I32EnumAttrCase<"blocked", 1, "blocked">,
      I32EnumAttrCase<"cyclic", 2, "cyclic">
    ]>;
def SdeDistributionKindAttr
    : ArtsSde_I32EnumAttr<"distribution_kind", SdeDistributionKind>;

//===----------------------------------------------------------------------===//
// CU (Compute Unit) Ops
//===----------------------------------------------------------------------===//

def SdeCuRegionOp
    : ArtsSde_Op<"cu_region", [
      SingleBlock, NoTerminator,
      RecursiveMemoryEffects
    ]> {
  let summary = "Parallel or single execution region";
  let description = [{
    Captures `omp.parallel`, `omp.single`, `omp.master`. Represents a
    region of computation that runs with a given concurrency kind and scope.

    The `nowait` attribute preserves OpenMP nowait semantics. The
    `concurrency_scope` attribute indicates local vs distributed execution.
  }];

  let arguments = (ins
    SdeCuKindAttr:$kind,
    OptionalAttr<SdeConcurrencyScopeAttr>:$concurrency_scope,
    UnitAttr:$nowait
  );

  let regions = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    $kind (`scope` `(` $concurrency_scope^ `)`)? (`nowait` $nowait^)?
    $body attr-dict
  }];
}

def SdeCuTaskOp
    : ArtsSde_Op<"cu_task", [
      SingleBlock, NoTerminator,
      RecursiveMemoryEffects
    ]> {
  let summary = "Task with explicit data dependencies";
  let description = [{
    Captures `omp.task` with `depend(in/out/inout)` clauses. Dependencies
    are specified via `sde.mu_dep` results or inferred from SSA when using
    tensor operands.
  }];

  let arguments = (ins
    Variadic<SdeDepType>:$deps
  );

  let regions = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    (`deps` `(` $deps^ `:` type($deps) `)`)? $body attr-dict
  }];
}

def SdeCuReduceOp
    : ArtsSde_Op<"cu_reduce", [MemoryEffects<[MemRead, MemWrite]>]> {
  let summary = "Reduction accumulation";
  let description = [{
    Captures reduction semantics from `omp.wsloop reduction(+: sum)`.
    PRESERVES the combiner semantics that the current pipeline loses.

    The reduction kind (add, mul, min, max, etc.) and identity value are
    explicit. For custom reductions, the combiner region carries the
    reduction body.
  }];

  let arguments = (ins
    SdeReductionKindAttr:$reduction_kind,
    AnyType:$accumulator,
    AnyType:$partial,
    AnyType:$identity
  );

  let results = (outs AnyType:$result);

  let regions = (region AnyRegion:$combiner);

  let assemblyFormat = [{
    $reduction_kind `(` $accumulator `,` $partial `:` type($accumulator) `,` type($partial) `)`
    `identity` `(` $identity `:` type($identity) `)`
    ($combiner^)? attr-dict `->` type($result)
  }];
}

def SdeCuAtomicOp
    : ArtsSde_Op<"cu_atomic", [MemoryEffects<[MemWrite]>]> {
  let summary = "Atomic memory update";
  let description = [{
    Captures `omp.atomic.update`. Currently only supports add{i,f}.
    Memref-only since atomics are inherently side-effecting operations
    on mutable memory locations.
  }];

  let arguments = (ins
    SdeReductionKindAttr:$reduction_kind,
    AnyMemRef:$addr,
    AnyType:$value
  );

  let assemblyFormat = [{
    $reduction_kind `(` $addr `,` $value `:` type($addr) `,` type($value) `)`
    attr-dict
  }];
}

//===----------------------------------------------------------------------===//
// SU (Scheduling Unit) Ops
//===----------------------------------------------------------------------===//

def SdeSuIterateOp
    : ArtsSde_Op<"su_iterate", [
      SingleBlock, NoTerminator,
      RecursiveMemoryEffects,
      AttrSizedOperandSegments,
      DeclareOpInterfaceMethods<LoopLikeOpInterface,
        ["getLoopRegions", "getLoopInductionVars", "getLoopLowerBounds",
         "getLoopUpperBounds", "getLoopSteps"]>
    ]> {
  let summary = "Parallel iteration space";
  let description = [{
    Captures `omp.wsloop + omp.loop_nest`, `omp.taskloop`, `scf.parallel`.
    Represents a parallel iteration space with optional schedule, chunk size,
    nowait, and reduction metadata.

    Reduction accumulators can carry the combiner kind and identity value,
    preserving information lost by the current pipeline.
  }];

  let arguments = (ins
    Variadic<Index>:$lowerBounds,
    Variadic<Index>:$upperBounds,
    Variadic<Index>:$steps,
    OptionalAttr<SdeScheduleKindAttr>:$schedule,
    Optional<Index>:$chunkSize,
    UnitAttr:$nowait,
    Variadic<AnyType>:$reductionAccumulators,
    OptionalAttr<ArrayAttr>:$reductionKinds,
    OptionalAttr<SdeReductionStrategyAttr>:$reductionStrategy,
    OptionalAttr<SdeLinalgClassificationAttr>:$linalgClassification
  );

  let regions = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    `(` $lowerBounds `)` `to` `(` $upperBounds `)` `step` `(` $steps `)`
    (`schedule` `(` $schedule^ (`,` $chunkSize^)? `)` )?
    (`nowait` $nowait^)?
    (`reduction` `[` $reductionKinds^ `]` `(` $reductionAccumulators `:` type($reductionAccumulators) `)`)?
    (`reduction_strategy` `(` $reductionStrategy^ `)` )?
    (`classification` `(` $linalgClassification^ `)` )?
    $body attr-dict
  }];
}

def SdeSuDistributeOp
    : ArtsSde_Op<"su_distribute", [
      SingleBlock, NoTerminator,
      RecursiveMemoryEffects
    ]> {
  let summary = "Distribution advisory wrapper for SDE work";
  let description = [{
    Wraps a scheduling-unit region with a runtime-neutral distribution hint.
    This is an SDE-level advisory carrier only; it does not encode ARTS worker,
    route, or node identifiers.
  }];

  let arguments = (ins SdeDistributionKindAttr:$kind);
  let regions = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    $kind $body attr-dict
  }];
}

def SdeSuBarrierOp
    : ArtsSde_Op<"su_barrier", [MemoryEffects<[MemWrite]>]> {
  let summary = "Synchronization point";
  let description = [{
    Captures `omp.barrier` (implicit) and `omp.taskwait` (explicit tokens).
    When tokens are provided, waits only for those specific tasks.
  }];

  let arguments = (ins Variadic<SdeCompletionType>:$tokens);

  let assemblyFormat = [{
    (`(` $tokens^ `:` type($tokens) `)`)? attr-dict
  }];
}

//===----------------------------------------------------------------------===//
// MU (Memory Unit) Ops
//===----------------------------------------------------------------------===//

def SdeMuDepOp
    : ArtsSde_Op<"mu_dep", [Pure, AttrSizedOperandSegments]> {
  let summary = "Data dependency declaration (memref fallback)";
  let description = [{
    Declares a data dependency with access mode and region. Used in the
    memref fallback path. In the tensor path, `tensor.extract_slice`/
    `tensor.insert_slice` carry the same information natively.
  }];

  let arguments = (ins
    SdeAccessModeAttr:$mode,
    AnyMemRef:$source,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );

  let results = (outs SdeDepType:$dep);

  let assemblyFormat = [{
    $mode $source (`[` $offsets^ `]`)? (`size` `[` $sizes^ `]`)?
    `:` type($source) `->` type($dep) attr-dict
  }];
}

def SdeMuAccessOp
    : ArtsSde_Op<"mu_access", [Pure, AttrSizedOperandSegments]> {
  let summary = "Access-region annotation for memref fallback paths";
  let description = [{
    Explicitly records an access slice in the memref fallback path without
    materializing a dependency token or introducing runtime-specific behavior.
  }];

  let arguments = (ins
    SdeAccessModeAttr:$mode,
    AnyMemRef:$source,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );

  let assemblyFormat = [{
    $mode $source (`[` $offsets^ `]`)? (`size` `[` $sizes^ `]`)?
    `:` type($source) attr-dict
  }];
}

def SdeMuReductionDeclOp
    : ArtsSde_Op<"mu_reduction_decl", [IsolatedFromAbove, Symbol]> {
  let summary = "Module-level SDE reduction declaration";
  let description = [{
    Declares a named SDE reduction symbol with its element type, reduction
    classification, optional identity attribute, and an optional combiner
    region for custom reductions.
  }];

  let arguments = (ins
    SymbolNameAttr:$sym_name,
    TypeAttr:$type,
    SdeReductionKindAttr:$reduction_kind,
    OptionalAttr<AnyAttr>:$identity
  );

  let regions = (region AnyRegion:$combiner);
  let hasVerifier = 1;

  let assemblyFormat = [{
    $sym_name `:` $type `kind` `(` $reduction_kind `)`
    (`identity` `(` $identity^ `)` )?
    ($combiner^)?
    attr-dict
  }];
}

//===----------------------------------------------------------------------===//
// Yield
//===----------------------------------------------------------------------===//

def SdeYieldOp
    : ArtsSde_Op<"yield", [Pure, ReturnLike, Terminator]> {
  let summary = "SDE region terminator";
  let description = [{
    Terminates all SDE regions. Can optionally return values for reductions
    and iter_args.
  }];

  let arguments = (ins Variadic<AnyType>:$values);

  let assemblyFormat = [{ ($values^ `:` type($values))? attr-dict }];
}

#endif // ARTS_SDE_OPS
