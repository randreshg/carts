# CARTS Dialect Map

## SDE Dialect: `sde`

Paths:

- `include/carts/dialect/sde/IR/`
- `lib/carts/dialect/sde/`
- tests: `lib/carts/dialect/sde/test/`

Purpose: runtime-agnostic semantic decomposition for OpenMP regions, memref
state normalization, reductions, HPF-style `DISTRIBUTE`/`ALIGN`, per-array
block layouts from affine access relations, abstract communication-volume cost,
and the real source/SU/CU/MU loop/layout rewrites required to make committed
layout facts true.

Limits: SDE should not encode ARTS runtime call shape or LLVM-facing ABI
details. It names no collectives, DBs, EDTs, routes, GUIDs, or runtime policy.
It should preserve and transform high-level semantics so later dialects do not
need to rediscover OpenMP intent or repair SDE layout promises.

Important transform areas:

- `state/` - representation changes, PatternAnalysis, MU materialization,
  scalar forwarding.
- `dep/` - structural and dependency transforms.
- `effect/` - scheduling, distribution, fusion/vectorization decisions.
- `Verify/` - SDE boundary contracts.

## CODIR Dialect: `codir`

Paths:

- `include/carts/dialect/codir/IR/`
- `lib/carts/dialect/codir/`
- tests: `lib/carts/dialect/codir/test/`

Purpose: isolated codelet bodies with explicit dependencies, params,
codelet-local verification, and graph structure. CODIR consumes committed SDE
layout, access-window, and movement facts, representing them mechanically before
ARTS realization.

Limits: CODIR should not encode runtime ABI or redo SDE data-layout analysis.
SDE hands codelets, layout facts, and movement structure here; ARTS picks up
that structure for orchestration.

Important areas:

- `IR/` - CodirDialect, CodirOps (codelet, dep slice).
- `Conversion/SdeToCodir/`, `Conversion/CodirToArts/`.
- `Transforms/` - CodirCodeletOpt, VerifyCodir.
- `Utils/` - CodeletABIUtils.

## ARTS Dialect: `arts`

Paths:

- `include/carts/dialect/arts/IR/`
- `lib/carts/dialect/arts/`
- tests: `lib/carts/dialect/arts/test/`

Purpose: high-level ARTS orchestration IR: EDTs, DBs, epochs, implementation
`scf.for` loops inside tasks/dispatch, barriers, atomics, runtime queries,
lowering contracts, per-block single-writer DB realization, owner maps,
DB modes, DB/EDT graph analysis, and grouped compute/bridge/communication CU
realization.

Limits: ARTS should not become a runtime ABI shim or a place to patch
frontend semantic loss. It should own orchestration invariants and
analysis-backed decisions over committed SDE/CODIR facts. It may verify,
consume, realize, or reject upstream plans; it must not silently recompute
owner dims, block shape, movement family, storage grain, or runtime mode.

Important areas:

- `Analysis/db`, `Analysis/edt`, `Analysis/loop`, `Analysis/heuristics`.
- `Transforms/db`, `Transforms/edt`, `Transforms/epoch`, `Transforms/verify`.
- `Utils/` - DbUtils, EdtUtils, LoweringContractUtils,
  PartitionPredicates, BlockedAccessUtils, ARTSCostModel.

Use `AnalysisManager` accessors for DB/EDT/loop analyses. Do not reach into
graphs directly from passes.

## ARTS-RT Dialect: `arts_rt`

Paths:

- `include/carts/dialect/arts-rt/`
- `lib/carts/dialect/arts-rt/`
- tests: `lib/carts/dialect/arts-rt/test/`

Purpose: flat runtime-facing bridge before LLVM. Runtime ops model epoch
creation/wait, EDT launch, param pack/unpack, dependency records, DB acquire/GEP,
state pack/unpack, dep bind/forward, and call-friendly values.

Limits: ARTS-RT should not introduce new high-level semantics, scheduling
policy, or analysis decisions. If a fix requires understanding OpenMP intent,
DB/EDT graph state, or partition ownership, it likely belongs in SDE, CODIR,
or ARTS.

Important areas:

- `Conversion/ArtsToRt` - implementation directory for the `pre-lowering`
  stage, which lowers chosen ARTS objects into runtime-shaped IR.
- `Conversion/ArtsRtToLLVM` - lower runtime-shaped IR into LLVM dialect
  runtime calls.
- `Transforms/` - LLVM-facing cleanup, runtime-call optimization, data pointer
  hoisting, alias scopes, loop hints, and lowered-form verification.

## TableGen Boundaries

Edit dialect-specific files under `include/carts/dialect/*`. Each dialect
owns its own `IR/*.td` (Dialect, Ops, Attributes, Types) and `Transforms/Passes.td`.

## Grain And Hypergraph Rules

- DB/MU grain and CU/bridge grain are distinct. DBs must be fine enough for
  single-writer concurrency; read-only and copy-like edges should use grouped
  compute/bridge/communication CUs rather than one tiny CU per DB.
- Hypergraph partitioning models MUs as weighted edges over CUs. It is evidence
  for grouping and partition quality over committed MU facts, not a place to
  hardcode benchmark-specific owner dimensions or repair missing SDE layouts.
