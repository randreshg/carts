# ARTS RDMA Stencil Validation Status - 2026-05-31

> Historical record. Commands below were run before the v4 driver cleanup; the
> `--distributed-db` and `--no-distributed-db` flags have since been removed;
> distribution is default-on under multinode `-O3`.

## Summary

The latest validated CARTS/ARTS state fixes the two-node Jacobi stencil
failure. The compiler now materializes per-block halo exchange inside the
timestep phases before stencil compute, and ARTS RDMA now uses stable eager
connections by default for multinode halo traffic.

The active failure was not an `arts_malloc` allocation timeout. Reducing
allocation size remains a valid pressure valve for future larger cases, but the
observed failing medium Jacobi case was stale halo placement first, then an
RDMA connection/progress policy issue after the compiler ordering was corrected.

## Validated Results

| Case | Transport | Result | Checksum | Kernel time | Result directory |
| --- | --- | --- | --- | --- | --- |
| `polybench/jacobi2d`, medium, 8 threads, 1 node | TCP | PASS | `1.296178421124e+02` | `0.010011s` | `external/carts-benchmarks/results/20260531_172311_519542` |
| `polybench/jacobi2d`, medium, 8 threads, 2 nodes | TCP | PASS | `1.296178421124e+02` | `0.541749s` | `external/carts-benchmarks/results/20260531_172017_049806` |
| `polybench/jacobi2d`, medium, 8 threads, 2 nodes | RDMA | PASS | `1.296178421124e+02` | `0.458949s` | `external/carts-benchmarks/results/20260531_172219_180831` |

The final 2-node RDMA run completed with Slurm job `70651` on
`m20u[19,25]`, exit code `0`, and no `arts_wait_on_handle` warning or RDMA
connect-refused error. `squeue -u "$USER"` was checked after the Slurm runs.

## Commands

Focused compiler validation:

```bash
timeout --signal=TERM --kill-after=20s 180s .dekk/env/bin/dekk carts lit --skip-test-time-recording \
  lib/carts/dialect/arts/test/conversion/sde-to-arts-per-block-single-writer-stencil.mlir \
  lib/carts/dialect/arts/test/conversion/sde-to-arts-host-whole-stencil-promoted-to-block.mlir \
  lib/carts/dialect/arts/test/conversion/sde-to-arts-stencil-tiling-nd-2d-no-replicated-read.mlir \
  lib/carts/dialect/arts/test/conversion/sde-to-arts-alternating-buffer-stencil-stays-block.mlir
```

2-node RDMA validation:

```bash
timeout --signal=TERM --kill-after=30s 300s .dekk/env/bin/dekk carts benchmarks run \
  polybench/jacobi2d --size medium --threads 8 --nodes 2 --arts \
  --compile-args '--distributed-db' --slurm -p mi300x_cpx --rdma \
  --timeout 120 --max-jobs 1 --debug 1
```

1-node baseline:

```bash
timeout --signal=TERM --kill-after=30s 240s .dekk/env/bin/dekk carts benchmarks run \
  polybench/jacobi2d --size medium --threads 8 --nodes 1 --arts \
  --compile-args '--distributed-db' --slurm -p mi300x_cpx --rdma \
  --timeout 120 --max-jobs 1 --debug 1
```

## Diagnosis

The earlier generated post-db-refinement IR emitted per-block halo exchanges
only after the full 100-step loop. That meant timesteps after the first used
stale neighbor halos at block boundaries. The corrected IR emits the halo EDTs
inside the timestep loop, then inserts a required-memory barrier before the
stencil compute EDTs.

After that compiler fix, TCP passed with the same generated code, while RDMA
first exposed connection-reset/refused behavior and then a timeout when eager
connect was enabled with full-duplex rsocket reuse. This isolated the remaining
failure to RDMA transport policy rather than ARTS halo placement or the CDAG
memory model.

The production RDMA default now keeps persistent eager connections open and
uses stable unidirectional rsocket paths:

```c
#define ARTS_RDMA_EAGER_CONNECT 1
#define ARTS_RDMA_EAGER_CONNECT_ROUNDS 16
#define ARTS_RDMA_FULL_DUPLEX 0
```

With those defaults, the no-override 2-node RDMA benchmark passes.

## Compiler/Runtime Shape

The current implementation follows the intended layering:

- SDE remains layout and graph planning only.
- ARTS carries explicit codelet deps and collective intent.
- ARTS realizes the per-block single-writer DB substrate and halo exchange.
- ARTS-RT lowers the already materialized EDT/DB/epoch shape to runtime calls.

The validated Jacobi path uses distributed per-block DBs with one writer per
block and nearest-neighbor RO reads for halo exchange. DB grain and CU grain
must remain separate for the larger production solution; shrinking CU tiles as
an allocation cap would increase bridge/CU task count and collapse two distinct
knobs into one.

## Remaining Work

For future allocation pressure, the production direction is separate
storage-grain metadata rather than benchmark-specific size cuts:

- SDE should compute storage block shape as layout evidence, separate from CU
  task shape.
- ARTS should carry storage tile attrs independently from compute tile attrs
  while still selecting collectives from pattern and layout mismatch.
- ARTS should allocate DBs from storage grain and group bridge/communication CUs
  over ranges when the edge is read-only or copy-like.

That keeps the SDE/ARTS responsibilities aligned and avoids hardcoded
benchmark behavior.
