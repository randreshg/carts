# Proposed SDE Passes (ranked)

> **SUPERSEDED by [`architecture.md`](./architecture.md) Phase 8.** The gap audit
> found this file (a) self-contradicts on `sde-default-tile-floor` (banner says
> "not first", DAG says "first") and (b) specs P0/P1 against now-deleted IR
> (`arrayLayout`/`reduction_strategy`/`sde.redist`/`halo_like`/`getL2CacheSize`).
> `sde-default-tile-floor` is **dropped** (the knob is removed, not defaulted);
> hypergraph grouping re-anchors on the structural `muBlockCount>1` objective; the
> real scaling work is the **A–E levers** as `architecture.md` steps S17–S21. Treat
> the specs below as historical design rationale, not the execution plan.

> **Reprioritized by benchmark evidence — read this first.** The ranking below
> came from a *static* survey of dormant/dead machinery. The subsequent
> *benchmark-grounded* sweep ([`benchmark-grounded.md`](./benchmark-grounded.md))
> changes the priorities, because the CU layer is healthy across all 21 kernels
> and scaling is decided at SU/MU grain and redist geometry. In particular:
>
> - The **top static candidate `sde-default-tile-floor` is now NOT recommended
>   first.** The tile floor (`min_distributed_tile_bytes`, set to ~4 MiB on the
>   megalarge path) is what *coarsens reads* and starves poisson-for/pooling.
>   Activating a nonzero default would fight the #1 benchmark lever.
> - **Implement in this order instead:** (A) reader-grain reconciliation
>   [6 kernels], (B) halo-bearing read access-window [6 kernels], (C) genuine
>   repartitioning redist `target≠source` [4 kernels], (D) one writer-consistent
>   owner grid for FEM [3 kernels], (E) split `block_contraction` K-grain
>   [3 kernels]. See the matrix and lever sites in `benchmark-grounded.md`.
> - `sde-tree-reduction-realization` (P1 below) stays valid and pairs with (A)
>   for the reduction family. `sde-activate-hypergraph-cu-grouping` and the
>   `buildLayoutGraph`/typed-hypergraph wiring remain true *code* facts (dead),
>   but are lower priority than A–E as scaling levers.
> - Confirmed **inert in production** (repair-or-delete, not levers):
>   `elementwise-fusion` (0/21), `iteration-space-decomposition` (0 rewrites),
>   `sde-mu-access-window-sync-opt` (0 barriers removed).

Design specs for new/rewired SDE passes that close the highest-value gaps in
[`optimizations.md`](./optimizations.md). Each respects the engineering
standard: **real shape change in SDE**, owner dims/grain read **verbatim** from
committed layout (never recomputed), **fail closed** with an evidence
diagnostic, and a paired `verify-sde-*` gate wherever a new committed fact is
introduced. Ranking: P0 = activate dormant machinery closing the measured 2-node
distribution-quality gap; P1 = charter-fill realizations reusing existing
emitters; P2 = verifier / contract-surface hygiene. All file:line anchors
verified against `cgo/main`.

