# Distribution (Technical Guide)

## Purpose

This document defines how CARTS selects and applies loop distribution for
`arts.for` in parallel EDT regions.

This is the H2 heuristic family:

- H2 decides distribution kind and worker bounds
- H2 does not decide DB layout modes (that is H1 partitioning)

For partitioning details, see:
`docs/heuristics/partitioning/partitioning.md`

## Runtime constraints that shape H2

- DB writes follow single-writer ownership semantics
- Internode write-back is expensive compared to read-only acquire
- Work stealing exists, but static mapping still matters for first placement
- Owner-computes remains the default lowering model

## Pipeline placement

Distribution is a dedicated pipeline stage:

- `concurrency`: prepare parallel structure, annotate access patterns/hints
- `edt-distribution`: choose distribution + annotate `arts.for`, then lower
- `concurrency-opt`: DB partitioning and downstream optimizations

Commands:

```bash
carts-run input.mlir --stop-at=concurrency
carts-run input.mlir --stop-at=edt-distribution
carts-run input.mlir --stop-at=concurrency-opt
```

## IR contract

`EdtDistributionPass` annotates each top-level `arts.for` in a parallel EDT:

- `distribution_kind` (`#arts.distribution_kind<...>`)
- `distribution_pattern` (`#arts.distribution_pattern<...>`)
- `distribution_version = 1`

These attributes are consumed by `ForLowering` and propagated to lowered epoch
and task EDT operations.

## Pattern classification ownership

Pattern detection is centralized in DB-backed analysis, not in lowering passes:

- `DbAnalysis::analyzeLoopDbAccessPatterns(ForOp)` computes loop summary
- `ArtsAnalysisManager::getLoopDistributionPattern(Operation *)` delegates to DB analysis
- `EdtDistributionPass` queries manager API and only maps pattern -> kind
- `EdtAnalysis` consumes loop summaries for EDT-level reporting

Current pattern values:

- `uniform`
- `stencil`
- `matmul`
- `triangular`
- `unknown`

## Strategy mapping

Current policy in `DistributionHeuristics::selectDistributionKind`:

- If EDT concurrency is internode, select `two_level`
- Otherwise:
  - `triangular` -> `block_cyclic`
  - `matmul` -> `tiling_2d`
  - `uniform`/`stencil`/`unknown` -> `block`

## Lowering architecture

`ForLowering` is orchestration only; strategy-specific behavior is delegated:

- Task-loop lowering:
  - `BlockTaskLoopLowering`
  - `BlockCyclicTaskLoopLowering`
  - `Tiling2DTaskLoopLowering`
- Acquire rewriting:
  - `BlockEdtRewriter`
  - `StencilEdtRewriter`

Current implementation status:

- `block`: implemented and default intranode path
- `two_level`: implemented internode path
- `block_cyclic`: implemented task-loop scheduling path
- `tiling_2d`: implemented with
  - dedicated task-loop lowering (`Tiling2DTaskLoopLowering`)
  - task-level column striping
  - owner-hint propagation for `inout` acquires so H1 can materialize 2D
    output ownership (`blockND`) in `DbPartitioning`

Current scope of `tiling_2d`:
- 2D owner partitioning is applied to writable outputs (`inout`) when 2D
  partition hints are present.
- Read-only inputs continue to use existing block/read paths.

## Interaction with partitioning (H1)

Distribution and partitioning are separate and ordered:

1. H2 distribution decides worker mapping and loop/task structure
2. H1 partitioning decides DB layout/rewrite in `DbPartitioning`
   - for `tiling_2d` outputs, H1 consumes the propagated owner hints and
     selects N-D block ownership when valid

Both consume shared DB/EDT analysis APIs, but they optimize different layers.

## Validation checklist

```bash
ninja -C build carts-run

# Inspect distribution annotations
carts-run gemm.mlir --O3 --arts-config arts.cfg --stop-at=edt-distribution

# Optional debug traces
carts-run gemm.mlir --O3 --arts-config arts.cfg --stop-at=edt-distribution \
  --debug-only=edt_distribution,for_lowering 2>&1
```

Expected baseline:

- checksums unchanged for existing workloads
- `distribution_kind` and `distribution_pattern` visible after `edt-distribution`
