# Epoch Optimizations — CPS Chain Transform

## Overview

The CPS (Continuation-Passing Style) chain transform eliminates blocking
`arts_wait_on_handle()` calls from iterative epoch loops. Instead of the main
thread waiting for each iteration to complete, the first epoch is created and
the main thread returns immediately. All subsequent iterations are driven by
continuation EDTs fired through the ARTS runtime's epoch finish mechanism.

## How It Works

Given an iterative loop with N epochs per iteration:

```
scf.for t = lb to ub step s {
  epoch_0 { worker EDTs ... }
  epoch_1 { worker EDTs ... }
  ...
  epoch_{N-1} { worker EDTs ... }
  sequential_ops ...
}
```

The transform produces:

```
outer_epoch (cps_outer_epoch):
  epoch_0 workers (iter 0, cloned with iv=lb)

  chain_0 EDT (continuation, fires after epoch_0 completes):
    [inter-epoch sequential ops]
    epoch_1 workers

  chain_1 EDT (continuation, fires after epoch_1 completes):
    ...

  chain_{N-1} EDT (continuation, fires after epoch_{N-1} completes):
    [tail sequential ops]
    CPSAdvance:
      if t_next < ub:
        epoch_0' workers (iter 1)    <-- next iteration, self-referential
```

Each continuation is a single EDT that fires asynchronously when its epoch
completes. No thread blocks — the chain drives itself through the runtime's
epoch finish signaling.

## Reference Implementation

A manually-written ARTS C example is at `external/arts/examples/cpu/cps_chain.c`.
It demonstrates the 5 design rules:

1. **Data through depv, not paramv** — workers receive DBs via dependency slots
2. **Separate DBs per worker** — each worker gets its own partition GUID
3. **Non-blocking main** — main EDT returns immediately after first epoch
4. **Finish EDT pattern** — `arts_initialize_and_start_epoch(finish_edt, slot)`
5. **CDAG frontier ordering** — EW before RO on same DB for correct ordering

The example uses a scratch DB to carry iteration state (counter, partition
GUIDs, norm, finish GUID) through `depv`. The compiler uses `paramv` instead
(packed i64 values), which is functionally equivalent for single-node.

## Compiler Implementation

### Pass: EpochOpt (step 13)

The transform lives in `tryCPSChainTransform()` in
`lib/arts/passes/opt/edt/EpochOpt.cpp`. It is enabled by default through the
`--arts-epoch-finish-continuation` pipeline switch and can be disabled with
`--arts-epoch-finish-continuation=false` for debugging or IR-diff isolation.

### Eligibility Checks

```
C1: No iter_args on the wrapping scf.for loop
C2: Not nested inside another loop
C3: Trip count >= 2
C4: Body contains at least one epoch (collectEpochSlots)
C5: Epochs not already marked for continuation
C6: All epoch bodies must be clonable (non-empty)
C7: All non-slot ops must be pure arithmetic, DbOps, or sequential ops
```

C7 was relaxed to allow sequential operations (scf.for, func.call, memref ops,
math ops) in the loop body via `isCPSSequentialOp()`. These ops are partitioned
by position relative to epochs and cloned into the appropriate continuation.

### Lowering Pipeline

```
EpochOpt (step 13)        → creates CPS chain IR (CPSAdvanceOp, continuation EDTs)
EpochLowering (step 15)   → resolves CPSAdvanceOp into epoch creation + worker cloning
EdtLowering (step 16)     → packs captured values into paramv via EdtEnvManager
ConvertArtsToLLVM (step 17)→ emits arts_initialize_and_start_epoch, arts_edt_create, etc.
```

### Key IR Attributes

- `cps_outer_epoch` — marks the outer epoch wrapping the chain
- `cps_chain_id` — identifies which chain a continuation belongs to
- `continuation_for_epoch` — marks an EDT as an epoch's finish continuation

## Benchmark Coverage

