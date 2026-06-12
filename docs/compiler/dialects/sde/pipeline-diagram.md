# Revised SDE Pipeline — End-to-End Diagram

A visual, analyzable view of the pipeline **as it would look after the revision
in [`design-revision.md`](./design-revision.md)** (the source of truth). This is
the target state, not the current tree. Legend used throughout:

```
[NEW]      op/pass/edge that does not exist today
[REVISED]  exists today, changes behavior
[REMOVED]  exists today, deleted by the revision
[→type]    fact that moves from an attribute into the rank-expanded memref TYPE
[→op]      fact that moves from an attribute into a first-class SU movement OP
(op-verify) verification now lives in the op's ODS verifier, not a pass
```

---

## 1. Macro spine — source to executable

```
  C / C++ / OpenMP source
        │  cgeist (Polygeist)
        ▼
  MLIR: func + scf/affine + memref + (omp.* when present)
        │
        ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │ SDE  — State / Dependency / Effect over CU / SU / MU                      │
 │ runtime-agnostic parallel IR. Commits REAL layout/movement as TYPES+OPS. │
 │ Names no EDT/DB/epoch/owner-map/route/collective/runtime-policy.         │
 └─────────────────────────────────────────────────────────────────────────┘
        │  SDE→ARTS boundary (first isolation boundary)
        ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │ ARTS — realizes DBs / EDTs / owner maps / grouped CUs over committed SDE  │
 │        shape. (The verification MODEL SDE now copies: op-level, 0 passes) │
 └─────────────────────────────────────────────────────────────────────────┘
        │
        ▼
   ARTS-RT  — mechanical lowering to runtime calls
        │
        ▼
   LLVM IR → executable (GASNet-EX transport for multinode)
```

---

## 2. The revised SDE compile pipeline (detailed)

Three imperative builders in `tools/compile/Compile.cpp`. **The biggest visible
change: the verify-* passes that were interleaved through planning are gone —
verification is attached to the ops (right column).**

