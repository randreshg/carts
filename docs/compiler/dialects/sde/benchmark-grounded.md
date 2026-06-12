# SDE from the Benchmarks' Point of View

What CU/SU/MU actually look like, and where each SDE transform helps or hurts,
read from real SDE-stage IR across the 21 core benchmarks (stream; jacobi-for,
poisson-for; activations, batchnorm, layernorm, pooling; gemm, 2mm, 3mm, atax,
bicg, correlation, convolution-2d/3d, jacobi2d, seidel-2d; volume-integral;
stress, velocity; vel4sg-base). Built from a benchmark-grounded multi-agent
sweep (per-benchmark × per-unit reads → per-kernel adversarial verify →
per-transform cross-cut → synthesis). Claims trace to dumps under
`.carts/outputs/*/2_sde` and root `*.sde-planning.mlir`.

> **Confidence caveats.** Most polybench/ml dumps are **single-node**
> (`commVolumeBytes=0` throughout); grain disagreements are verified in IR but
> their 2-node payoff is inferred against the measured "CARTS does not scale at
> 2n" baseline. `volume-integral` MU has **no live dump** at HEAD (fails closed);
> its "1.57x" is from a stale commit. `stress`/`velocity`/`vel4sg` verifier
> verdicts come from a slightly **staler in-tree verifier**. Treat those as
> lower confidence.

## The headline

**The CU layer is almost always healthy.** Across all 21 kernels there are *no*
over-tiled CU explosions and *no* `cu_task`/`cu_reduce`/`cu_atomic` pathologies
(those counts are zero suite-wide; reductions are in-CU `iter_args` scalars).
**Scaling is decided one and two levels down**, at the **SU** (`commVolumeBytes`
+ `muBlockCount`) and the **MU** (`redist` geometry). A CU that looks perfect
(`poisson-for`, `velocity`) can still fail to scale, or fail to lower at all,
because the SU grain starves it or the MU window fails the verifier. The unit you
read for "does this scale" is the SU `commVolumeBytes`/`muBlockCount` and the MU
`redist` geometry — not the CU.

## CU / SU / MU, grounded

- **SU = Scheduling Unit** — one `sde.su_iterate` (optionally in
  `su_distribute<blocked|owner_compute>`) carrying the committed layout facts for
  one source loop. *Healthy:* `stream` (5 SUs, 1-D owner-strip, producer/consumer
  grains match, `commVolumeBytes=0`, no barriers); `gemm` (88 owner-tiles, single
  static wave). *Pathological:* (1) **read/write grain disagreement** —
  `batchnorm` writes output at 256 blocks but its reductions read it at 2 (128×),
  no `layoutsDisagree` authored; `jacobi-for` writes `[512,512]`/400, reads
  `[5120,5120]`/4; (2) **block-count throttle** — `poisson-for`'s correct halo SU
  is pinned to `muBlockCount=4` by the 4 MiB tile floor; (3) **mislabeled
  barriers** — jacobi-for/poisson-for keep a full `su_barrier{unknown_required}`
  where a per-block halo frontier would do.
- **MU = Memory Unit** — one `sde.mu_alloc` + rank-expanded block-grid type +
  per-CU `sde.mu_access_window`; `sde.redist` names movement. *Healthy:* `stream`
  (2 MiB budget blocks, zero coarse); `2mm` intermediate `tmp` (block-native, no
  redist). *Pathological:* (1) **coarse contraction home** (`gemm` B, `2mm`/`3mm`
  operands frozen `block_contraction muBlockCount=2`); (2) **halo dissolved into
  index math** — `stress`/`velocity`/`vel4sg` rank-expand to 6-D grids but every
  window is the writer's own footprint with **no halo widening**; ±1 reads become
  `divui`/`remui` (114 div + 114 rem in vel4sg) the window doesn't see; (3)
  **identity-geometry redist** — *every* `sde.redist` in the corpus has
  `from == to` (target hard-coded to source), so the named reduce-scatter/gather
  moves nothing across owners (3mm half-checksum at 2n); (4) **two MUs for one
  tensor** (`batchnorm` `x` owner-tile cube vs `output` channel-strip → "copy" is
  a full reshuffle); (5) **fail-closed writer-grid mismatch** (`stress`/`velocity`/
  `vel4sg`: grid anchored on the init writer, compute writer permutes owner order
  → `VerifySdeMuAccessWindow` fails, 0-byte binary).
- **CU = Compute Unit** — the executable leaf `sde.cu_region<single|parallel>`.
  *Healthy:* one fat data-parallel leaf per compute phase, serial work confined to
  amortized `<single>` prologues/checksums. *Pathological (rare, never
  over-tiling):* serial `<single>` on the compute path (`batchnorm`/`pooling`
  init: `Parallelize::createSuIterateForNest` hardcodes `cu_region<single>`,
  discarding proven parallelism); a serial `<single>` checksum tail bounding E2E;
  "structurally healthy but starved" (`poisson-for` CU fine, grain caps it at 4
  blocks).

