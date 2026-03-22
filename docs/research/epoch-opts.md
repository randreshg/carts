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
`lib/arts/passes/opt/edt/EpochOpt.cpp`. It is gated by the
`--arts-epoch-finish-continuation` flag.

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
pooling (NREPS>1)     CRASHES    3 epochs + timer call. NREPS=2 crashes in
                                 EdtLowering (MemoryAccessClassifier). NREPS=1: no-op.
batchnorm (NREPS>1)   CRASHES    5-6 epochs + timer call. Same crash as pooling.
                                 NREPS=1: no-op (C3).
activations (NREPS>1) FAILS      CPS chain fires (1-epoch loop) but compilation fails:
                                 promoteAllocsForCPSChain() misses allocs used inside
                                 epoch worker bodies (only scans sequential ops).
                                 NREPS=1 (small): no-op (C3). Flag harmless.
poisson-for           FIRES/BAD  CPS chain fires on time-step loop (no iter_args on
                                 that loop). Checksum WRONG: 1.22e-16 vs baseline
                                 1.04e-01. Possible bug in CPS chain for multi-epoch
                                 Jacobi-alternating-buffer pattern.
seidel-2d             BLOCKED    C2: wavefront loop nested inside time-step loop.
                                 Flag is harmless — compiles and runs correctly.
gemm, stream          N/A        Single kernel, no iteration.
```

### Known Issue: External memref.alloc (PARTIALLY RESOLVED)

Benchmarks like `activations` have a `memref.alloc` (e.g., softmax scratch
buffer) defined outside the loop but used by code inside it. When this code
ends up inside a continuation EDT body, the EdtOp verifier rejects the
external reference.

Fix implemented: `promoteAllocsForCPSChain()` promotes these allocations to
`DbAllocOp` so they flow through EdtEnvManager's paramv machinery. The EdtOp
verifier was also updated to allow `DbAllocOp` results (they're already
captured as `dbHandles`).

**Remaining gap**: `promoteAllocsForCPSChain()` only scans operands of
*sequential ops* (ops between epochs). Allocs used inside *epoch worker
bodies* (e.g., `softmax_output` in activations, `memref<100xf32>`) are not
found. When the epoch body is cloned into the continuation EDT, the external
alloc reference causes an EdtOp verification error. Fix: extend the scan to
walk into epoch regions as well.

### Known Issue: EdtLowering crash with multi-epoch NREPS > 1

Pooling and batchnorm crash in `MemoryAccessClassifier::collectAccessOperations`
during EdtLowering when compiled with `--arts-epoch-finish-continuation` and
`NREPS > 1`. The crash occurs during `resolveAcquireRewriteContract` for
continuation EDTs. Root cause TBD — likely a contract analysis issue when CPS
chain clones multi-epoch patterns into continuation EDT bodies.

### Known Issue: poisson-for checksum mismatch

The CPS chain fires on poisson-for's time-step loop (which has no iter_args),
but produces wrong results. Baseline checksum `1.038656999420e-01`, CPS
checksum `1.224646799147e-16`. The time-step loop uses Jacobi alternating
buffers (`depPattern = jacobi_alternating_buffers`), which may have ordering
constraints not captured by the current CPS chain transform.

## Future Work

- **promoteAllocsForCPSChain() scope**: Extend alloc scanning to walk into
  epoch worker regions, not just sequential ops (blocks activations at NREPS>1)
- **EdtLowering crash**: Fix contract analysis for multi-epoch CPS chain
  continuations (blocks pooling/batchnorm at NREPS > 1)
- **poisson-for correctness**: Investigate CPS chain interaction with Jacobi
  alternating buffer patterns
- **Nested loops (C2)**: Relax `isInsideLoop()` or chain CPS driver for inner
  loop + CPS chain for outer (seidel-2d wavefront)
- **Runtime performance**: Benchmark CPS chain vs blocking wait overhead at
  large scale