> Two patterns the survey deliberately did **not** propose: `StorageGrainReconciliation`
> (intentionally folded into `tiling`'s `selectSingleBudgetWriteLayoutFact`) and
> `RepeatSink` (correctly owned by ARTS `EpochAmortizeRepeatedLoop`).
> Re-introducing either would re-create a deleted, layer-misplaced pass.

---

## P0 · `sde-default-tile-floor`  ·  effect · memory · small  ·  **top candidate**

**Problem.** The whole tile-floor partition path (`chooseCuMuTileFloorPlan` and
the hypergraph caller) is bypassed in `-O3`: every invocation is gated behind
`getMinDistributedTileBytes()>0`, which defaults to `0`
(`include/carts/dialect/arts/Utils/RuntimeConfig.h:117`) and is flipped only by
the opt-in `-min-distributed-tile-bytes` CLI flag that no production pipeline or
dekk wrapper sets. The canonical partition-quality engine and any over-tiling
coarsening never run on a normal build.

**Design.** Derive a default `targetTileBytes` from the cost model's
already-present `getL2CacheSize()` (`SDECostModel.h:58`) / element bytes instead
of from a CLI flag. Replace the `getMinDistributedTileBytes()<=0` short-circuits
in `DistributionPlanning.cpp:995/1022/1061` with a cost-model-sourced value when
the flag is unset (flag still overrides). Bound the floor by the logical-worker
concurrency floor so it never reduces parallelism below a proven requirement.

- **IR before:** fine per-element task waves; hypergraph caller never entered.
- **IR after:** `sde.su_distribute` owner-block tiles sized to
  `max(L2-derived floor, single-writer concurrency floor)`; excess waves
  coalesced; no legal independent work dropped.
- **Placement:** `DistributionPlanning` gate logic (`Compile.cpp:1153`).
  Prerequisite for `sde-activate-hypergraph-cu-grouping` to fire by default.
- **Verify:** lit snapshot shows floor sourced from cost model not 0; benchmark
  gate stream + activations megalarge 64t 1n (over-tiling), all 21 Correct=YES
  with no geomean regression; adversarial check the floor never goes below the
  concurrency floor.
- **Risk:** low-medium. A too-large floor under-exposes parallelism; bounded by
  the concurrency floor.

## P0 · `sde-activate-hypergraph-cu-grouping`  ·  dep · sync · medium

**Problem.** The typed CU/MU hypergraph partitioner — the lambda-1 cut + FM/KL
refinement the vision names as the authoritative CU/bridge grouping engine —
never runs. `refineCuMuAssignment` / `computeCuMuHypergraphCutBytes` are gated on
`memory.typedHypergraph`, which no code populates (only read at
`CuMuGraphPartitioning.cpp:466/468`); the production caller fills only the flat
scalar `hyperedges` pressure list, so CU placement rests on a per-edge scalar
approximation. `buildLayoutGraph`/`summarizeLayoutGraphBalance` (the natural
producer) have zero callers.

**Design.** Wire the existing dead producer to the existing dead consumer. In
`chooseCuMuTileFloorPlan` (`DistributionPlanning.cpp:988-1013`): build a
`CuMuGraphVertex` per CU work-block from `buildOwnerBlockWorkWeights`, a
`CuMuGraphNet` per `LayoutGraphFact` edge already collected by
`collectCuMuHyperedgePressures` (traffic/fanout as net weight), assemble into
`memory.typedHypergraph`, then let the already-implemented
`refineCuMuAssignment` + `computeCuMuHypergraphCutBytes` drive `vertexToPart`.
Emit only CU/bridge **grouping** (projected into `logicalWorkerSlice`); **must
not** touch committed owner dims or `physicalBlockShape`.

- **IR after:** identical committed owner/block facts, but CU/bridge grouping
  selected by the connectivity-cut minimizer; DB/MU grain byte-identical.
- **Placement:** inside `DistributionPlanning` (`Compile.cpp:1153`); depends on
  `sde-default-tile-floor`.
- **Verify:** ship `sde-verify-partition-plan` (below); lit byte-diff stability
  on unaffected kernels + positive grouping snapshot on gemm/2mm/3mm/correlation;
  benchmark gemm/2mm/3mm/correlation megalarge 1n→2n, Correct=YES + no regression.
- **Risk:** medium. Grouping change could perturb correct kernels; mitigate with
  additive-evidence framing + byte-diff stability gate.

## P0 · `sde-time-band-stencil-tiling`  ·  dep · sync · large

**Problem.** Iterative timestep stencils (jacobi-for/poisson-for) are
Correct=YES but anti-scale (poisson-for 1n large 0.191x kernel — 5.23x slower).
The shape is one coarse per-timestep band with per-timestep host-whole bridge
copies. `status.md` names the next owner as real time-band/skew/diamond tiling —
which does not exist (zero `time-band`/`diamond`/`temporal` in
`lib/carts/dialect/sde`; only spatial `WavefrontSkew`).

**Design.** New SDE pass recognizing the repeat/timestep loop carried over a
stencil array (reuse the `full_timestep`/`alternating_buffer_stencil` facts
`barrier-elimination` already commits). Prove Jacobi-style legality; perform a
real time-band/diamond loop rewrite (fuse a band of `T` timesteps into one
tile); widen the halo MU access window by `radius*bandDepth`; commit a
once-per-band `halo_like` `sde.redist`. Reuse `buildWavefrontSkewPlan` /
`realizeWavefrontSkew` for intra-band space skew and `MuLayoutRewriter` for
index rewrites. Fail closed when band legality is unprovable.

- **Placement:** after `tiling`, before `distribution-planning` and
  `barrier-elimination` (`Compile.cpp` ~1148-1153).
- **Verify:** new `verify-sde-time-band` proving halo width `== radius*bandDepth`
  and band legality against committed neighbor-offset facts; benchmark
  jacobi-for/poisson-for megalarge 1n (the 0.191x case) and 1n→2n; seidel-2d
  unaffected (spatial-only fail-closed).
- **Risk:** high. Temporal tiling legality + FP-order/halo correctness is
  subtle; mitigate with strict fail-closed + verifier; single-node and multinode
  shape must stay identical.

---

## P1 · `sde-tree-reduction-realization`  ·  effect · compute · small

**Problem.** `reduction-strategy` sets `reduction_strategy(tree)` as a metadata
attr and explicitly leaves lowering unchanged (`ReductionStrategy.cpp:91/118`).
Only the atomic branch has an in-SDE realizer — the tree verdict is a downstream
promise (charter violation).

**Design.** Mirror `sde-atomic-reduction-realization`. Consume
`reduction_strategy(tree)` by rewriting the leaf-CU accumulator into the
per-block partial-buffer + ordered-combine shape `sde-scalar-block-reduction`
already builds (reuse its expanded-`mu_alloc` emitter, owner-strip partial-fill
`su_iterate`, deterministic per-slot combine), then strip the consumed reduction
operands/attrs. Preserve `partialReductionOwnerDims` from committed layout. Fail
closed when not a legal rank-0 add over a committed block layout.

- **Placement:** immediately after `sde-atomic-reduction-realization`
  (`Compile.cpp:1157`).
- **Verify:** `verify-sde-mu-layout` already gates the expanded partial buffer;
  lit tree-verdict snapshot + fail-closed snapshot; deterministic checksum
  unchanged.
- **Risk:** low. Heavy reuse; FP-order determinism handled by per-slot partials.

## P1 · `sde-chained-matmul-align`  ·  dep · memory · medium

**Problem.** 2mm/3mm under-scale vs gemm (64t 1n: gemm 15.12x, 2mm 7.61x, 3mm
4.75x) because the chained intermediate `B` in `B=A*X; C=B*Y` round-trips a
`storageBridgeCopy`: SDE commits matmul physical layout per-SU with no alignment
of B's producer-write grain to its consumer-read grain.

**Design.** Extend `elementwise-fusion` (or a new pass) so that for a producer
matmul writing `B` and a consumer matmul reading `B`, B's committed owner block
grain is aligned across both SUs. Reuse `commitMatmulPhysicalLayout` owner-dim
selection and `rebuildMergedLayoutEntry`/`applyMergedLayoutAttrs`. Fail closed to
an **explicit `sde.redist`** when grains cannot be made compatible (never a
silent coarse bridge).

- **Placement:** with `elementwise-fusion` (`Compile.cpp:1149`), before
  `distribution-planning`.
- **Verify:** `verify-sde-physical-consistency` + `verify-sde-redistribute`; lit
  2mm/3mm intermediate-grain snapshot shows no host_whole bridge; benchmark
  2mm/3mm megalarge 1n, gemm + 7 others byte-identical.
- **Risk:** medium. Aligning across two contraction owner-dim choices can
  over-constrain one matmul; fail-closed to redist preserves correctness.

## P1 · `sde-parallelize-reductions`  ·  dep · compute · medium

**Problem.** `sde-parallelize` raises only write-only host inits
(`Parallelize.cpp` "no read from a written root"); sequential host reduction
loops (stream checksum; layernorm/batchnorm mean/variance) stay serial, forcing
a coarse host_whole shadow DB that funnels tens of GB on one node — a dominant
measured anti-scaling cause. The `partialReductionOwnerDims=[0,1]` vs
`planOwnerDims=[0]` mismatch makes the owner-local reduction split cross-node
every iteration.

**Design.** Admit reduction host loops — raise to a tree/partial reduction
(reuse the `sde-tree-reduction-realization` shape) over committed `arrayLayout`
owner dims, stamping `partial_reduction` with owner dims taken **verbatim** from
the committed layout so partial dims match plan dims — and verification loops as
parallel-effect CUs. Keep the reduced read input `compute_block`. Fail closed on
non-associative/aliasing reductions.

- **IR after:** per-node partial-reduce `su_iterate` over committed owner dims +
  cross-node `reduce_scatter_like`/`allreduce_like` redist; input stays block.
- **Placement:** `Parallelize` stage (`Compile.cpp:1140`) or a companion before
  `layout-assignment`.
- **Verify:** `verify-sde-redistribute` gates the emitted family; benchmark
  stream + layernorm + batchnorm megalarge 1n→2n, checksum-match, funnel removed.
- **Risk:** medium. FP-order/associativity + owner-dim agreement; tie partial
  dims to committed layout to avoid the `[0,1]`-vs-`[0]` cross-node split.

---

## P2 · `sde-verify-partition-plan`  ·  dep · sync · small

**Problem.** `partition_graph` grain is a consumed attr with **no** structure-
recompute mirror verifier (every other SDE consumed-attr has a `verify-sde-*`
companion). A stale partition grain can drift into an ARTS-RT fault.

**Design.** `VerifySdePartitionPlan` recomputes the CU/MU partition grain from
committed MU structure + hyperedge facts (reuse the same vertex/net assembly as
`sde-activate-hypergraph-cu-grouping`) and rejects a `partition_graph` attr that
disagrees, mirroring `verify-sde-physical-consistency`. Fail closed.

- **Placement:** after `distribution-planning` (`Compile.cpp` ~1153). Ships with
  `sde-activate-hypergraph-cu-grouping`.
- **Verify:** lit positive (consistent passes) + negative (corrupted attr fails);
  full suite byte-identical.
- **Risk:** low. Pure verifier.

## P2 · `sde-complete-cps-step0-cleanup`  ·  state · state · small

**Problem.** The CPS Step-0 contract-surface deletion is incomplete:
`VerifySdeCpsPlan` is gone but `asyncStrategy` survives as a declared
`OptionalAttr` threaded as nullptr builder args
(`ConvertOpenMPToSde.cpp:592/742/808`, `Interchange.cpp:900`).

**Design.** Confirm zero non-test branches on `asyncStrategy`, then drop
`SdeAsyncStrategyAttr` from `SdeOps.td` and remove the nullptr builder threading.
(Async timestep pipelining, if later wanted, belongs in ARTS over
`arts.codelet`, not as an SDE attr.)

- **Placement:** dialect/op-definition + builder edit (not a pipeline pass).
- **Verify:** build + full SDE lit green; grep confirms zero non-test consumers
  before deletion.
- **Risk:** low. Mechanical.

---

## Dependency order

```
sde-default-tile-floor (P0) ──► sde-activate-hypergraph-cu-grouping (P0) ──► sde-verify-partition-plan (P2)
sde-tree-reduction-realization (P1) ──► sde-parallelize-reductions (P1)
sde-time-band-stencil-tiling (P0, independent, large)
sde-chained-matmul-align (P1, independent)
sde-complete-cps-step0-cleanup (P2, independent)
```

`sde-default-tile-floor` is the recommended first implementation: smallest,
lowest-risk, and the gate that makes the dormant partition path (and the P0
hypergraph wiring) reachable in production `-O3`.