## Benchmark → scaling-lever matrix

| Kernel | Class | Pathology (unit) | Real shape change that fixes it | Layer | Status |
|---|---|---|---|---|---|
| stream | bandwidth, **at optimum** | none | none in SDE; AVX/NUMA/EDT-orchestration | runtime/codegen | no SDE fix |
| jacobi-for | stencil, ~100× comm | SU coarse read [5120²]/4 vs write 400; halo redist sized coarse | narrow read window to budget grain + ±1 halo; size halo redist fine | SDE (Tiling read-reconcile + RaiseWindow) | diagnosed |
| poisson-for | stencil, breadth-capped | MU `muBlockCount=4` from 4 MiB floor | decouple `muBlockCount` from floor-collapsed grain | SDE (DistributionPlanning) | diagnosed |
| batchnorm | reduction+EW, 128× read | reduction reads output at 2 vs write 256, no `layoutsDisagree` | reconcile reduction **read** grain to committed write grain | SDE (Tiling/DistributionPlanning read-reconcile) | ~parity |
| pooling | reduction+EW, 256× read | input read coarsened 512→2 by tile floor | reconcile read grain to writer | SDE (DistributionPlanning) | diagnosed |
| gemm | matmul, ~1.52× | operand B `block_contraction muBlockCount=2` → reduce_scatter 46 MB | widen B K-grain to budget / co-owner-tile | SDE (LayoutAssignment) | diagnosed |
| 2mm | chained matmul | B/C frozen `block_contraction` | split B/C K-grain | SDE (LayoutAssignment) | diagnosed |
| 3mm | chained matmul, 2n half-checksum | coarse reads **+ identity self-edge redist** | author redist with target≠source so the chain edge unions halves | SDE (RedistributionEdges) | root 2n gap |
| atax | matvec reduction, 480× | A read at 2 vs budget 960; partials never combined | restore 960 read grain + fix step2 dep-result-dim maps | SDE + CU→EDT maps | under-delivers |
| bicg | dual reduction | A sole coarse operand; cross-owner read 1.06 GB/step | dual-layout A / drop to budget + narrow CU2 window | SDE (LayoutAssignment+window) | diagnosed |
| correlation | reduction+matmul, **2.08× PASS** | latent: O(n³) corr read window full-data, `commVolume=0` | author partial-reduction split / explicit movement for >2n | SDE (cost model + read window) | healthy at 2n |
| seidel-2d | in-place stencil | wavefront grain collapsed to whole-row strips ([1,9600]/4) | keep both owner dims (tiled wavefront); narrow per-diagonal halo | SDE (buildWavefrontOwnerStoragePlan) | improves constant only |
| volume-integral | FEM contraction | compute SU step 36000 (worker-count) coarser than budget 17455 → fail-closed | Tiling refine pipeline tile to budgetBlockShape | SDE (Tiling) | **fail-closed at HEAD** |
| stress | FEM owner-strip RO-halo | init vs compute writer owner-order disagree → 18 verifier errors; halo dissolved | one writer-consistent owner grid + widen read window by ±halo | SDE (LayoutAssignment/RankExpandMu + RaiseWindow) | **fail-closed (18)** |
| velocity | FEM owner-strip RO-halo | window `valid==block`, neighbor reads div/rem; R2 fail | widen read window by `physicalHaloShape` | SDE (RaiseToMuAccessWindow) | **no 2n binary** |
| vel4sg-base | FEM owner-strip RO-halo | 78 whole-grid windows, no halo; 9 verifier errors | emit halo-bearing read window for cross_dim_stencil_3d | SDE (RaiseToMuAccessWindow) | **fail-closed (9)** |

(jacobi2d, convolution-2d/3d, layernorm, activations verify-agents dropped on
socket errors; their unit reads landed and are folded into the families below.)

## Kernel families and the canonical optimization per family

1. **Compute-dense matmul** (gemm, 2mm, 3mm): output owner-tiled at `comm=0`, but
   the **contraction operand** frozen `block_contraction muBlockCount=2` on K
   (`LayoutAssignment` computes `budgetBlockShape` only for `block_parallel`,
   `LA.cpp:495-498`). Fix: split the K-home to budget grain; for **chained**
   (2mm/3mm) also allow the redist to **repartition** (the identity self-edge,
   below).
2. **Elementwise / bandwidth** (stream): at the data-parallel optimum; no SDE
   lever. Residual is runtime/codegen.
3. **Reduction** (atax, bicg, batchnorm, pooling, correlation, volume-integral):
   the consumed array is read **coarse** while written fine; plus float
   reductions are never *realized* (only the integer-atomic branch lowers). Fix:
   (a) reconcile reduction read grain to committed write grain; (b) realize a
   distributed/tree float reduction with a cross-owner combine. `correlation` is
   the healthy exemplar (owner-local row reductions → 2.08×).
