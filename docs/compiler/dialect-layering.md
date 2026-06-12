# CARTS Dialect Layering

This document records the intended CARTS dialect layering. The live pipeline in
`tools/compile/Compile.cpp` and `dekk carts pipeline --json` remains the source
of truth for the current implementation.

For execution order, use [`pipeline.md`](./pipeline.md) and
`dekk carts pipeline --json`.
For per-dialect analysis and optimization ownership, see
[`dialects/`](./dialects/).

Scratch investigation records live under `.carts/sessions/...`; they are not
part of the durable docs tree.

## One-Line Rule

SDE proves source semantics and transforms MU/CU/SU shape. ARTS consumes that
shape as the first isolation boundary, realizes explicit deps and params,
and binds the resulting graph to abstract DB/EDT/epoch objects. `arts-rt`
lowers the abstract machine shape to runtime-facing calls.

## Target Stack

The target compiler stack is:

```text
Polygeist -> sde -> arts -> arts-rt -> LLVM
```

The names are part of the layering rule:

- `sde` is a CARTS semantic-decomposition dialect. It is not an ARTS dialect.
- `arts` is the abstract ARTS dialect. It owns the first isolation boundary,
  explicit deps/params, DB/EDT/epoch objects, distributed ownership, and grouped
  execution.
- `arts-rt` is the runtime-facing ARTS bridge. It replaces the documentation
  term "RT"; the textual dialect uses `arts_rt` where MLIR syntax requires an
  underscore.

Namespaces follow the same rule: `mlir::carts::sde`,
`mlir::carts::arts`, and `mlir::carts::arts_rt` are the intended namespace
roots.

The isolation boundary is deliberate. ARTS verifies task bodies, token-local
memory views, scalar params, explicit dependency lists, and abstract ARTS
objects in one dialect before runtime ABI lowering. The IEEE codelet/ARTS paper
referenced by issue discussion is useful prior art, but CARTS keeps the live
compiler stack minimal and shaped around the boundaries below.

## SDE

SDE owns OpenMP semantics, structured program analysis, memref access facts,
scheduling-unit structure, approved `sde.pattern` facts, reductions, barrier
legality, and the memory-unit transformations that make compute units address
the right data slices.

SDE may contain:

- `mu_data`, `mu_alloc`, `mu_dep`, and `mu_token`: memory-unit roots, backing
  storage declarations, source dependency slices, and canonical access-window
  tokens.
- `cu_region`: compute-unit regions before final codelet formation.
- `su_iterate` and `su_barrier`: scheduling-unit iteration spaces, task-shape
  shape facts, and source-level synchronization intent.
- `resource_query <logical_workers>`: target-neutral logical execution
  capacity for symbolic grain arithmetic.
- Structured work facts for pattern classification, chunks, access windows,
  reductions, orchestration groups, and diagnostics.

SDE must not contain ARTS-machine concepts: node count, workers per node,
routes, current node/worker, ARTS runtime topology queries, depv layout, DB
pointer layout, concrete EDT placement, or runtime API decisions.

SDE also should not own the final EDT ABI. SDE authors the MU/CU/SU shape; the
SDE-to-ARTS boundary creates isolated ARTS tasks and object facts from that
shape.

The names are intentional:

- **MU** means memory unit: data roots, tokens, slices, and access modes.
- **CU** means compute unit: executable computation before ARTS binding.
- **SU** means scheduling unit: iteration spaces, barriers, task topology, and
  orchestration intent. `su_barrier` is a synchronization operation inside this
  scheduling layer.
- **SCU** is not a current dialect unit. If CARTS later needs a scheduling
  control abstraction, it should be introduced deliberately. Do not overload SU
  with ARTS-machine control, and do not move ARTS scheduling mechanics into
  SDE.

## ARTS

ARTS is the bridge between SDE semantic shape and runtime-independent ARTS
object realization.

ARTS owns:

- isolated task bodies;
- the complete task boundary: memory deps, control deps, scalar params,
  yielded values, and local-only values;
- token-local memref views and body rewrites derived from the SDE MU/CU/SU
  shape;
- verification that ARTS tasks do not close over values from enclosing regions;
- runtime-independent DB, EDT, dependency, epoch, placement, mode, and owner-route
  object facts;
