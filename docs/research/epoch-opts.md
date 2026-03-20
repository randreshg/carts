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
polybench/jacobi2d    FIRES      2 epochs, clean loop body
layernorm (NREPS>1)   EXPECTED   1 epoch + timer call
pooling (NREPS>1)     EXPECTED   3 epochs + timer call
batchnorm (NREPS>1)   EXPECTED   5-6 epochs + timer call
activations (NREPS>1) BLOCKED    memref.alloc in sequential tail (pre-existing)
poisson-for           BLOCKED    C1: iter_args (convergence loop)
seidel-2d             BLOCKED    C2: nested wavefront loop
gemm, stream          N/A        Single kernel, no iteration
```

### Known Issue: External memref.alloc

Benchmarks like `activations` have a `memref.alloc` (e.g., softmax scratch
buffer) defined outside the loop but used by sequential code inside it. When
this sequential code ends up inside a continuation EDT body, the EdtOp verifier
rejects the external reference.

Fix implemented: `promoteAllocsForCPSChain()` promotes these allocations to
`DbAllocOp` so they flow through EdtEnvManager's paramv machinery. The EdtOp
verifier was also updated to allow `DbAllocOp` results (they're already
captured as `dbHandles`).

## Future Work

- **iter_args support (C1)**: Transport iteration values through paramv for
  convergence loops (poisson-for)
- **Nested loops (C2)**: Use CPS driver for inner loop + CPS chain for outer
  (seidel-2d wavefront)
- **Runtime validation**: Benchmark runs to measure sync overhead reduction