4. **Stencil / halo** (jacobi-for, poisson-for, jacobi2d-class): halo facts are
   raised but the per-block read window is **never narrowed** to the halo
   neighborhood. Fix: narrow the stencil read window to budget grain + ±halo and
   size the `halo_like` redist fine; narrow the copy→stencil barrier to a halo
   frontier.
5. **In-place stencil** (seidel-2d): genuine anti-diagonal skew authored, but
   collapsed to whole-row strips. Fix: keep both owner dims (tiled wavefront).
   Intrinsic O(diagonal-width) ceiling remains — improves the constant.
6. **FEM owner-strip RO-halo** (volume-integral, stress, velocity, vel4sg): 6-D
   block-native storage that **fails closed** in window-raise from (a) init vs
   compute writer owner-order disagreement and (b) halo dissolved into div/rem.
   Fix (must co-land): commit one writer-consistent owner grid + materialize a
   halo-bearing read window.

## The four concentrated SDE lever sites

Almost every lever lands in one of four places:

1. **LayoutAssignment** — freezes contraction-operand K-grain (matmul family);
   discards owner *order* (FEM family).
2. **Tiling / DistributionPlanning** — `reconcileArrayLayoutWithCommittedPhysicalShape`
   is **writer-role only** (`SdeCommittedFactUtils.h:241-252`), leaving coarse
   reads (reduction + stencil families).
3. **RaiseToMuAccessWindow** — consumes `physicalHaloShape` only as a boolean
   gate (`MuAccessWindow.cpp:256`); read `validExtents = tile` (`:301`) is never
   widened by halo (stencil + FEM families).
4. **RedistributionEdges** — `targetOwnerDims=sourceOwnerDims` /
   `targetBlockShape=sourceBlockShape` **unconditionally**
   (`RedistributionEdges.cpp:502-503`) — every redist is a same-geometry
   self-edge (chained-matmul + cross-owner-reduction 2n correctness).

## Top opportunities (benchmark-ranked)

Ranked by motivating-kernel count and convergence between kernel records and
transform cross-cuts. These **supersede** the static-survey ranking in
[`proposed-passes.md`](./proposed-passes.md) for prioritization (see note there).

- **A — Reader-grain reconciliation** (extend the writer-only reconcile to read
  homes). *6 kernels:* jacobi-for, poisson-for, pooling, batchnorm, atax, bicg.
  The fine grain is already proven legal (`budgetBlockShape` rides beside the
  coarse read). **Highest value.**
- **B — Halo-bearing read access-window** (widen `validExtents` by
  `physicalHaloShape`). *6 kernels:* stress, velocity, vel4sg, jacobi-for,
  poisson-for, seidel-2d. Converts invisible div/rem halo into the structured
  movement fact ARTS needs. (For FEM, gated behind D.)
- **C — Genuine repartitioning redist** (allow `target≠source`). *4 kernels:*
  3mm, 2mm, atax, bicg. The identity self-edge violates the no-metadata-promise
  rule directly. (Cross-owner combine must also be realized at the CU→EDT
  boundary — possibly architecture-scale.)
- **D — One writer-consistent owner grid** for arrays written by both an init and
  a compute SU. *3 kernels:* stress, velocity, vel4sg. The gate currently
  producing 0-byte FEM binaries; prerequisite for B on those three.
- **E — Split `block_contraction` K-grain** in LayoutAssignment. *3 kernels:*
  gemm, 2mm, 3mm. Lower rank: partly redundant with C for chained matmuls, and
  gemm at 1.52× is already the best scaler.

## Dead / inert in production (repair-or-delete, not levers)

- `elementwise-fusion` — fires on **0 of 21** (su_iterate counts byte-identical
  007→008; `elementwise_pipeline` SUs originate upstream in `SuLoopAccessAnalysis`).
- `iteration-space-decomposition` — **0 rewrites** across 167 captured stage
  pairs (confirmed no-op).
- `sde-mu-access-window-sync-opt` — **0 barriers removed**;
  `partitionBarrierPhases` only sees top-level `cu_region` siblings while
  production barriers sit under `su_distribute`.

## The tile-floor reconciliation (important)

`getMinDistributedTileBytes()` defaults to **0** in code (`RuntimeConfig.h:117`,
`SDECostModel.h:133`) — so the tile-floor + hypergraph path is **dormant on the
default/lit build**. But the `arts.cfg` parser accepts
`min_distributed_tile_bytes = N` (`RuntimeConfig.cpp:158`), and the **megalarge
benchmark runs set it to ~4 MiB** — which is precisely what coarsens reads and
caps `poisson-for` at `muBlockCount=4`. Consequence: **activating a nonzero
default floor (the `sde-default-tile-floor` proposal) would coarsen grain further
and fight opportunity A** on the reduction/stencil families, which need *finer*
reads. The benchmark evidence says the high-value work is reader-grain
reconciliation, halo windows, and real repartitioning redist — **not** turning
the floor on. Resolve this tension before implementing any tile-floor change.