- ARTS-local canonicalization that is independent of the runtime ABI.

ARTS may contain operations such as:

- `arts.edt`: concrete abstract EDT objects created from isolated ARTS
  task bodies.
- `arts.db_*`: DB allocation, acquire, release, ref, mode, layout, and access
  windows.
- `arts.db_access_window`: direct SDE boundary carrier for committed MU storage
  and access-window facts before acquires are finalized.
- `arts.epoch_*`: abstract epoch grouping, waits, continuation, and CPS shape.
- typed ARTS facts on EDTs, DBs, and epochs while those facts are being
  realized or checked.
- ARTS topology and placement queries selected after SDE has provided logical
  work shape.
- local `scf.for` control flow used to implement dispatch or task-local loops.

The exact op set can evolve. The invariant is not: every value used by an ARTS
task must be local, a dep, a param, or a result of a dep/param-local op. Memrefs
and mutable shared state are deps, not params. Scalars and small immutable
captures are params. Values that can be reconstructed inside the task should be
reconstructed inside the task.

ARTS must not contain source-level OpenMP carriers, semantic loop-family
rediscovery, loop fusion policy, SDE-style distribution planning, or
runtime ABI calls.

The ARTS dialect binds logical worker lanes to the ARTS abstract machine after
SDE has produced logical work shape. If an ARTS pass needs
to infer owner dims, tile legality, task dependence legality, or codelet
captures from raw source-shaped regions, the missing fact belongs earlier.

## ARTS-RT

`arts-rt` is the runtime ABI bridge.

ARTS-RT may contain:

- EDT create calls.
- dependency record calls.
- state and parameter packing.
- depv addressing.
- DB pointer/GUID GEPs.
- epoch runtime calls.
- low-level cleanup needed before LLVM lowering.

ARTS-RT must not choose task grain, stencil layout, loop distribution, DB
layout, or epoch topology from semantic facts. Those decisions must already be
fixed by SDE and ARTS.

## EDT Isolation Rule

Every EDT must be isolated from enclosing SSA values before `EdtLowering`.
EDT creation must enumerate the complete dependency list and the complete
parameter list.

The rule is:

- memory roots, memrefs, mutable shared state, DB handles, token windows, and
  control dependencies are deps;
- scalar firstprivate-style values and small immutable captures are params;
- loop IVs and constants used by the EDT are either params or reconstructed
  inside the body;
- no operation inside an EDT body may reference an SSA value defined above the
  EDT unless that value is passed through a declared dep or param;
- lowering fails if it finds an implicit capture.

This makes `EdtLowering` simpler. It should lower an already-isolated ARTS EDT
object by emitting runtime params, dep records, and the body function ABI. It
should not rediscover captures, infer missing deps, or repair codelet
boundaries. ARTS is the verifier-enforced staging point that makes this true.

## Current Flow

The live implementation uses the direct SDE-to-ARTS spine. The canonical
`sde-planning` stage performs OpenMP-to-SDE conversion and SDE-owned transforms,
then `sde-to-arts` mechanically converts committed SDE storage, access, and
scheduling facts into ARTS objects:

```text
ConvertOpenMPToSde
SdeLoopPatternFacts
SDE transforms
MemoryUnitRealization
RaiseToMuAccessWindow
SdeStorageToArtsDb
SdeAccessesToArtsDeps
FinalizeSdeToArts
RealizeEdtDistribution
VerifySdeLowered
VerifyArtsObjectsOnly
```

The current boundary has one EDT-producing path:

1. SDE MU storage and access windows become `arts.db_alloc`,
   `arts.db_access_window`, and `arts.db_acquire`.
2. SDE CU/SU scheduling structure becomes isolated `arts.edt` bodies with
   explicit deps and params.
3. Remaining SDE work is invalid after `sde-to-arts`; `VerifySdeLowered` and
   `VerifyArtsObjectsOnly` reject it.

`CreateDbs` realizes ARTS storage from the SDE structure it receives.
It must not choose owner dims, tile geometry, dependency-window policy, or
block-local coordinates by inspecting task bodies.

Generated `scf.for` operations are implementation control flow only. They are
not a semantic carrier.

## Target Flow

The production target is the direct SDE/ARTS path:

```text
sde
  mu_data / mu_alloc    data root and storage intent
  mu_token              mode + memref slice
  cu_region             compute body
  su_iterate            task topology
        |
        | SdeStorageToArtsDb / SdeAccessesToArtsDeps / FinalizeSdeToArts
        v
arts
  arts.db_alloc         created from MU storage
  arts.db_access_window committed storage and access-window facts
  arts.db_acquire       created from SDE access windows
  arts.edt              isolated task body with explicit deps/params
  arts.epoch_*          abstract frontier/continuation shape
        |
        | pre-lowering
        v
arts-rt
  runtime calls
```

ARTS should not have an `arts.db_control` operation. There should not be an ARTS
operation whose only purpose is to preserve user-provided dependency metadata
until a later pass rediscovers what SDE already knew. The replacement is:

- `sde.mu_dep` carries source-level dependency slices during SDE planning.
- `sde.mu_token` carries memory access windows for SDE-to-ARTS.
- `sde.control_token` carries ordering/completion only.
- ARTS turns those into explicit codelet deps.
- ARTS turns those deps directly into DB acquires, control edges, and EDT
  creation operands.

## MU/CU/SU Rewrite Rule

Tiling is not valid unless the MU, CU, and SU all agree.

A blocked or sliced MU is not a drop-in replacement for the original whole
memref. A local payload view uses coordinates relative to the slice, while the
source program's memref indices are usually global element coordinates.
Therefore SDE transforms and the SDE-to-ARTS boundary must do all pieces
together:

- choose the CU/SU tile and task schedule;
- choose the MU storage layout and token dependency window;
- rewrite codelet/body accesses so they address the token-local view, including
  ND owner dimensions, halo windows, and strided memref views.

Simple one-dimensional owner slices are covered by the current memref-native
boundary path: the boundary consumes SDE owner facts, creates the acquire
window, and keeps the task body on full MU payload coordinates. That is a
correctness bridge, not the final performance architecture. The final path is
token-local ARTS codelet form, then direct ARTS DB/acquire/EDT lowering.

## Work Shape Rule

The SDE work shape on a unit should include:

- work family: elementwise, stencil, matmul, reduction, wavefront, Jacobi, or
  explicit unsupported diagnostic;
- logical worker capacity or requested logical lanes, expressed as SDE facts or
  `sde.resource_query <logical_workers>`;
- iteration domain: rank, bounds, steps, owner dims, spatial dims, and local
  task-loop shape;
- schedule: source intent and selected logical chunking;
- access facts: per-root read/write mode, offsets, sizes, halo offsets, write
  footprint, owner dims, disjointness proof, and self-read status;
- physical data facts: memory roots, MU token slices, block shape, halo shape,
  layout request, and root data value;
- reduction facts: accumulator, kind, identity, strategy, partial storage
  request, and final exposure;
- orchestration facts: barrier status, timestep/wave group, repetition
  structure, and async strategy;
- codelet boundary facts: deps, params, token-local views, local values, yielded
  results, and diagnostic reason if no codelet can be formed.

The capture rule is explicit:

- scalar firstprivate-style values can become ARTS params and then EDT params;
- dynamic arrays, memrefs, and mutable shared state become deps;
- values that can be constructed locally inside a codelet should be constructed
  locally rather than captured.

## Migration Status

The direct SDE-to-ARTS realization path, ARTS EDT verification, and
frontend-carrier raising/lowering removal is complete. Remaining work is narrower:
finish token-local ARTS view rewrites for every supported benchmark, make DB
creation consume already-authored ARTS facts, and preserve the
SDE/ARTS/ARTS-RT responsibility split.

SDE may request logical capacity; ARTS decides abstract-machine placement;
ARTS-RT lowers the chosen runtime API shape.

## Placement Rules

- Decisions about source meaning, legality, reductions, chunking, data layout,
  and distribution intent live in SDE.
- Isolation, token-local access rewriting, deps, params, DB/EDT/epoch object
  realization, and ARTS-machine binding live in `arts`.
- Runtime ABI mapping lives in `arts-rt`.
- If a pass needs to recover source semantics from ARTS implementation loops,
  the required fact belongs in SDE instead.
- If a utility is reusable, put it in the owning utility namespace instead of a
  local pass helper.
