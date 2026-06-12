# SDE Dialect

SDE is the first dialect in the CARTS lowering spine
(**SDE → ARTS → ARTS-RT**). It is runtime-agnostic parallel IR: it commits
*real* layout, dependency, and effect transformations over three units —
**CU**, **SU**, **MU** — and never names a concrete runtime object (no EDT, DB,
epoch, owner map, collective, route, GUID, node, or worker).

> **Start here for the revised design:** [`architecture.md`](./architecture.md) —
> the consolidated target architecture (one principle, the SDE-sync → ARTS spine,
> the affine/`ValueBounds` analysis layer, the no-contract pass pipeline, the
> async-dialect reuse boundary, what gets deleted, and the migration order). The
> docs below are the as-built reference and the file:line derivations behind it.

## What "SDE" stands for

The acronym is glossed three ways in this tree. Document and code should
converge on the first; the other two are noted so a reader is not confused.

| Gloss | Where | Status |
| --- | --- | --- |
| **State, Dependency, and Effect** (over MU/CU/SU) | `docs/vision/compiler.md:7`, `docs/vision/plan.md`, every cgo/carts skill, and the in-tree boundary verifier `Transforms/Verify/VerifySde.cpp:4` | **Authoritative** — used by every governing doc and the boundary gate. |
| "Structured Decomposition Environment" | `IR/SdeDialect.td:15` (and `.h`/`.cpp`, `Transforms/Passes.h:4`) | Legacy IR-header name; stale, scheduled to be updated to the authoritative gloss. |
| "semantic decomposition dialect" | this file, historically | Removed below. |

The load-bearing in-code acronyms are the **unit** names (CU/SU/MU), which are
unambiguous and consistent everywhere.

## The three units

Defined verbatim in `IR/SdeDialect.td:17-20` and banner-sectioned in
`IR/SdeOps.td`:

- **CU — Compute Unit** ("what computation happens"). Executable leaves:
  `cu_region`, `cu_task`, `cu_reduce`, `cu_atomic`, `cu_work`. A CU may **not**
  contain SU scheduling ops — SUs compose CUs, not the reverse (enforced by
  `verifyCuContainsNoScheduling`, `IR/SdeOps.cpp:34-49`).
- **SU — Scheduling Unit** ("how/when it runs"). `su_iterate`, `su_distribute`,
  `su_barrier`. SU bodies are scheduling-only and carry the committed layout
  fact source-of-truth (`arrayLayout`, owner/block shapes, topology).
- **MU — Memory Unit** ("data access and movement"). `mu_alloc`, `mu_data`,
  `mu_access_window`, `redist`, `mu_reduction_decl` (+ the deprecated-but-present
  `mu_dep` / `mu_token`).

## The State / Dependency / Effect taxonomy

The `lib/carts/dialect/sde/Transforms/` tree is split into the three axes the
authoritative gloss names, plus the boundary gates:

| Dir | Axis | What it transforms |
| --- | --- | --- |
| `state/` | **State** | MU realization, layout → memref-type structure, access windows, redistribution, coarse avoidance. |
| `dep/` | **Dependency** | parallelization, per-array BLOCK layout assignment, loop interchange/tiling/fusion, iteration-space decomposition. |
| `effect/` | **Effect** | distribution planning, schedule/chunk/reduction strategy, barrier elimination. |
| `Verify/` | (gates) | fail-closed boundary verifiers, one per shape-mutating transform. |

One known mismatch: `SdeLoopPatternFacts.cpp` lives in `state/` but registers in
`DepPasses.td:69` (it is a Dependency-axis pass by registration).

## Engineering invariants

- **CU/MU/SU only.** SDE never reasons about EDT/DB/epoch/owner-map/route. This
  rule has been violated and corrected twice; treat it as load-bearing.
- **Real transformations, not metadata promises.** A pass that has enough
  information to transform must transform, or **fail closed with evidence**
  (`signalPassFailure` + a located diagnostic). It must not stamp a fact for a
  later layer to repair.
- **Consume verbatim, never recompute.** Downstream passes (and ARTS) consume
  committed owner dims / block shape / movement family as-is; they do not
  re-derive a different shape.
- **Keep DB/MU storage grain separate from CU/worker dispatch grain.**

## Documents in this directory

- [`ops.md`](./ops.md) — op, type, and attribute reference (source-anchored).
- [`passes.md`](./passes.md) — the 36 registered passes and the imperative
  pipeline order in `tools/compile/Compile.cpp`.
- [`analysis.md`](./analysis.md) — the analysis/utils substrate and the
  what-is-wired-vs-dead register.
- [`optimizations.md`](./optimizations.md) — the canonical optimization catalog
  across the six value dimensions (with dormant/dead/missing levers marked).
- [`benchmark-grounded.md`](./benchmark-grounded.md) — CU/SU/MU and every
  transform read from real SDE IR across the 21 core benchmarks: the
  scaling-lever matrix, kernel families, the four concentrated lever sites, and
  the benchmark-ranked top opportunities.
- [`proposed-passes.md`](./proposed-passes.md) — ranked design specs for new
  SDE passes (reprioritized by `benchmark-grounded.md`).
- [`design-revision.md`](./design-revision.md) — the design-time revision:
  minimize attributes/contracts/knobs (push facts into ops and types), add
  first-class SU movement ops (`su.halo`, `su.reduce_scatter`, …), add a general
  `raise-to-sde` pass; with a critic-reviewed 12-step migration plan.
- [`op-level-verification.md`](./op-level-verification.md) — eliminating the 8
  standalone verify passes by moving verification to op-level ODS verifiers (the
  ARTS/EDT model); achievable end state 8 → 2 (or 3), with the migration.
- [`arts-all-to-all.md`](./arts-all-to-all.md) — the ARTS realizer for
  `su.all_to_all` (genuine cross-owner repartition): reader-pull per-target-block
  single-writer gather; order-preserving tier now, permuted tier via a `DbAlloc`
  owner-dims attribute; the self-edge fix for 3mm/2mm/atax/bicg.
- [`pipeline-diagram.md`](./pipeline-diagram.md) — end-to-end diagram of the
  revised pipeline (macro spine, detailed pass sequence with NEW/REVISED/REMOVED,
  attribute→type/op shift, SU movement-op placement, op-level verification model).
- [`pass-walkthrough.md`](./pass-walkthrough.md) — pass-by-pass trace of the
  revised pipeline (incl. `raise-to-sde` and the SU movement ops) over **GEMM**
  and **jacobi-for** side-by-side, with `[REAL]` IR + `[PROJECTED]` deltas.
- [`pass-walkthrough-gemm.md`](./pass-walkthrough-gemm.md) — GEMM only: the IR
  evolving top-to-bottom through the revised pipeline (one trace per kernel).
- [`pass-walkthrough-jacobi.md`](./pass-walkthrough-jacobi.md) — jacobi-for only:
  the IR evolving top-to-bottom through the revised pipeline.

The most complete design-level reference is
`docs/vision/references/dialect-redesign-sde.md` (keep/add/remove tables and
sequencing); this directory is the as-built reference for the current tree.