```
╔══ buildSdeInputNormalizationPipeline ════════════════════════ (UNCHANGED) ══╗
║  promote-target-attrs → lower-affine → CSE → input-inliner →                ║
║  canonicalize → scalar-forwarding → memref-normalization →                  ║
║  handle-deps → dead-state-cleanup                                           ║
╚════════════════════════════════════════════════════════════════════════════╝
                                   │
                                   ▼
╔══ buildSdePlanningPipeline ═══════════════════════════════════ (REVISED) ═══╗
║                                                          op-level checks ──► ║
║  RAISE  (convert FIRST, then raise the missing things)                       ║
║   ├─ convert-openmp-to-sde (FRONT)  [REVISED] omp.* regions → shared          ║
║   │     buildSuIterate helper, then DECORATE schedule/chunk/nowait/redKind   ║
║   └─ raise-to-sde (CORE)            [NEW]   subsumes sde-parallelize +       ║
║         sde-cu-normalization + scf.parallel: raises the MISSING non-omp      ║
║         scf/affine nests → su_iterate+cu_region<parallel>; wraps residual    ║
║         host work → cu_region<single>; normalizes SU bodies; proof-derived   ║
║         CU kind (fixes hardcoded single); re-entrancy guard skips converted  ║
║   ✗ loop-pattern-facts              [REMOVED as a pass → recomputed analysis] ║
║                                                                             ║
║  DEPENDENCY axis (dep/)   (classification/pattern/offsets = ANALYSIS, not    ║
║                            a pass — recomputed on demand by SuLoopAccessAna.) ║
║   ├─ layout-assignment              [REVISED] commits CONSUMER required-     ║
║   │     read layout as the consumer mu_alloc TYPE  [→type]                   ║
║   ├─ loop-interchange                                                        ║
║   ├─ tiling                         [REVISED] reader-grain reconcile too     ║
║   │     (not writer-only) → no coarse reads                                  ║
║   └─ elementwise-fusion                                                      ║
║   ✗ schedule-refinement             [REMOVED]  (→ optional OMP-front input)  ║
║   ✗ chunk-opt                       [REMOVED]  (→ optional OMP-front input)  ║
║                                                                             ║
║  EFFECT axis (effect/)                                                       ║
║   ├─ distribution-planning          [REVISED] NO min_distributed_tile_bytes  ║
║   │     knob; owner/block facts → realized as type downstream                ║
║   ├─ iteration-space-decomposition                                           ║
║   └─ barrier-elimination                                                     ║
║   ✗ reduction-strategy (enum)       [REMOVED]  tree → realized as op below   ║
║                                                                             ║
║  STATE axis (state/)  — realize facts as STRUCTURE                           ║
║   ├─ memory-unit-realization                              (op-verify mu_alloc)║
║   ├─ atomic-reduction-realization → sde.cu_atomic                            ║
║   ├─ tree-reduction-realization     [NEW] → su.reduce_scatter  [→op]         ║
║   ├─ (re-normalize CU containment — now internal to the realization passes) ║
║   ├─ rank-expand-mu                  block grid becomes memref TYPE [→type]  ║
║   │                                          (op-verify: VerifySdeMuLayout R1)║
║   ├─ scalar-block-reduction                                                  ║
║   ├─ raise-to-mu-access-window      [REVISED] emits su.halo radii as an      ║
║   │     ACQUIRE SLICE; validExtents stays == blockExtent  [→op]              ║
║   ├─ mu-access-window-sync-opt                                               ║
║   └─ redistribute                   [REVISED] emits su.halo / su.reduce_     ║
║         scatter under su_distribute; target≠source; NO self-edge   [→op]     ║
║   ✗ verify-sde-physical-consistency [REMOVED → op-verify / vanish-with-type] ║
║   ✗ verify-sde-mu-layout (R2)       [REMOVED → tautology gone w/ one type]   ║
║   ✗ verify-sde-mu-access-window     [REMOVED → mu_alloc/mu_access_window op]  ║
║   ✗ verify-sde-mu-access-window-sync[REMOVED → SdeSuBarrierOp::verify]        ║
║   ✗ verify-sde-coarse-avoidance     [REMOVED → impossible once type-realized]║
║   ✗ verify-sde-redistribute         [REMOVED* → per-op verifiers on su.*]     ║
║   ✗ verify-sde                      [REDUCE→1 arm] foreign-op CU-containment   ║
╚════════════════════════════════════════════════════════════════════════════╝
                                   │
                                   ▼
╔══ buildSdeToArtsPipeline ═════════════════════════════════════════ boundary ╗
║  SdeStorageToArtsDb → SdeAccessesToArtsDeps → FinalizeSdeToArts             ║
║  ✗ verify-sde-lowered  [REMOVED → ConversionTarget.addIllegalDialect<sde>]  ║
║     ("no sde.* op survives" is conversion legality, not a verifier)         ║
╚════════════════════════════════════════════════════════════════════════════╝
                                   │
                                   ▼
                          ARTS planning → ARTS-RT → LLVM
```

**Net pass-count change:** raise gains 1 (`raise-to-sde`), loses 1
(`sde-parallelize`, subsumed); effect/scheduling loses 2 (`schedule-refinement`,
`chunk-opt`); state gains 1 (`tree-reduction-realization`); **verification loses
all 8 standalone passes** (→ op verifiers + conversion legality, with at most one
thin residual for the barrier-sync cross-phase check, pending the companion
op-level-verification analysis).

---

## 3. Where facts live — attribute → type / op

The single organizing move. Today one fact is encoded up to three times
(dict attr + `physical*` attr + the rank-expanded type); the revision keeps **one**
representation.