```
Benchmark             Status     Notes
--------------------  ---------  ----------------------------------------
polybench/jacobi2d    FIRES      2 epochs, clean loop body. Checksum verified.
layernorm (NREPS>1)   FIRES      1 epoch + timer call. Checksum verified (NREPS=2).
                                 NREPS=1 (small): no-op (C3: trip count < 2).
pooling (NREPS>1)     FIRES      3 epochs + timer call. Checksum verified (NREPS=2).
                                 NREPS=1: no-op (C3).
batchnorm (NREPS>1)   FIRES      6 epochs + timer call. Checksum verified (NREPS=2).
                                 NREPS=1: no-op (C3).
activations (NREPS>1) FIRES      1 epoch + timer call. Checksum verified (NREPS=2).
                                 NREPS=1 (small): no-op (C3). Flag harmless.
poisson-for           HARMLESS   CPS chain does not fire (loops fail C1/C2).
                                 Known: continuation-prep in pre-lowering changes
                                 checksum (2.08e-02 vs baseline 1.04e-01).
                                 Pre-existing; not caused by CPS chain transform.
seidel-2d             BLOCKED    C2: wavefront loop nested inside time-step loop.
                                 Flag is harmless — compiles and runs correctly.
gemm, stream          N/A        Single kernel, no iteration.
```

### Resolved: External memref.alloc

Benchmarks like `activations` have a `memref.alloc` (e.g., softmax scratch
buffer) defined outside the loop but used by code inside it. When this code
ends up inside a continuation EDT body, the EdtOp verifier rejects the
external reference.

Fix: The EdtOp verifier in `Dialect.cpp` now skips the external-value check
for CPS continuation EDTs (identified by `arts.cps_chain_id` attr). External
values are captured by EdtEnvManager's paramv machinery during EdtLowering.
Additionally, `rematerializeExternalDefs()` clones pure external definition
chains (constants, pointer2memref) into continuation EDT bodies so they don't
require paramv capture.

### Resolved: EdtLowering crash with multi-epoch CPS chain

Pooling crashed in `MemoryAccessClassifier::collectAccessOperations` during
EdtLowering. The root cause: EdtLowering processes EDTs in post-order (inner
first), outlining each EDT body to a separate function. This invalidates
cached `DbGraph` nodes that still reference block arguments and operations
from the old EDT body. Subsequent EDT lowerings would find stale graph nodes
when `resolveAcquireRewriteContract` triggered `DbDimAnalyzer::compute`,
which walks peer acquires via `collectDistributedPeerDims`.

Fix (two-layer):
1. `resolveAcquireRewriteContract()` detects acquires inside CPS continuation
   EDTs (via `arts.cps_chain_id` attr) and derives the contract directly from
   IR attributes instead of the analysis graph.
2. `EdtLoweringPass::runOnOperation()` now invalidates the `DbAnalysis` graph
   after each EDT is lowered, ensuring the next EDT's analysis rebuilds from
   fresh IR state.

### Resolved: poisson-for conditional epoch support

`emitAdvanceLogic()` and the initial epoch cloning (Step 2) now handle the
`wrappingIf` case: when `slots[0].wrappingIf` is set, they clone the entire
`scf.if` (both then and else branches) and mark all enclosed epochs for
continuation. This enables correct parity-conditional CPS chains.

Note: poisson-for itself doesn't fire the CPS chain (its loops fail C1/C2
checks), but the wrappingIf support is validated by the
`epoch_cps_chain_parity.mlir` contract test and is needed for future
benchmarks with conditional epoch patterns.

### Resolved: Multi-epoch paramv narrowing (batchnorm)

In multi-epoch CPS chains (N>2), intermediate continuations captured only
the values they directly used, "narrowing" the paramv. When the innermost
continuation (chain_{N-1}) looped back to chain_0, it couldn't provide all
values chain_0 needed.

Root cause (two bugs):
1. **Paramv narrowing**: Different epochs reference different DB pointers.
   chain_{N-1} only captured values for its own epoch + epoch_0 (via the
   advance), missing values from epochs 1 through N-2.
2. **CPS param index mismatch**: The advance resolver read CPS param indices
   (iter counter, outer epoch GUID) from the TARGET chain's EdtCreateOp
   instead of the LOCAL function's. Since each chain has different param
   counts, the indices were wrong.

Fix:
1. `collectAllEpochExternalValues()` collects external values referenced
   by ANY epoch body. These "carry values" are passed as extra operands
   to `CPSAdvanceOp`, forcing EdtEnvManager to capture them in every
   continuation through the nesting chain.
2. The CPSAdvance resolver now reads CPS param indices from the local
   function's own EdtCreateOp (found by matching `outlined_func` to
   `parentFunc.getName()`).

## Future Work

- **Nested loops (C2)**: Relax `isInsideLoop()` or chain CPS driver for inner
  loop + CPS chain for outer (seidel-2d wavefront)
- **Runtime performance**: Benchmark CPS chain vs blocking wait overhead at
  large scale