```
              TODAY (attributes / contracts)              REVISED (structure)
 ┌──────────────────────────────────────────┐   ┌──────────────────────────────┐
 │ su_iterate {                              │   │ su_iterate {                 │
 │   physicalOwnerDims  = [0,1]   ───────────┼──►│   lowerBounds/upperBounds/   │
 │   physicalBlockShape = [437,600]──────────┼──►│   steps,                     │
 │   iterationTopology  = owner_tile_2d ─────┼─► │   reductionAccumulators,     │
 │   arrayLayout = {id,kind,owner,block} ────┼─► │   reductionKinds, nowait     │
 │   arrayId = 7 ; layoutsDisagree=[..] ─────┼─X │ }                            │
 │   commVolumeBytes = 46080000 ─────────────┼─X │  (everything else GONE)      │
 │   physicalHaloShape = [1,1] ──────────────┼─► sde.su_halo (op)              │
 │   distributionKind = blocked ─────────────┼─► sde.su_distribute (wrapper)   │
 │   reductionStrategy = tree ───────────────┼─► sde.su_reduce_scatter (op)    │
 │   schedule/chunkSize ─────────────────────┼─X (OMP-front optional input)    │
 │ }                                         │   │                              │
 │ mu_alloc : memref<128x64xf64>             │   │ mu_alloc :                   │
 │   + the 11 attrs above                    │   │   memref<8x16x... > (grid    │
 │                                           │   │   prefix = owner dims,       │
 │                                           │   │   tail = block extents)      │
 │ sde.redist { family, src=tgt geometry } ──┼─► sde.su_halo / su_reduce_      │
 │   (self-edge: target == source) ✗BUG      │   scatter (target≠source)       │
 └──────────────────────────────────────────┘   └──────────────────────────────┘
        ──►  becomes TYPE structure        ──►(op)  becomes a first-class op
        ──X  deleted entirely
```

`su_iterate`'s effect/schedule/layout attribute surface drops from **~27 → ~6**.

---

## 4. SU movement ops in the IR

Movement is no longer a `sde.redist` fact-op carrying a same-geometry attribute
pair; it is a real op under `su_distribute`, sequenced between the producer and
consumer `su_iterate`, with source and target as the operand/result **types**.

```
 sde.su_distribute blocked {
   sde.su_iterate (producer)  ...            // writes %A at producer layout (type P)
        └─ sde.cu_region<parallel> { ... }

   sde.su_reduce_scatter %A { reduceDim, kind }  : memref<P> -> memref<P>   [→op]
        // reduction movement: source==target type is LEGITIMATE here
   //  ── or, for a stencil read edge: ──
   sde.su_halo %A { radiusLo=[1,..], radiusHi=[1,..] } : memref<P> -> memref<P>  [→op]
        // halo radii ride as an ACQUIRE SLICE; validExtents stays == blockExtent

   sde.su_iterate (consumer)  ...            // reads %A at consumer layout (type C)
        └─ sde.cu_region<parallel> { ... }
 }

 // Genuine repartition (consumer type C ≠ producer type P) closes the self-edge:
 //   sde.su_all_to_all %A : memref<P> -> memref<C>     [SPECIFIED — ARTS realizer:
 //                                                       tier1 order-preserving NOW,
 //                                                       tier2 permuted needs DbAlloc
 //                                                       owner-dims attr; see
 //                                                       arts-all-to-all.md]
 //   sde.su_scatter / su_gather / su_broadcast          [degenerate cases of the
 //                                                       same realizer]
```

Rules (all enforced in op-verifiers / traits, no pass):
- legal **only** as a direct child of `sde.su_distribute` (trait + `isCuForbiddenSchedulingOp`); never inside a `su_iterate`/CU body.
- **no tokens** — ordering is structural; `VerifySde`'s no-dataflow-graph rule holds by construction.
- **identity move (target==source) is a verifier error** for repartition ops — the old hardcoded self-edge cannot recur.

Live now: `su.halo`, `su.reduce_scatter`. **Specified with ARTS realizer:**
`su.all_to_all` (genuine repartition — tier1 order-preserving now, tier2 permuted
via a `DbAlloc` owner-dims attr; [`arts-all-to-all.md`](./arts-all-to-all.md)).
Degenerate cases of that realizer: `su.scatter`, `su.gather`, `su.broadcast`.
(`su.allreduce` cut = `reduce_scatter ∘ broadcast`.)

---

## 5. Verification model — EDT-style, op-level

The ARTS model SDE adopts: **every invariant lives in the op** (verifier / trait /
interface). The honest outcome from the per-pass analysis + adversarial review:
**8 verify passes → 2 (or 3)** — two checks have no host op. Full detail in
[`op-level-verification.md`](./op-level-verification.md).

```
   TODAY                                   REVISED
   8 ModuleOp verify-* passes              op verifiers + traits  (+ 2 irreducibles)
   interleaved through the pipeline        attached to the op definition
   ───────────────────────────────        ──────────────────────────────────────────
   verify-sde-physical-consistency ─────►  SdeSuIterateOp::verify (arms vanish w/ type)
   verify-sde-mu-layout R2        ───────►  SdeMuAllocOp::verify (true-writer extent)
   verify-sde-mu-layout R1        ───────►  SdeMuAllocOp::verify / core memref verifiers
   verify-sde-mu-access-window    ───────►  SdeMuAllocOp + SdeMuAccessWindowOp::verify
   verify-sde-mu-access-window-sync ────►  SdeSuBarrierOp::verify (bounded sibling scan,
                                           classifyBarrierSync verbatim)
   verify-sde-coarse-avoidance    ───────►  DELETE (verify-twin of an optional rewrite)
   verify-sde-redistribute        ───────►  SdeRedistOp/su.* verifiers + child-whitelist
                                           trait  (*1 arm conditional, see below)
   ─── irreducible (no host op) ───────────────────────────────────────────────
   verify-sde (1 arm)             ──stays► "source compute outside any CU" — foreign
                                           trigger op, function-scope; can't be op-local
   verify-sde-lowered             ──stays► "no sde.* op survives" — dialect-absence,
                                           O(N) barrier (boundary is a hand-written
                                           walk, no ConversionTarget to host it)
   (conditional 3rd)              ──maybe► redistGroundedInCommittedLayout: 3 module
                                           walks by arrayId; vanishes ONLY if the
                                           movement operand is the home-writer's SSA def
```

Why this is sound (MLIR mechanism): an op `hasVerifier` may inspect its own
attrs/operands/results/regions, each operand's **defining op** (≤2 hops), the
**parent**, a self-region walk skipping nested same-kind ops, and uses — so most
"cross-op" checks are op-local-reachable. Structural rules → **traits**. Two
checks genuinely cannot be op-local (a *foreign* trigger op, and a dialect-absence
predicate), so they stay as thin O(N) residual passes. The load-bearing rule the
review pinned: the tautology-avoidance checks must resolve their extent via the
**true-writer** path and fail closed on the reader fallback — never collapse into
"attribute equals itself."

---

## 6. The CU/SU/MU data model (what flows through)

```
        ┌── MU (Memory Unit) ──────────── storage = rank-expanded memref TYPE
        │     mu_alloc / mu_data / mu_access_window
        │     movement = su.halo / su.reduce_scatter / (roadmap su.*)
        │
  SU ───┤── SU (Scheduling Unit) ───────  schedule = su_iterate / su_distribute /
 owns   │     su_barrier; movement ops live here, under su_distribute
 the    │
 shape  └── CU (Compute Unit) ──────────  executable leaf = cu_region<parallel|
              cu_region / cu_task /         single> / cu_reduce / cu_atomic / cu_work
              cu_reduce / cu_atomic         (proof-derived kind; almost always healthy)
```

Reading order for "does this kernel scale": **SU** owner/block grain (now the
memref type) + **movement ops** present → not the CU. The revision makes that grain
a type you can read off the IR directly, and the movement an op you can see — no
attribute archaeology.

---

## How to read this against the plan

- Pipeline order + pass deltas → `design-revision.md` Part 3 (raise-to-sde) and
  Part 4 (migration, the 12 ordered steps).
- Attribute→type/op table → `design-revision.md` Part 1.
- Movement op signatures → `design-revision.md` Part 2.
- Per-verify-pass op-level destinations → [`op-level-verification.md`](./op-level-verification.md)
  (8 passes → 2/3, with the EDT model and the convert-then-delete migration).
- Why each lever matters per kernel → `benchmark-grounded.md`.
