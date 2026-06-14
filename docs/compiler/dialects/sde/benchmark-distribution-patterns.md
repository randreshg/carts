# Benchmark Distribution Patterns & Enhancements

## 1. Introduction

### North star

The single objective across every kernel is to **maximize BLOCK-distributed DBs and minimize coarse / host_whole / single-block DBs**. Coarse is where scaling dies: a coarse DB funnels the whole array to the home node, every remote access pays full communication, and the second node sits idle. A kernel that "passes" 2n while coarse is *correct-but-not-distributed* — numerically fine because all data co-locates on rank 0, but offering no speedup (and at megalarge, anti-scaling or hanging from the funnel tax).

Two corollaries shape every fix below:

- **Granularity-preserving only.** A fix may move a DB from coarse toward block, never the reverse. Existing block/owner-dim/muBlockCount grain is preserved; nothing is coarsened to node count.
- **Distributed reduction is two-level, never a coarse gather.** A reduction over a distributed array must be realized as block/tile **PARTIALS → a small FINE result DB → a combine CU**. A `mode=in` whole-array gather of a distributed array onto one node is the anti-pattern that defines the class-2 failures.

### Distribution vocabulary

**Per-array layout (HPF DISTRIBUTE/ALIGN):** each array gets a BLOCK layout with committed `ownerDims`, `blockShape`, `budgetBlockShape`, `muBlockCount`, and `kind` (`block_parallel` / `block_contraction` / `replicated`). A writer must commit owner-dims that match the readers' committed grain, or ARTS fails closed (refusing coarse realization) — or, worse, silently coarse-falls-back.

**Movement ops (SDE-authored):**

- `su.halo` — radius-r neighbor face exchange between owner blocks (stencils).
- `su.reduce_scatter` — cross-owner reduction: block partials summed into an owner-indexed fine result.
- `su.all_to_all` — repartition / transpose / read-side broadcast when a consumer's owner/contraction layout differs from the producer's write layout.

**The S17–S21 levers:**

| Lever | Name | What it does |
| --- | --- | --- |
| **S17** | reader/writer-grain reconcile | Commit the writer's owner-dims/grain to match the readers' already-committed block layout (or project an over-ranked grouping fact onto the storage owner rank). The single most-shared fix. |
| **S18** | halo read window | Raise the stencil ±r neighbor reach as a committed access window so `SdeRedistribute` authors `sde.su_halo` and ARTS does a thin face exchange instead of a whole-array copy. |
| **S19** | FEM one-owner-grid | Force all arrays in an element-parallel kernel onto one element-owned grid so outputs co-locate with the inputs they read. |
| **S20** | target≠source repartition | Insert a real `su.all_to_all`/repartition when a consumer reads a producer's array under a different owner/contraction layout. |
| **S21** | K-grain split | Author a reduction as per-block/per-tile partials → fine result DB → combine (cross-owner reduce_scatter, or two-level checksum). |

---

## 2. Summary Table

| Kernel | Pattern class | 2n status | Dominant lever |
| --- | --- | --- | --- |
| stream | Elementwise map + full-reduction checksum | FAIL (sde-to-arts) | S17 + S21 |
| jacobi-for | Stencil/halo (5-pt, out-of-place) | PASS, coarse (0.34×) | S18 (+S17) |
| poisson-for | Stencil/halo (5-pt + rhs init) | PASS, coarse | S18 + S17 (+S20 for f) |
| activations | Elementwise maps + softmax + checksums | FAIL (heap-capture) | S17 + S21 |
| batchnorm | Per-channel full-reduction (nested) | FAIL (sde-to-arts) | S21 + S17 |
| layernorm | Row-wise elementwise + per-row local reduce | PASS, coarse | S17 |
| pooling | Owner-local reduction-to-output | FAIL (memory-unit-realization) | S17 |
| 2mm | Matmul / chained K-reduction | FAIL (codir-to-arts) | S17 (+S21, S20) |
| 3mm | Matmul / three chained GEMMs | FAIL (sde-to-arts) | S17 + S20 + S21 |
| atax | Matmul / dual GEMV (cross-owner reduce) | FAIL (boundary) | S17 + S21 |
| bicg | Matmul / dual mat-vec | FAIL (boundary) | S17 |
| convolution-2d | Stencil/halo (9-pt) | PASS, coarse | S17 + S18 |
| convolution-3d | Stencil/halo (15-tap 3D) | PASS, coarse (~1.12× noise) | S18 + S17 |
| correlation | Mixed: row-local reduce + all-pairs K-reduce | FAIL (owner-rank) | S17 (+S20) |
| gemm | Matmul / K-reduction | FAIL (owner-rank) | S17 (+S20, S21) |
| jacobi2d | Stencil/halo (5-pt ping-pong) | PASS, coarse | S18 + S17 |
| seidel-2d | In-place stencil (Gauss-Seidel wavefront) | FAIL (boundary) | S17 + S18 |
| volume-integral | Element-parallel two-stage contraction | PASS, coarse | S17 + S19 |
| specfem3d/stress | 3D stencil/halo, 6 in-place writers | PASS, coarse | S17 + S18 + S19 |
| specfem3d/velocity | 3D one-sided stencil/halo | PASS, coarse | S18 + S17 |
| sw4lite/vel4sg-base | 3D 7-pt velocity stencil | PASS, coarse (~1.0×) | S17 + S18 |

Status legend: **FAIL** = compile fails closed at 2n; **PASS, coarse** = compiles + Correct=YES but realizes a single coarse DB (not distributed). 1n is 21/21 Correct=YES.

---

## 3. Pattern-Class Strategies

### Class A — Matmul / K-reduction (gemm, 2mm, 3mm, atax, bicg, correlation Step-4)

**Shared distribution shape.** Owner-parallel over the output row dim (`i`); the K-reduction is loop-private (a scalar accumulator → one store per output element), so each node fully reduces its own `k` for its own rows — **no cross-node reduce on the core matmul**. RHS K-operands (`B`/`D`/`p`/`r`) are legitimately `replicated`. The output (`C`/`D`/`G`/`corr`) must be row-block, owner-aligned with the LHS.

**Shared blocker.** The **writer lacks committed owner-dims**, or commits an *over-ranked* `groupBlockCount` (a phantom non-owner `k`/`j` slot, e.g. `[2,1]` / `[2,2]` against a rank-1 `ownerDims=[0]`). At 2n this is refused (`SdeToArtsBoundary*`, "groupBlockCount rank does not match committed owner rank" / "non-coarse DB without committed access windows"), leaving the output coarse.

**The unblocking fix — S17, at the SDE commit site.** Project the writer grouping fact onto the array's storage owner rank (drop the no-op non-owner slot), and author a `write` layout-root + per-block `db_access_window` when none exists. This must happen in `DistributionPlanning` / `LayoutAssignment` (commit-side), not as a downstream boundary repair, per the fail-closed-or-transform standard. **This single S17 reconcile unblocks gemm, 2mm, 3mm, correlation, bicg, and the A-read of atax — six kernels.** It is the highest-leverage fix in the suite.

**Secondary, per kernel.** atax needs **S21** for its *Step-2* cross-owner reduce (`y = Aᵀ·tmp` reduces over the owner dim → block-partial y-vectors → fine y → `su.reduce_scatter`). 3mm needs **S20** for the F intermediate (MM2 write-owner ≠ MM3 contraction-read). correlation Step-4 and gemm/2mm benefit from **S20** column-block of the replicated RHS at high node counts. All benefit from lowering the diagonal checksum to two-level partials (S21) so the verify tail scales.

### Class B — Stencil / halo (jacobi-for, poisson-for, jacobi2d, convolution-2d, convolution-3d, specfem3d/stress, specfem3d/velocity, sw4lite/vel4sg-base)

**Shared distribution shape.** Out-of-place point-stencils (read neighbors, write owner-local), embarrassingly block-parallel within a sweep with a thin radius-r `su.halo` face exchange between owner blocks. Surface-to-volume favorable: O(N²) (3D) / O(N) (2D) halo vs O(N³)/O(N²) compute.

**Shared blockers (two coupled).**
1. The **writer carries no committed owner-dims** (B in conv-2d/3d; vx/vy/vz in specfem velocity & sw4lite; the six stress outputs; seidel's demoted write root) → coarse-fallback of *every* DB in the family to keep grain consistent.
2. **`SdeRankExpandMu` dissolves the ±1 neighborhood into `div/rem` index math** before `classifyPattern` runs, so no halo access window survives and `SdeRedistribute` authors no `su.halo`. Classification must run *before* rank-expand, or recover offsets from the div/rem form.

**The unblocking fix.** **S18 (halo read window)** is primary: raise the ±r reach as a committed window so ARTS authors `su.halo`/`perBlockHaloExchange` (the landed jacobi2d/`chooseCollective halo` path) instead of a whole-array RO copy. **S17** is the prerequisite: commit the writer block layout owner-aligned to the reads so `evaluateDistributedDbEligibility` flips from `StencilReadInternodeUse` to eligible. Additional gates: the single-locality stencil cap (`getStencilWorkerTarget`, DistributionPlanning.cpp:78–84) must be raised to inter-locality with a concrete ARTS halo path; the `RedistributionEdges` collector must recover home-layout from committed physical for read-only-stamped arrays (the dead `homeLayoutFromCommittedPhysical`/`recordHomeLayout` tier-2 pass).

**Leverage.** The S17+S18 stencil fix is shared verbatim across all eight stencils. jacobi2d already proved the ARTS `perBlockHaloExchange` mechanism; the remaining work is wiring it through rank-expand + the writer commit. Multi-array families (specfem stress/velocity, sw4lite) additionally want **S19** (one owner grid across all i/j/k arrays) so output blocks co-locate with input blocks and one halo exchange is shared.

**Scaling note.** 2D stencils (jacobi2d/conv-2d) are comm-bound → expect ~1.0× even when block-distributed; the paper result is *correct + genuinely block-distributed with halo-only traffic*. 3D stencils (conv-3d, specfem, sw4lite) are surface-to-volume favorable → ~1.5–1.9× target.

### Class C — Full / partial reduction over a distributed array (stream checksum, batchnorm, atax Step-2, activations checksums+softmax)

**Shared distribution shape.** A reduction reads a distributed array; the result must be a **block/tile partial → fine result DB → combine**, never a coarse `mode=in` gather. batchnorm and atax-Step2 are *cross-owner* (reduce over the owner dim); stream/activations checksums reduce over the data dim into a scalar.

**Shared blocker.** Enforced at one point — `verifyRawSuAccessCoveredByDep` / `SdeToArtsBoundaryDepAnalysis` ("touches a DB without a committed access-window dependency"). The reduction SU has the layout fact (`array_layout_root read`, `block_parallel`) but **no `arts.db_access_window`**, so ARTS fails closed. batchnorm additionally hits a matcher gap: `ScalarBlockReduction` rejects nested `scf.for`, so the two-level batch×spatial reduction is never split.

**The unblocking fix.** **S21 (K-grain split)** authors per-block partials + `createFinalCombine` into the fine result; **S17** raises the committed read window for the reduced array (and gives downstream normalize/scale phases a fine RO-replicated channel vector instead of a coarse gather). For batchnorm, generalize `matchReductionLoop`/`validateLoopBody` to accept a perfectly-nested reduction where the outer loop is the committed owner block dim. The shared enforcement point means one window-raise + partial-combine mechanism covers stream, batchnorm, atax-Step2, and the activations checksums.

### Class D — Elementwise / NREPS map (layernorm, activations 7 maps, pooling, volume-integral, plus stencil copy phases)

**Shared distribution shape.** Owner-aligned elementwise/owner-local-reduction-to-output over a block dim (batch or element); zero cross-block movement; the ideal scaler.

**Shared blocker.** The **in-place / single-owner writer never commits owner-dims** (layernorm `x`, activations outputs), or the **read-admission gate** rejects an owner-local external read (pooling, activations). layernorm "passes silently coarse" — the same writer-owner-dim gap as gemm, but legal as one DB so it isn't refused. pooling fails closed at `MemoryUnitRealization` because `getUnclassifiedOwnerSliceLayout` demands read==write-array.

**The unblocking fix.** **S17** — author the writer arrayLayout entry when absent (layernorm: `commitWriterPhysicalLayoutViaMuType` / `hasCommittedCuMuPartitionFacts` short-circuit), and relax the unclassified-read gate to admit an external read whose owner dims match the writer's committed owner-local block (pooling/activations). No movement, no halo, no K-split. volume-integral additionally wants **S19** to put `dofs`/`fluxOut`/compute on one elem-owned grid.

### Class E — FEM element-parallel contraction (volume-integral; FEM aspects of specfem/sw4lite)

Element-parallel, two reductions both element-local (private `alloca`), no cross-owner reduce. Pure owner-compute block over `elem`: block `dofs`+`fluxOut` on dim 0, matrices replicated. Blocker = the **write array absent from `arrayLayout`**; fix = **S17** (project the proven `elem` owner map onto the write target) + **S19** (one owner grid). Cleanest scaler — ~1.8–2.0× once the writer layout lands.

### Class F — Transpose / repartition (3mm F-intermediate, correlation data replication, gemm/2mm column-block B)

No kernel is *defined* by transpose, but several need **S20** as a secondary lever: a real producer-side `su.all_to_all`/repartition when a consumer reads a producer's array under a different owner/contraction layout (3mm MM2→MM3 F gather; correlation all-pairs `data[j]` read; column-block of replicated RHS at high node count). The primitive needs no new ARTS mechanism (producer-side gather/broadcast).

### Class G — In-place serial-carry (seidel-2d)

The only genuinely serially-constrained kernel: loop-carried Gauss-Seidel dependence, parallel only via diagonal wavefront skew. Blocker: `realizeWavefrontSkew` demotes the output `array_layout_root` to `read`, so `commitWriterPhysicalLayoutFacts` commits nothing → coarse → fail-closed at the boundary. Fix = **S17** (stop demoting; author the write fact on the wavefront owner dim so the already-computed `buildWavefrontOwnerStoragePlan` actually commits) + **S18** (the ±1 RO halo window). Win is *correctness + block distribution* (exits the fail bucket), not strong scaling — ~1.0–1.3× bounded by wave-to-wave fronts.

---

## 4. Per-Kernel Entries

### stream

- **Pattern class**: Elementwise/streaming map (copy/scale/add/triad over one flat dim, x NTIMES) + a trailing strided full-reduction (checksum). No matmul, halo, scatter-add, transpose, or in-place dependency.
- **Distributed arrays (owner-dims, block)**: a/b/c all BLOCK on the kernel side — `blockShape=[500000]`, `budgetBlockShape=[250000]`, `kind=block_parallel`, `muBlockCount=2`, `ownerDims=[0]`. Compute side is fully block-native; `a` (arrayId 0) stays `<block>` end-to-end.
- **Coarse spots**: `b` (arrayId 1) and `c` (arrayId 2) lower to `arts.db_alloc[...<coarse>]` because they are consumed only by the serial checksum; init/timing scaffolding residual CUs are `<single>`/`<coarse>` (benign).
- **Movement**: None authored — and that is the bug. Kernels are owner-aligned elementwise (correctly need none); the checksum needs a `su.reduce_scatter`-style distributed reduction that SDE never emits.
- **Reduction**: `compute_checksum` (stream.c:51-56), strided `for j step 128` summing a/b/c → 3 scalars → 1 f64. SDE rank-expands the indices to the `[2,500000]` layout but leaves the loop in a `sde.cu_region <single>` `serialReason<residual_source>` with no `array_layout_root read` / committed access window — a coarse serial gather, not block partials.
- **2n status**: FAILS at `sde-to-arts` (`SdeAccessesToArtsDeps`). Errors at `stream.c:53` (`asum += a[j]`, arrayId 0) and `:54` (`bsum += b[j]`, arrayId 1): "uses a DB-backed external memref that was not remapped to an EDT dependency; SDE must provide a committed access window for this standalone CU access." `c` at `:55` does not error (left remappable by the trailing kernel). SDE-planning itself succeeds (EXIT=0).
- **Enhancement (granularity-preserving fix + S17-S21 lever)**: Class is block-partials → fine result-DB reduction (writer owner-dims already committed — not a writer-grain gap). (a) Emit a committed read access window + `db_acquire(<in>, partitioning <block>)` for b/c exactly as already done for a, unblocking compile; (b) lower `compute_checksum` as per-block partial sums over the existing block grain → small fixed-size result DB (O(num_blocks) f64 triples) → a combine CU. Keep b/c allocated `<block>`, never `<coarse>`. **S17** (reader/writer-grain reconcile) is primary — reconcile the coarse reduction reader to the committed block layout; **S21** (K-grain split) authors the reduction as per-block partials + combine. S18/S19/S20 do not apply (no halo/grid/repartition). Same single SDE/ARTS mechanism fixes the shared "coarse reduction reads a distributed array" class: bicg (byte-identical error), atax, batchnorm.
- **Scaling limit**: None fundamental — pure streaming, no carried cross-block dependency; the only serial point is the O(num_blocks) checksum combine. 1n→2n should give a real but bandwidth-bound (sub-linear) speedup once b/c are block and the checksum is two-level. Residual single-node ~3x gap is runtime/codegen (AVX2/NUMA), orthogonal to the 2n compile+scale fix.

### kastors-jacobi/jacobi-for

- **Pattern class:** Point-stencil with halo (5-point 2D Jacobi) in a 10-sweep time loop; each sweep is elementwise copy `u = unew` then stencil `unew = stencil(u, f)`. Out-of-place (separate `u`/`unew`), embarrassingly parallel within a timestep — NOT in-place Gauss-Seidel.
- **Distributed arrays (owner-dims, block):** SDE commits real per-array block plan — `u` 2-D grid `ownerDims=[0,1]`, blockShape `[128,128]`, muBlockCount 4; `unew` 1-D rows `ownerDims=[0]`, `[128,256]`, muBlockCount 2; `f` 1-D rows `ownerDims=[0]`, `[128,256]`, muBlockCount 2. None replicated at SDE level.
- **Coarse spots:** ARTS discards the block plan — all 3 DBs `db_alloc[...,<coarse>]`, all 8 `db_acquire` whole-array `partitioning(<coarse>)`, all EDTs `<intranode> route(-1)`. 11/11 db refs coarse; committed muBlockCount 2/4 dropped, no owner map, no per-block EDT.
- **Movement:** Needed `su.halo` (radius-1 boundary face of `u` between owner blocks; `f` none, `u=unew` copy is owner-local at unified grain). Emitted: NONE — no `sde.halo`/`reduce_scatter`/`all_to_all`/`gather` raised.
- **Reduction:** None in compute kernel (pure stencil). Only a trailing serial diagonal-checksum in a `<single>` residual region (off the kernel timer) — not the atax/bicg/stream reduction class.
- **2n status:** Compile PASS, Correct=YES (megalarge `7.1486e-01` ≈ ref). Anti-scales ~3x: kernel 1n 46.1s → 2n 137.2s (0.34x). Coarse collapse means both nodes funnel whole arrays to the home node each sweep = pure comm tax.
- **Enhancement:** SDE facts are correct; gap is downstream. Lift the single-locality stencil cap (`DistributionPlanning.cpp:82-84`) so the committer targets cluster capacity and raises an `sde.su.halo` window on `u`; then `queryAccessWindows` is non-empty and the boundary takes `createBlockDbBackedMemref` over committed muBlocks instead of coarse-fallback (`SdeToArtsBoundaryStorage.cpp:326-366`). **S18 (halo read window)** is primary — flips the boundary off coarse; **S17 (reader/writer-grain reconcile)** secondary for the `u=unew` copy (1-D reader vs 2-D writer grain). Best win: unify all three arrays to 1-D row-blocks `ownerDims=[0]` so the halo is a single-dim row exchange and the copy is local memcpy, avoiding **S20** repartition entirely. Granularity-preserving (keeps 128×128/128×256 block grain, no coarsening). Shares the exact fix with jacobi2d and poisson-for; do NOT apply to seidel-2d (in-place, correctly fail-closed).
- **Scaling limit:** Surface-to-volume bound — per-timestep radius-1 row exchange is O(N) vs O(N²/2) compute per node; expect ~1.6–1.9x at 2n (10 per-timestep halo barriers + double-buffer copy prevent ideal 2.0x), ~5x improvement over current 0.34x. Good scaling above 2 nodes for a 1-D decomposition until halo/compute ratio rises. Only ceiling is comm + the mandatory per-timestep stencil/copy sync; no correctness or in-place barrier.

### kastors-jacobi/jacobi-for + poisson-for

- **Pattern class**: Point-stencil with halo (5-point Jacobi), double-buffered (out-of-place: read `u`, write `unew`, swap via copy). NREPS=1, itnew=10. Both kernels share an identical `sweep()`; poisson-for adds an `rhs()` elementwise init. Not in-place (unlike seidel-2d), so genuinely parallelizable.
- **Distributed arrays (owner-dims, block)**: SDE plans all arrays `block_parallel` at SIZE=256. jacobi: `u` rank-expanded `2x2x128x128` owner-dims `[0,1]` muBlockCount 4 (stencil source), `unew` `2x128x256` owner-dim `[0]` count 2, `f` `2x128x256` owner-dim `[0]` count 2. poisson: `f` `2x256x128` owner-dim `[1]` (rhs j-outer), `u` owner-dims `[0,1]`, `unew` owner-dim `[0]` — init-vs-compute owner-dim disagreement on `f`.
- **Coarse spots**: ALL of them. Despite block_parallel SDE facts, ARTS create-dbs coarse-falls-back every array to a single monolithic `<coarse>` `route(-1)` DB (3 per kernel, 0 block DBs); all EDTs `<intranode>`. The muBlockCount 2/4 parallel grain is dropped at the sde-to-arts boundary.
- **Movement**: Needed = `su.halo` for the i±1/j±1 stencil reach (unew→u copy is owner-local, no movement). Present = NONE. `SdeRankExpandMu` dissolves the k±1 halo into `divui`/`remui` block-localization index math with zero access-windows raised, so `SdeRedistribute` finds no halo edge and emits no `sde.su_halo`.
- **Reduction**: None in the timed kernel. The only sum is the host-side diagonal checksum (`sum unew[i][i]`) in a serial `residual_source` region after the timer — at megalarge this forces a coarse gather of the distributed array to rank 0 (shares the atax/bicg/stream coarse-reduction anti-pattern).
- **2n status**: COMPILE+PASS (both); not in the failing-11 set. But "passes" because the coarse collapse makes DBs monolithic/local-to-rank-0, so the halo is satisfied on the home node and node 1 is idle — correct-but-NOT-distributed. This is the funnel where megalarge 2n hangs/anti-scales.
- **Enhancement**: S18 (halo read window) — raise the i±1/j±1 stencil reach as a committed access window so `SdeRedistribute` authors `sde.su_halo`, carrying `stencilMin/MaxOffsets` → `stencilSupportedBlockHalo` → `setPerBlockSingleWriterStencilAttr`, flipping ARTS `evaluateDistributedDbEligibility` from `StencilReadInternodeUse` to eligible (per-block compute_block DBs + perBlockHaloExchange, halo faces not full-buffer copy). Prerequisite S17 (reader/writer-grain reconcile): pin `u`/`unew` to one consistent owner-dim plan so the stencil SU is `su_distribute`-wrapped and the window is detectable. poisson-for additionally needs S20 (target≠source repartition) for `f`'s orthogonal `[1]`-init vs `[0]`-stencil owner dims, or pin `rhs()` to write `f` row-major. Shared fix with `polybench/jacobi2d` (identical coarse state on this HEAD); the e13bf3001/f4571a71 per-block-halo path is not active here.
- **Scaling limit**: None fundamental in the stencil — out-of-place double-buffered 5-point Jacobi is the canonical embarrassingly-block-parallel-with-nearest-neighbor-halo kernel; with S17+S18 it should scale ~linearly 1n→2n at megalarge (halo = one ghost row per face, O(N)/timestep vs O(N²) coarse copy). The only mandatory serialization is the cheap per-sweep copy/stencil barrier. Residual ceiling = the host checksum gather (needs two-level block-partials→fine-result→combine reduction) and ARTS frontier concurrency at high block counts — neither a compiler-shape defect.

### ml-kernels/activations

- **Pattern class:** Mixed — 7 independent elementwise maps (relu/leaky/relu6/gelu/gelu_fast/sigmoid/tanh) over `input[262144]` → 7 distinct outputs; 1 sequential softmax full-reduction (max-scan, exp+sum, normalize) over a size-100 array; 8 full-reduction `fabs`-sum checksums.
- **Distributed arrays (owner-dims, block):** `input` and all 7 outputs are BLOCK-distributed, owner-dim 0, `muBlockCount=2`, blockShape `[131072]` (stride SIZE/2); EDTs are `concurrency=internode` with `ownerLocalWriterSplit`, owner map `blockIdx % total_nodes`. This is the north-star shape.
- **Coarse spots:** `softmax_input`/`softmax_output` (size 100) never DB-promoted — stay raw `memref.alloc<100xf32>` (`%140`); the 8 checksums fold into one coarse single SYNC EDT that `db_acquire mode=in`s all 7 distributed outputs and reduces sequentially on one node (coarse gather of distributed arrays).
- **Movement:** None for the 7 elementwise maps (owner-aligned, no cross-block reads). Checksums and softmax want `su.reduce_scatter`-style block partials → small fine result DB, not a coarse `mode=in` gather.
- **Reduction:** Each checksum `sum |out_k[i]|` over 262144 → scalar (×8, then summed). softmax internal: max-reduce → sum-reduce → divide over 100 elements (sequential dependency chain).
- **2n status:** FAIL at SDE→ARTS (`lib/carts/dialect/arts/IR/Dialect.cpp:321`): `'memref.store' op EDT region captures pointer-bearing value '%140 = memref.alloc() : memref<100xf32>'`. softmax_output never raised to a DB, so the fused internode SYNC EDT illegally captures the raw heap memref. (1n compiles; rule only bites the 2n distributed path.) The 7 big arrays distribute cleanly.
- **Enhancement (granularity-preserving + lever):** Promote `softmax_input`/`softmax_output` to a (small block / single-block) DB and pass it as an EDT dependency instead of capturing the raw alloc — DB-promotion of the reduction target, not a coarse fallback. Realize softmax as block-local max/sum partials → tiny 2-scalar fine result DB → broadcast → block `divf`. **S17** (reader/writer-grain reconcile + author writer owner-dim, same family that split the 7 elementwise writers) + **S21** (K-grain split of max/sum into block partials → fine result). Apply the same block-partial → fine-result reduction to the 8 checksums to remove the serial gather. S18/S19/S20 N/A. Shared class with atax/bicg/stream/batchnorm ("coarse reduction reads a distributed array") and the `Dialect.cpp:332` heap-capture tripwire.
- **Scaling limit:** The 7 elementwise maps are embarrassingly parallel but bandwidth-bound and gated by per-rep RDMA cost of the shared `input`; expect modest positive 1n→2n at megalarge, not compute-bound linear. softmax has a fundamental serial limit (sequential max/sum over 100 elems) — correctness gate only, do not expect scaling. E2E ceiling is set by the serial checksum gather unless it too is block-reduced. Measure real scaling at `--size megalarge`.

### ml-kernels/batchnorm

- **Pattern class:** Per-channel full-reduction (mean/variance over batch+spatial) + elementwise broadcast in NCHW; reduction axis (batch) is the same axis SDE chose as owner dim. Reduction-reads-distributed-array class (with atax/bicg/stream), not the gemm writer-owner-dim class.
- **Distributed arrays (owner-dims, block):** arrayId 0 `output`/`x` activations `memref<2x2x64x1024xf32>` (batch rank-expanded 4→[2,2]), `kind=block_parallel`, `ownerDims=[0]` (batch), `blockShape=[2,64,1024]`, `budgetBlockShape=[4,64,1024]`, `muBlockCount=2`; arrayId 2 `mean[64]`→`[2,32]` `ownerDims=[0]` (channels) DB `<block>`; arrayId 3 `variance[64]`→`[2,32]` `ownerDims=[0]` (channels). Writer owner-dims ARE committed.
- **Coarse spots:** Despite committed `block_parallel` layout, activation DBs `%guid`/`%guid_18` allocated `<coarse>` (2x2x64x1024) and acquired `partitioning(<coarse>)`; variance DB `%guid_35` `<coarse>`; copy phase runs as one coarse EDT over the whole tensor. Coarseness is forced by the un-authored reduction triggering a whole-array gather.
- **Movement:** reduce_scatter for mean/variance (block-local batch partials → per-channel result) — currently none authored; RO broadcast/replicate of the tiny `[64]` channel vectors to every batch block for normalize/scale/add. No halo, no all_to_all.
- **Reduction:** `mean[c]=scale·Σ_{b,k} output[b,c,k]`, `variance[c]=scale·Σ_{b,k}(output[b,c,k]−mean[c])²`. Result iterates channels (fine `[64]`/`[2,32]`); reduces over batch (owner dim 0) + spatial — a cross-owner two-level reduction, must be block partials → fine combine, never coarse gather.
- **2n status:** FAIL at SDE→ARTS conversion. `build.log:14-23`: `batchnorm.c:70:20` (`mean[i]+=x[j][i][k]`) "touches a DB without a committed SDE access-window dependency". arrayId 0 read in mean/variance SUs has `array_layout_root read` + committed `block_parallel`/ownerDims=[0] but no `arts.db_access_window`; ARTS fails closed (`SdeToArtsBoundaryDepAnalysis.cpp:1156-1158`, `verifyRawSuAccessCoveredByDep`). `batchnorm-arts.ll` is 0 bytes.
- **Enhancement:** Root cause is `ScalarBlockReduction.cpp` matcher gap — `matchReductionLoop` (`:304-352`) requires a single IV and `validateLoopBody` (`:213-256`) rejects any nested `scf.for` (`:218-225`), so the two-level batch×spatial reduction (`batchnorm.mlir:962-977` mean, `:1086-1116` variance) is dropped and no partials/combine/access-window authored. Fix: accept a perfectly-nested reduction where the outer loop is the committed owner block dim and inner loop(s) are non-owner (spatial), collapse inner extent into per-owner-block partial slots, then the existing `createFinalCombine` (`:684`) yields the fine `[64]` result. Lever: **S21** (K-grain/reduction split, generalized from flat K to nested owner×spatial) primary; **S17** (reader/writer-grain reconcile) secondary, to give normalize/scale/add a fine RO-replicated channel vector instead of a coarse gather. Granularity-preserving: partials per-batch-block, combine result fine, tensor stays `block_parallel` across both nodes for all five phases. Shared SDE lever with atax/bicg/stream (common enforcement point `verifyRawSuAccessCoveredByDep`).
- **Scaling limit:** No fundamental limit — no in-place serial dependency; all five phases embarrassingly parallel across batch. Only cross-node traffic is the `[64]`-element channel-vector reduce + RO rebroadcast (kilobytes). Expected near-linear for the FLOP-dominant elementwise phases (~1.7–1.9× at megalarge once RDMA amortizes), bounded only by the two small channel-vector exchanges per pass and fixed NREPS=1 startup/teardown tax.

### ml-kernels/layernorm

- **Pattern class:** Row-wise (per-batch) normalization — ELEMENTWISE / embarrassingly-parallel over BATCH, with a per-row LOCAL full-reduction over HIDDEN. Not a stencil, matmul, or cross-block reduction. Outer `b` loop is `#pragma omp parallel for`; each row independently computes mean, var (scalars private to row `b`), then rewrites `x[b][h] = norm*gamma[h]+beta[h]` in place. Zero inter-row / inter-node data dependence in the compute.
- **Distributed arrays (owner-dims, block):** Target shape — `x` (array_id 0, `memref<64x2048xf32>`) BLOCK over dim 0 (BATCH), ownerDims=[0], blockShape=[32,2048], muBlockCount=2; `gamma`/`beta` (`memref<2048xf32>`) replicated (correct today). Actual at 2n — `x` has **no committed owner-dims / no arrayLayout entry**, so it falls to a single coarse DB.
- **Coarse spots:** `x` realized as one monolithic `<coarse>` `local_only` DB (`elementSizes[64,2048]`, all 5 acquires coarse) on one node; compute is a single `arts.edt <sync> <intranode>` looping the full `0..64` BATCH range. `groupBlockCount=[2]` on the CU never reaches DB grain (no storage owner-dims to block on). Verification checksum reads `x[i][i]` in a serial `<single>` cu_region — latent coarse-read-of-distributed-array once `x` is split.
- **Movement:** NONE required. With BATCH block-distributed each node owns 32 complete rows (full HIDDEN per row); gamma/beta are replicated reads. Communication-free at 2n — the ideal scaler. No halo / reduce_scatter / all_to_all / all_gather anywhere.
- **Reduction:** Two reductions (mean, var), both per-row and fully owner-local — each produces a scalar private to row `b` (`memref.alloca`, EDT-private scratch, not a DB), consumed in the same row's rewrite. No partial→combine, no reduce_scatter, no cross-owner reduction. NOT the atax/bicg/stream coarse-gather class.
- **2n status:** COMPILES and PASSES (Correct=YES; medium 221.61, large 7094.59 vs OMP match) — but **correct-but-NOT-distributed**. Passes only because coarse-fallback for the write-root is *accepted* here (`x` is a dense 2-D array, legal as one DB), unlike gemm/2mm/3mm/correlation where the same writer-owner-dims gap is *refused* → compile fail. Layernorm is the "passes silently coarse" tail of that defect. No megalarge-2n leg collected.
- **Enhancement (granularity-preserving fix + lever):** **S17 (reader/writer-grain reconcile)** is the primary lever — force `x`'s MU/DB storage grain to match the committed CU `groupBlockCount=[2]`. Root cause: `DistributionPlanning.cpp:2037-2040` — `hasCommittedCuMuPartitionFacts` returns true on the bare CU `groupBlockCount`, so dispatch `continue`s and skips `commitPhysicalLayoutFromAssignedLayout` (:2054), the path that would commit storage owner-dims for a single-owner in-place writer. Upstream, `LayoutAssignment.cpp:656-667` routes the writer into `writerCommits`, and `commitWriterPhysicalLayoutViaMuType` only rewrites an *existing* arrayLayout entry — none was authored for `x`. Fix: either have `commitWriterPhysicalLayoutViaMuType` author the arrayLayout write entry when absent, or tighten `hasCommittedCuMuPartitionFacts` so a bare `groupBlockCount` without a storage fact does not short-circuit, letting the single-owner committer run. This authors ownerDims=[0], blockShape=[32,2048], muBlockCount=2 → per-batch-block single-writer DBs via RankExpandMu/CreateDbs. Pure writer owner-dim commit — no halo, no repartition, no K-split. Shared fix with **activations** (identical in-place elementwise writer, same `groupBlockCount=[2]` + full `mu_alloc`, no ownerDims). Distinct from batchnorm/pooling, which already commit ownerDims=[0]+muBlockCount=2 via the non-in-place separate-output path — do not regress those. The latent checksum fix (block-local partial → small fine result DB → combine) is shared with the atax/bicg/stream/batchnorm class-2 group.
- **Scaling limit:** Near-linear on the batch dim up to `min(BATCH_blocks, nodes)` once `x` is split — no cross-row dependence, no body communication. Bounded by **batch-block count**, which is byte-capped by `kTargetBlockBytes=2 MiB` (`LayoutAssignment.cpp:143`): medium `x`=4 MiB → only 2 blocks (exactly 2n, zero headroom beyond 2 nodes). A tighter/locality-aware budget would expose finer batch grain. The in-place write is not a real serialization (it stays within an owner's row) — it only trips the committer exclusion.

### ml-kernels/pooling

- **Pattern class** | Owner-local reduction-to-output: three independent downsampling SUs (maxpool, avgpool, global_avgpool) over NCHW `[batch, channels, spatial]`; each output cell's reduction is entirely local to one batch row — no cross-owner reduction, no halo, no transpose. Distinct in/out arrays (no in-place, no carried recurrence) — embarrassingly parallel on batch.
- **Distributed arrays (owner-dims, block)** | input (id 1) `4x64x4096`, maxpool_out (id 0) `4x64x1024`, avgpool_out (id 2) `4x64x1024` all `block_parallel`, `ownerDims=[0]` (batch), `muBlockCount=2`, blockShape `[2,64,…]`. Owner dim = batch at every size (small batch=4, megalarge=3840); batch/2 step at 2n.
- **Coarse spots** | global_output (id 3) `4x64` — no committed writer arrayLayout → coarse/replicated. Final diagonal-only checksum `cu_region <single> {serialReason=residual_source}` reads outputs diagonally (RO-serve, tiny diag≤4, off-timer — not a limiter).
- **Movement** | None. Each output cell reads only its own batch block (owner-local); no `su.halo` (2x2 windows non-overlapping, within-block), no `su.reduce_scatter` (reduction target owner-indexed), no `su.all_to_all`, no inter-SU gather. No movement op emitted (compile aborts first).
- **Reduction** | Owner-local, no SU-level accumulator: max/avg reduce a 2x2 window into a local scalar alloca → owner-indexed store (`su_iterate` is `nowait`, empty `reduces(...)`/`getReductionAccumulators()`); global_avgpool reduces full spatial within each owned (b,c) → `[4x64]`. All reduce non-owner inner axes to an owner-indexed result → no cross-node combine.
- **2n status** | FAIL — compile aborts at `sde-memory-unit-realization` (exit 1, empty `pooling-arts.ll`). maxpool (`pooling.c:93`) + avgpool (`:158`) hit *"committed physical storage layout facts that this pass cannot realize."* SUs carry no `structuredClassification` (windowed loop has `scf.if` bounds guard + integer-computed indices → `classifyPattern` bails), so `canRealizeCommittedOwnerSlices` routes to `getUnclassifiedOwnerSliceLayout`, which rejects because the RO `input` read is not also written in the SU (`MemoryUnitRealization.cpp:127-131`). global_avgpool does NOT error (write target id 3 has no committed block layout → passes trivially). Owner-dep store check itself passes; only the read==write precondition blocks it.
- **Enhancement (granularity-preserving fix + S17-S21 lever)** | **S17 (reader/writer-grain reconcile)** — primary. Relax `getUnclassifiedOwnerSliceLayout` (`MemoryUnitRealization.cpp:127-132`) to admit an external read whose owner dims are committed identically to the writer (`ownerDims=[0]`, owner-local block) instead of requiring read==write-array. Layout is already fully block (owner=batch); only the admission gate blocks it — preserves `blockShape=[2,64,1024]` block DBs, no coarse fallback, no movement/combine added. **S18 does NOT apply** and is the discriminator: the 2x2 window is owner-local over spatial (batch is owner), so there is no cross-block halo — a halo-style rejection would be wrong. Same fix unblocks sibling `activations` (same unclassified read-admission gate). Alternative route (b): teach `classifyPattern` to recognize the bounds-guarded windowed reduction — more invasive (affine raising through `scf.if`).
- **Scaling limit** | None fundamental — embarrassingly parallel on batch, no in-place/recurrence (unlike seidel-2d), already block-native (no coarse bridge/gather to de-coarsen). Megalarge batch=3840 → 1920 owned rows/node, RO owner-sliced input (no remote input traffic). Expect near-linear ~1.6–1.9x at 2n once the gate clears (owner-aligned compute-dense class, cf. gemm 1.52x / correlation 2.43x); ceiling is memory-bandwidth/transport of the owner-local input block, not algorithmic.

### polybench/2mm
- **Pattern class**: Chained matmul / two-stage K-reduction (`D := alpha*A*B*C + beta*D`); two embarrassingly-parallel matmuls in a clean producer→consumer chain over a shared owner-dim, no loop-carried cross-owner dependence. Verification reads `diag(D)`.
- **Distributed arrays (owner-dims, block)**: `tmp` (id 0) and `A` (id 1) are `block_parallel`, `ownerDims=[0]`, `blockShape=[64,128]`, rank-expanded `2x64x128`, muBlockCount 2.
- **Coarse spots**: `B` (id 2), `C` (id 4) legitimately `replicated` (k-shared RHS); **`D` (id 3) illegitimately coarse** — it is owner-i, structurally identical to `tmp`, but stamped `ownerDims=[]`. The B/C/D init `su_iterate` carry no `db_access_window`; the residual `diag(D)` checksum is a `<single>` coarse SU touching block `tmp`.
- **Movement**: `none` — both stages are owner-local (owner-i LHS, replicated RHS); `tmp` produced and consumed on the same owner-dim 0 so source==target. No halo/all_to_all needed for 2n correctness.
- **Reduction**: Two owner-local K-reductions (sum over k into the same owner block's output element); no cross-owner partials, so **no** `reduce_scatter` and no fine result DB. The "easy" reduction class.
- **2n status**: **FAIL** at SDE→ARTS (`codir-to-arts`, empty `2mm-arts.ll`). `2mm.c:44:15: error: touches a non-coarse DB without committed SDE access windows; refusing coarse SU realization` (`SdeToArtsBoundaryDepAnalysis.cpp:1104-1113`). Root: SU collapses to 1-D so `hasCanonicalMatmulAccessShape` fails → no `<matmul>` classification → matmul-output writer-window path never fires; module has 0 `role="write"` facts, so `tmp`/`D` get no writer owner-dims and `D` falls to coarse.
- **Enhancement**: Commit writer owner-dims + access windows on the matmul output SUs (classify SU1/SU2 as `<matmul>` before the 1-D collapse, or accept the post-distribution shape in `classifyPattern`, feeding the already-tested output-window path `sde_matmul_output_access_windows.mlir`) so `tmp` and `D` get `ownerDims=[0]`, `blockShape=[64,128]` matching the read grain; B/C stay replicated. **Lever: S17** (reader/writer-grain reconcile — propagate consumer block grain to producer write fact) primary; **S21** (K-grain split) secondary for k-parallelism. Keep `tmp` block-resident across both stages (no coarse bridge) and make the checksum a per-node partial `diag(D)` → scalar combine, not a coarse gather. Shared fix with gemm/3mm/correlation.
- **Scaling limit**: No fundamental serial limit; expect the **gemm band (~1.4-1.5x at 2n)**, bounded below 2x by replicated B/C broadcast and the single-scalar checksum combine. At megalarge the replicated RHS becomes the comm/memory bottleneck → needs S20 column-block (`all_to_all`) repartition of B/C to approach linear.

### polybench/3mm
- **Pattern class**: Chained matmul / K-reduction — three dependent GEMMs (E=A·B, F=C·D, G=E·F) where E and F are compiler-produced intermediates both consumed by the third; per-(i,j) privatized accumulator; diagonal-sum checksum over G.
- **Distributed arrays (owner-dims, block)**: A, C, E — `block_parallel`, ownerDims `[0]`, blockShape `[64,128]`, muBlockCount 2 (row-block over dim 0); F — `block_contraction`, ownerDims `[0]`, budgetBlockShape `[64,128]` (block, but read in MM3 as the K/contraction operand). 5 of 7 arrays carry real owner dims.
- **Coarse spots**: B, D — `replicated` 128×128 K-operands (legitimate, cheap, leave alone); F — lowers to coarse `2x64x128` whole-array intermediate gather despite a block write root; G — `memref<128x128>` final output with no committed writer owner-dim → coarse single DB.
- **Movement**: MM1/MM2 owner-aligned over rows with replicated B/D → no cross-block movement. MM3 needs F brought together along its owner/K dim → an `su.all_to_all`/repartition of F (today realized as the coarse whole-array copy). No halo (not a stencil).
- **Reduction**: three K-reductions accumulating `L[i][k]*R[k][j]` into a privatized per-element scalar; no partial/combine split authored. Cross-node issue is a contraction-operand gather (F along K) plus a coarse output, not a reduce-scatter. Diagonal checksum is a separate coarse serial reduction over G.
- **2n status**: FAIL. SDE planning PASSES (clean layouts); aborts at `sde-to-arts` with `'sde.su_iterate' op commits groupBlockCount whose rank does not match the committed owner rank` (`3mm.c:28` init writer for A/C: rank-2 `groupBlockCount=[2,1]` vs rank-1 committed `ownerDims=[0]`). Same init/elementwise-writer coarse-fallback class as gemm/2mm/correlation.
- **Enhancement**: (1) Fix the single-owner data-parallel writer committer (`DistributionPlanning.cpp:1331`/`:1659`) to accept rank-expanded single-owner init writers and reconcile `groupBlockCount` rank to the rank-1 owner — **S17 (writer/owner-grain reconcile)**, the primary granularity-preserving fix, shared with gemm/2mm/correlation. (2) Keep F block and insert a real producer-side `su.all_to_all`/repartition between MM2's write-owner and MM3's contraction-read layout instead of the coarse copy — **S20 (target≠source repartition)**. (3) Realize G=E·F as two-level block/tile partials → fine result DB → combine so the output and checksum stay owner-local — **S21 (K-grain split)**.
- **Scaling limit**: Compute-dense O(n³), no in-place/wavefront serial carry → structurally a good 2n scaler; expected ~1.4–1.7× once F/G stop coarsening (cf. gemm 1.52×, correlation 2.43×). Ceiling set by one repartition/communication step per chained matmul (MM2→MM3 dependency forces F gather) plus 2-block load balance (even 64+64), both negligible against O(n³) compute and shrinking with n — no serial bottleneck.

### polybench/atax

- **Pattern class**: Matmul / K-reduction; two coupled GEMV passes (bipartite). Step 1 `tmp = A·x` (reduce over j, row-aligned); Step 2 `y = Aᵀ·tmp` (reduce over i, cross-owner). x is constant-folded (`j·π`), not a live array. No stencil, no in-place loop-carried dep.
- **Distributed arrays (owner-dims, block)**: `tmp` (id 0) is the only genuinely block DB — `block_parallel`, ownerDims `[0]`, blockShape `[250]`, 2 blocks (1 row-block/node).
- **Coarse spots**: `A` (id 1) DB is `<coarse>` (single 2×2×250×250 block) despite a committed `block_parallel`/ownerDims `[0,1]`/blockShape `[250,250]`/muBlockCount 4 layout — a grain contradiction; init EDT writes the whole matrix on one route. `y` (id 2) DB is `<coarse>` with no `arrayLayout` entry in SU2 (whole-buffer output funnel, also feeds the coarse verification reduction).
- **Movement**: none emitted (zero `su.halo`/`su.reduce_scatter`/`su.all_to_all`/`partial_reduction`). Step 1 is owner-local with A block-resident (no movement). Step 2 needs `su.reduce_scatter` of per-i-block partial y-vectors into j-owned y. No halo (not a stencil).
- **Reduction**: Step 1 inner reduce over j → row-aligned `tmp`, no cross-owner. Step 2 inner reduce over i (A/tmp owner dim) → cross-owner partial reduction: each node computes a partial y over its i-range; partials must sum into the fine y result. Currently left silently coarse, unrealized.
- **2n status**: FAIL, closed at the ARTS boundary (`SdeToArtsBoundaryDepAnalysis.cpp:1167-1169`, `verifyRawSuAccessCoveredByDep`): `atax.c:63:25` "touches a DB without a committed SDE access-window dependency". Root cause: A is read `block_parallel` but allocated `<coarse>`, so no `db_access_window` is raised over A in either SU (only `tmp` gets one); the boundary sees a raw load into a coarse DB carrying a block claim and fails closed. `atax-arts.ll` is 0 bytes.
- **Enhancement (granularity-preserving + lever)**: **S17 (reader/writer-grain reconcile) — primary, sufficient to compile**: emit A's `db_alloc` as `<block>` (2×2 grid, ownerDims `[0,1]`, blockShape `[250,250]`) to match its committed layout instead of `<coarse>`; the SDE access-window raise then authors A's read window in both SUs (as it already does for tmp) and the boundary check passes. Granularity-preserving (coarse→block, never coarser); do not demote A's layout. Then **S21 (K-grain split)**: give y a committed block layout (ownerDims `[0]`) and realize Step 2 as block-partial y-vectors → fine y result DB → combine via `su.reduce_scatter` (needs the redist reduction-edge gate at `RedistributionEdges.cpp:585,595-613` to fire on `reader.getPartialReductionAttr()`, plus repair of ARTS `PartialReductionSplitMaterialization` combine body). **S20** is avoided by keeping A on the full 2-D grid (transposed SU2 read stays owner-local). Step 1 needs no movement once A is block. Shares fix shape with bicg (identical Step-2 cross-owner reduce), the gemm/2mm/3mm/correlation grain-reconcile class, and the shared ARTS combine-body / dead CODIR `buildDepResultDimMaps` blockers. Do not hard-fail the coarse path while fixing (bicg relies on it).
- **Scaling limit**: no fundamental serial/dependency limit — genuinely 2n-scalable (no in-place dep). Bound is memory bandwidth + A residency: O(N²) data, low arithmetic intensity (one madd/A-element); only small O(N) y-partials move. Expected ~1.3–1.7× at 1n→2n megalarge (gemm/2mm class), not linear; 0× if A stays coarse.

### polybench/bicg

- **Pattern class:** Dual mat-vec (`q=A·p`, `s=Aᵀ·r`) — two independent owner-local row/column reductions; no stencil, no in-place, no transpose. A read under two complementary owner orientations.
- **Distributed arrays (owner-dims, block):** A (id 1) `block_parallel` ownerDims=[0,1] blockShape=[250,250] muBlockCount=4 (2×2 grid, read in both SUs); q (id 0) `<block>` ownerDims=[0]; s (id 3) `<block>` ownerDims=[0]. p (id 2) / r (id 4) `replicated` (broadcast operands).
- **Coarse spots:** A `db_alloc <coarse>` then reshaped to the 2×2 block memref (benign — grain recovered by block_parallel reader layout); p/r coarse+replicated (acceptable broadcast); verification checksum CU is `<single>` serial over q+s (off timed path). No coarse gather/bridge in the compute hot path.
- **Movement:** none required for the reductions (both owner-local). One unavoidable cross-node read of the off-diagonal 250×250 A blocks — one pass reads A row-banded, the other column-banded over the same 2×2 grid. No `su.halo`/`reduce_scatter`/`all_to_all` needed; p/r broadcast only.
- **Reduction:** healthy two-level shape already lowered — `q[i]=Σ_j A[i][j]·p[j]` reduces K=j (not the owner dim i), `s[j]=Σ_i r[i]·A[i][j]` reduces K=i (not owner dim j). Each result block computed locally → block partial → block result DB. No cross-owner reduction, no coarse accumulator.
- **2n status:** COMPILE FAIL at SDE→ARTS. `bicg.c:38` (`A[i][j]=…` init writer) parked in a `sde.cu_region <single> {serialReason=residual_source}` with no `arts.db_access_window` for arrayId 1 → ARTS fails closed ("DB-backed external memref … no committed access window"). `bicg-arts.ll` is 0 bytes. Same class as gemm/2mm/3mm/correlation; exact sibling of atax. The reductions are NOT the blocker.
- **Enhancement (granularity-preserving fix + lever):** **S17 (reader/writer-grain reconcile)** — commit a `write` layout-root + per-block `db_access_window{arrayId=1, ownerDims=[0,1], blockShape=[250,250]}` on the A-init writer, matching the grain the two compute SUs already commit. Split the owner-parallel A 2-D init (`A[i][j]=f(i,j)`, no cross-block dep) into its own block-distributed CU, leaving the scalar r/p init as the cheap residual remainder. S21 (K-grain split) is already satisfied (block partials present); secondary: lower the serial verify checksum to block-partials→scalar→combine (S21) so the verify tail scales too. S18/S19/S20 N/A.
- **Scaling limit:** No fundamental serial limit — A read-only in kernel, q/s independent fully owner-parallel outputs, no carried dependence. Ceiling is the one-directional remote read of the off-diagonal half of A (irreducible without replicating A); mild. Realistic ≈ 1.3–1.7× at 2n (gemm/2mm band), compute-bound at megalarge.

### polybench/convolution-2d

- **Pattern class:** Point-stencil with halo. 9-point out-of-place 3x3 convolution (`B[i][j] = Σ c·A[i±1][j±1]`, `i,j ∈ [1,N-1)`); read A / write B, no in-place dependency, no compute-loop reduction. Favorable — should be a top 2n scaler.
- **Distributed arrays (owner-dims, block):** A (array_id 1, read): SDE commits real HPF layout — `blockShape=[512,512]`, `kind=block_parallel`, `ownerDims=[0,1]` (2×2 grid), `muBlockCount=4`; RankExpandMu fires (`memref<2x2x512x512xf32>`). B (array_id 0, write): **no committed layout** — flat `array_layout_root write`, no owner-dims/blockShape.
- **Coarse spots:** Everything at runtime. Both DBs lower `<coarse> route(-1) {local_only}` (A's grid folded into `elementSizes=[2,2,512,512]` with `sizes=[1]`; B monolithic `[1024,1024]`); single `arts.edt <intranode> route(-1)` over whole `i:1..1023` band, 3 EDTs total. Block grain appears in types, never at runtime — node 1 does no work.
- **Movement:** Needed `su.halo` (1-element boundary exchange on the 512 splits, i±1/j±1). Realized: **none** — halo trivially satisfied because A is one coarse local DB.
- **Reduction:** None in kernel. Only the post-kernel diagonal checksum (`Σ B[i][i]`), outside the timed scop; coarse `cu_region <single>` gather, not the scaling concern.
- **2n status:** Compile PASS (no fail-closed; `sde-coarse-avoidance`/`verify-*` clean), Run Correct=YES (checksum `5.099021584427e+02`). In the 10/21 passing bucket — but **correct-by-non-distribution**: same coarse-fallback class as pre-fix jacobi2d/seidel-2d.
- **Enhancement (granularity-preserving fix + S17–S21 lever):** Two coupled fixes. **Fix A — commit B's writer block layout** (`{arrayId=0, role=write, blockShape=[512,512], ownerDims=[0,1], muBlockCount=4}`, owner-aligned to A's read) in `SdeLoopPatternFacts.cpp`/`LayoutAssignment.cpp`; this is **S17** (reader/writer-grain reconcile) and the shared gemm/2mm/3mm/correlation "writer lacks owner-dims" fix verbatim. **Fix B — emit `sizes=[N] <block>` allocs** at the SDE→ARTS boundary (`SdeToArtsBoundaryStorage.cpp`) instead of coarse `sizes=[1]` with grid-in-element-dims, so `hasMultipleAllocationBlocks` (`DbDistributedEligibility.cpp:26/333`) passes. Then **S18 (halo read window)** is primary: per-block A DBs with a 1-deep `su.halo`/`db_access_window<in>` replacing the coarse full-A copy (>500× movement reduction per 512-edge). S19/S20/S21 not applicable (single array family, owner-aligned write, no transpose/contraction).
- **Scaling limit:** Architecture-scale, not algorithmic. `DistributionPlanning.cpp:78-81` (`getStencilWorkerTarget`) explicitly caps the stencil physical-layout committer at single-locality ("stencil halo ownership is currently a single-locality layout fact; inter-locality expansion belongs after the dialect boundary has a concrete halo path"). Shared blocker for all owner-strip RO-halo stencils (jacobi2d landed via `perBlockHaloExchange` f4571a71; conv-2d not yet wired). Secondary risk: stencil classifier may not fire post-RankExpandMu since A is read through div/rem block-localized indices `StructuredOpAnalysis` can't resolve into neighbor offsets — classification must run before rank-expand or recover offsets from div/rem form. With Fix A+B + S18 and the cap lifted, near-linear 2n expected (dense 9-FLOP/point, halo surface ≪ block volume).

### polybench/convolution-3d

- **Pattern class:** Out-of-place 15-tap 3D point-stencil (±1 halo on all three dims), single timestep (`NREPS=1`), no self-read/in-place dependency (`isInPlaceSelfReadStencil` false) — embarrassingly parallel across owner blocks with thin halo comm.
- **Distributed arrays (owner-dims, block):** A (read, arrayId 1) gets a real committed layout — `ownerDims=[0,1,2]`, `blockShape=[64,64,64]`, `kind="block_parallel"`, `muBlockCount=8` (2×2×2), rank-expanded to `2x2x2x64x64x64` addressed via `div/rem 64`. B (write, arrayId 0) gets **no** `arrayLayout` / no owner-dims.
- **Coarse spots:** Both DBs land coarse despite A's 8-block plan — A = one `<coarse>` DB over the whole rank-expanded array, B = one `<coarse>` DB over `128³`; all acquires `partitioning(<coarse>)`, all EDTs `<intranode> route(-1)`. Zero block/compute_block DBs; node 1 idle.
- **Movement:** Needs `su.halo` (±1 face exchange on A's 64³ blocks). None present — the neighborhood is dissolved into `div/rem` index math (SdeRankExpandMu), so `classifyPattern` never returns `stencil`; cross-block reads are satisfied by replicating all of A to both nodes.
- **Reduction:** None in-kernel (pure map). Only a serial diagonal `checksum += B[i][i][i]` in a post-kernel `cu_region <single>`.
- **2n status:** PASSES — compiles clean through `complete`, Correct=YES (checksum == OMP). In the 10/21 pass group, but correct-but-coarse: not actually distributed (~1.12x is noise).
- **Enhancement (granularity-preserving):** Recover the ±1 neighborhood through A's `div/rem` block addressing (or classify on pre-rank-expand access facts) so `commitStencilPhysicalLayout` fires, committing B a writer owner-dim layout (`ownerDims=[0,1,2]`, `blockShape=[64,64,64]`) at A's existing 8-block grain and raising a halo read window on A. **S18** (halo read window) PRIMARY — compact ±1 face acquire over A's 64³ blocks instead of whole-array RO copy; **S17** (reader/writer-grain reconcile) — give B a per-block single-writer DB matching A's committed 64³ grain via the missing `commitWriterPhysicalLayoutFacts`. S19/S20/S21 N/A (no FEM grid, no transpose, no reduction K-grain). Identical fix resolves convolution-2d (same bug at lower rank).
- **Scaling limit:** Compute/halo ratio at 256³ ≈ 30:1 (comm-light) ⇒ near-linear potential, target ~1.7–1.9x at 2n. Current ceiling is the SDE single-locality stencil cap: `getStencilWorkerTarget` returns only `getLogicalWorkerCapacity()` (DistributionPlanning.cpp:78-83 — inter-locality halo deferred to "after the dialect boundary has a concrete halo path"). True 2n scaling needs (a) classification-through-rank-expand AND (b) raising the stencil committer to `getInterLocalityTargetWorkers` with a realized ARTS halo path (`arts.halo_slice`/perBlockHaloExchange). No in-place/serial limit blocks this — only the missing inter-locality halo wiring.

### polybench/correlation

- **Pattern class:** MIXED, four sequential SUs — Steps 1-3 (mean, stddev, center/scale) are O(m·n) row-local reductions/elementwise; Step 4 (correlation matrix `corr[i][j]=Σ_k data[i][k]·data[j][k]`) is the dominant O(m²·n) all-pairs symmetric K-reduction (matmul-like), owner-aligned on `i` with `j` spanning all owners.
- **Distributed arrays (owner-dims, block):** `data` M×N — BLOCK on dim 0, `ownerDims=[0]`, `blockShape=[64,128]`, 2 blocks (block DB, good); `mean` M and `stddev` M — BLOCK on dim 0, `ownerDims=[0]`, `blockShape=[64]`; 3 of 4 arrays cleanly row-block-distributed.
- **Coarse spots:** `corr` M×M — NO committed layout → `db_alloc[...<coarse>]` whole 128×128 single-block DB (the all-pairs output + diagonal checksum read live here); `mean`/`stddev` DBs emitted coarse (rank-expanded `2x64` allocated as one coarse DB, not 2 block DBs).
- **Movement:** Steps 1-3 none (row-local — each owner has its own `data` rows + `mean`/`stddev`). Step 4 needs full replication of centered `data` (each i-block reads `data[i]` AND `data[j]` for all j → read-side `su.all_to_all`) plus the symmetric `corr[j][i]` write is a cross-owner scatter (`su.all_to_all`/scatter on output). Currently neither named; `corr` left coarse. Halo NOT applicable (access is global, not neighbor-banded).
- **Reduction:** mean `Σ_j data[i][j]→mean[i]` and stddev `Σ_j(data[i][j]-mean[i])²→stddev[i]` are 1-D row-block-aligned, row-local (no cross-owner). Step-4 `k`-sum produces one scalar per `(i,j)` written directly to `corr[i][j]`, owner-local to the i-block — NO reduce-scatter, two-level reduction not needed (contrast atax/bicg).
- **2n status:** FAILS at `sde-to-arts` — `'sde.su_iterate' op commits groupBlockCount whose rank does not match the committed owner rank` (verifier `SdeToArtsBoundaryAccessLowering.cpp:1387`). `init_array` SU (`correlation.c:23`) is the only 2-D-iterated `data` writer and carries orphan rank-2 `groupBlockCount=[2,1]` (+ no `arrayLayout`) vs the array's authoritative rank-1 `ownerDims=[0]`; the corr SU has the same defect `[2,2]`. The 3 row-local SUs (`[2]`, rank 1) convert cleanly. SDE-planning bug, not transport. (Prior 1n "PASS 2.08x" declined the rank-2 split; at 2n the same shape block-distributes and trips the owner-rank verifier.)
- **Enhancement:** (1, blocker) S17 reader/writer-grain reconcile applied to *owner-rank*: in `commitBudgetReconciledLayout` (`DistributionPlanning.cpp:1308`), gate the `ownerDims.size()>=2` path (`:1331`) on the store genuinely block-striding each candidate owner dim — tighten `allExternalStoresCoverOwnerDims` (`:1123`) so the contiguous `j` dim is not counted; init then commits `ownerDims=[0]`, `groupBlockCount=[2]` matching Step-3 and readers (granularity-preserving, never coarsens). Fixes gemm/2mm/3mm/correlation at the writer commit, not per-kernel. (2, scaling) commit row-block layout for `corr` (`ownerDims=[0]`, `blockShape=[64,128]`) → block-local output writes (coarse→block); author `data` replication as one read-side `su.all_to_all` (S20 target≠source repartition, producer-side broadcast — same primitive as gemm/2mm/3mm B-operand); keep symmetric `corr[j][i]` block-distributed (combine, never coarse funnel) or reconstruct from transpose.
- **Scaling limit:** compute-dense O(m²·n) owner-aligned on `i` (each node owns m/2 independent output rows) — best-scaling kernel in the suite, previously measured **2.43× at 2n**; expect ~1.8-2.4× at 2n megalarge once `corr` is block + `data` is a single all_to_all. Fundamental floor: `data` read is inherently all-to-all (every output row pairs with every input row), O(m·n) per-node replication volume amortized by O(m²·n) compute at 2n but grows and bounds speedup at high node counts. No in-place serial dependency (unlike seidel-2d); only the necessary inter-step barriers.

### polybench/gemm

- **Pattern class:** Matmul / K-reduction. `C[i][j] = alpha·(Σ_k A[i][k]·B[k][j]) + beta·C[i][j]`. The `k`-reduction is loop-private (scalar `alloca` accumulator, single store to `C[i][j]`) — a per-element dot product, not a cross-owner reduction. Owner-parallel over `i` (legally over `j`). Compute-dense `O(n³)`, owner-aligned output.
- **Distributed arrays (owner-dims, block):** A (id 1) = `block_parallel`, ownerDims `[0]`, blockShape `[64,128]`, muBlockCount 2 (row-blocked over `i`, rank-expanded `memref<2x64x128>`); B (id 2) = `replicated`, ownerDims `[]`, full 128×128 on every node (correct for the all-rows `B[k][j]` access); C (id 0) = result, carries no consistent kernel layout fact (the problem array).
- **Coarse spots:** C result DB lands `<coarse>` (single whole 128×128) instead of owner-aligned 2× `64×128` block; B replicated whole (acceptable, read-only); coarse C is gathered for the serial diagonal `checksum` tail. C coarse is where scaling would die.
- **Movement:** None for the core matmul — A row-blocked over `i`, B replicated, C should be row-blocked over `i` aligned with A → fully owner-local compute. No halo, no reduce_scatter, no all_to_all. Only "movement" is the coarse gather of C for the serial checksum.
- **Reduction:** Loop-private per output element (`k`-accumulator into scalar, one store per `(i,j)`). Reduces to a scalar per element, not an array-level reduction → no `su.reduce_scatter` required. This is the "clean" owner-aligned case: result shape = `C[i][j]`, owner-aligned with A's `i`-block.
- **2n status:** FAIL — `build_arts: fail`, `run_arts: skip` (OMP reference passes, checksum `1.573450018167e+02`). Aborts at `sde-to-arts` (`SdeStorageToArtsDb`) with `'sde.su_iterate' op commits groupBlockCount whose rank does not match the committed owner rank`. Root cause: writer owner-rank disagreement on a `<block>` array — the init/data-parallel writer commits a grouping fact ranked over loop space (`groupBlockCount=[2,1]`, includes phantom non-owner `k`/`j` slot) while the committed storage owner rank is 1 (`ownerDims=[0]`). The over-ranked grouping fact disagrees with the array's own 1-D owner storage; rejected at `SdeToArtsBoundaryAccessLowering.cpp:1386`. Class-1 ("init/elementwise WRITER lacks committed owner-dims → coarse-fallback refused"), here sharpened to an over-ranked grouping fact.
- **Enhancement (granularity-preserving fix + S17–S21 lever):** **S17 (reader/writer-grain reconcile) — primary.** Make the init/data-parallel writer commit a `groupBlockCount` projected onto the array's storage owner dims only (rank 1, `[2]`), dropping the no-op `1` slot on the non-owner loop dim, so the writer grouping fact matches the committed DB owner grain. Fix belongs in SDE at the commit site (`commitCuGroupBlockCounts` / `commitWriterPhysicalLayoutFacts` handed owner dims restricted to storage owner rank), not as a downstream boundary repair, per the fail-closed-or-transform standard. Same reconcile applied to the C init writer keeps C `<block>` (2× `64×128`) and removes the coarse C DB. This shared S17 fix unblocks the whole gemm/2mm/3mm/correlation class at once. Secondary, for finer grain beyond row-parallel: **S20 (target≠source repartition)** + **S21 (K-grain split)** — adopt a 2-D `C`-tile owner plan (`ownerDims=[0,1]`, as `commitMatmulPhysicalLayout` already commits) with column-blocked `B[*,j]`, introducing K-partials reduced via `su.reduce_scatter` into a fine `C`-tile result DB (never a coarse gather). Prefer row-parallel first (no reduction, no halo, no gather); adopt 2-D/K-split only if 2 nodes saturate. S18 (halo) and S19 (FEM grid) do not apply.
- **Scaling limit:** No fundamental serial limit — independent output rows/tiles, clean per-element `k`-reduction kept node-local (each node fully reduces its own `k` for its `C` rows, zero cross-node reduce). Expected ~1.5× at 2n for the row-parallel plan (consistent with recorded 1.52×); ~1.8–2× achievable by removing B replication (column-block `B`) and the coarse C init. The only ceilings are broadcast cost of replicated B and the coarse C init touch — both addressable, neither fundamental.

### polybench/jacobi2d

- **Pattern class**: Point-stencil with halo (5-point 2D Jacobi), double-buffered ping-pong (A↔B per timestep); trailing diagonal-only checksum reduction, not part of stencil compute. Fully parallel within a sweep (no in-place serial barrier, unlike seidel-2d).
- **Distributed arrays (owner-dims, block)**: A (`%5`) and B (`%6`), both `mu_alloc memref<2x2x128x128xf32>` = 2×2 grid of 128×128 blocks (budget-driven, scales with N). `arrayLayout`: `kind="block_parallel"`, `muBlockCount=4`, `ownerDims=[0,1]` (both spatial dims), `blockShape=[128,128]`, `budgetBlockShape=[256,256]`. Only `role="read"` (neighbor-source) facts are serialized; the writer carries no serialized `role="write"` fact.
- **Coarse spots**: Both arrays collapse to a single `<coarse>` whole-array DB at `sde-to-arts` (`db_alloc[<inout>,<heap>,<write>,<coarse>] route(-1)`, all acquires `partitioning(<coarse>)`, all EDTs `<intranode> route(-1)`). 0 per-block DBs, 0 owner map, 0 internode EDTs. The 2×2 grid survives only as inner `divui/remui` indexing in one buffer. Collapse point is `sde-to-arts`, not `create-dbs`.
- **Movement**: Needed: `su.halo` k±1 neighbor exchange on owned dims [0,1] across the 2×2 blocks (radius [1,1] from `queryNeighborhoodAccessInfo`), plus per-timestep epoch separation. Emitted: none (coarse single-DB co-locates all data on rank 0, making movement trivially unnecessary at the cost of zero distribution).
- **Reduction**: Only the checksum — `cu_region <single>` serial loop summing diagonal `A[i][i]` over the coarse array to a scalar `memref<f64>`. Coarse serial gather, scalar result; not a kernel reduction, not the failing path.
- **2n status**: Compiles cleanly and runs correct (checksum 3.196312170126e+01 matches OMP), but correct-but-NOT-distributed — entirely on rank 0, node 1 idle. Failure-class-1 (writer lacks serialized owner-dims → coarse-fallback), stencil coarse-fallback subclass.
- **Enhancement (granularity-preserving fix + S17-S21 lever)**: **S18 (halo read window)** primary, **S17 (reader/writer-grain reconcile)** root. Fix the home-layout recovery asymmetry in `RedistributionEdges.cpp`: the collector `collectRedistributionEdges` builds `homeByArrayId` only from serialized `role="write"` facts (`:499` filter), so jacobi2d's read-only-stamped arrays get no home → silent `:543 continue`, no edge. The verifier already does two-tier recovery (serialized fact, then `homeLayoutFromCommittedPhysical` for `array_layout_root write`); add the same tier-2 pass to the collector using the dead-but-built helpers `homeLayoutFromCommittedPhysical` (`:48`) + `recordHomeLayout` (`:64`, zero callers). Once `home` exists, the reader's [1,1] halo fires a Halo edge → `sde.su_halo` → per-block DBs + halo faces (face-only [1,1] exchange, fails closed on corners/non-unit). Preserve 4-block MU grain (do not coarsen to node count); reuse `swmr_block_lane` epoch separation for the ping-pong halo retirement. Validate with full-array oracle (not diagonal checksum) at small AND medium. Same collector blind spot blocks gemm/2mm/3mm/correlation (writer geometry in expanded MU type, not serialized) — high-leverage shared lever.
- **Scaling limit**: O(N²) compute, O(N) halo per step — comm-bound. Even perfectly block-distributed with halo-only traffic, expect ≈1.0× or mild slowdown at 2n (like conv-2d/3d); only O(N³) kernels (gemm/2mm/correlation) scale. Paper-relevant result is correct + genuinely block-distributed with halo-only traffic (demonstrating the halo primitive), not a speedup.

### polybench/seidel-2d

- **Pattern class:** In-place 2D point-stencil with a loop-carried Gauss-Seidel dependence (`A[i][j]` reads just-updated `A[i][j-1]`, `A[i-1][*]`). Not Jacobi-parallelizable on `i`/`j`; SDE exposes parallelism via diagonal **wavefront skew** (`tryRealizeWavefrontSkew`/`realizeWavefrontSkew`, `DistributionPlanning.cpp:543-667`). Outer time loop (TSTEPS=10/20) fully sequential.
- **Distributed arrays (owner-dims, block):** One array only — `A`, `memref<500x500xf64>`, `array_id(0)`, in-place read+write. 2n SDE *attempts* BLOCK: `su_distribute<owner_compute>`, `ownerDims=[0,1]`, `pattern=stencil_tiling_nd`, `writeFootprint=[1,1]`, `accessMin/Max=[-1,-1]/[1,1]`. But owner-dims are never committed (see Coarse spots). 1n: no distribution at all (capacity 1 skips wavefront) — serial, Correct=YES.
- **Coarse spots:** Everything. The wavefront SU carries only `array_layout_root read` (write root demoted to read at `DistributionPlanning.cpp:607-615`), so no `arrayLayout` write fact, no `physicalOwnerDims`/`physicalBlockShape` (zero occurrences in pass dump). DB lowers as a single `arts.db_alloc[...<coarse>]` 500x500 — the exact coarse grain the north star forbids, and the reason compile fails closed.
- **Movement:** Stencil `su.halo`, ±1 in both dims (from `accessMin/MaxOffsets`). `buildWavefrontOwnerStoragePlan` already derives the correct `haloShape`, but **no `sde.access_window`/halo-slice is ever committed** — only the bare offset attributes survive.
- **Reduction:** None on the distributed path. Only a verification diagonal checksum `sum(A[i][i])` in a serial `sde.cu_region<single>` (`sde.mlir:133-138`); coarse sequential, not a cause of failure.
- **2n status:** FAIL — fail-closed at the **SDE→ARTS boundary** (`SdeToArtsBoundaryAccessLowering.cpp:1180-1185`), not in DistributionPlanning: `'sde.su_iterate' op has movement, halo, or physical scheduling facts without committed access windows; refusing coarse ARTS realization`. (Memory note `g2a_seidel_failclosed_landed` is STALE — the guard moved one stage later now that the wavefront transform fires.)
- **Enhancement (granularity-preserving fix + lever):** In `realizeWavefrontSkew`, stop demoting the output `array_layout_root` to `read` (`DistributionPlanning.cpp:607-615`); preserve/author the **write** fact on `ownerPhysicalDims=[0]` (row-block, `physicalBlockShape[0]=step`) so the already-inert `commitWriterPhysicalLayoutFacts` (`:619-622`) actually commits the layout `buildWavefrontOwnerStoragePlan` already computed. This unblocks MU rank-expand → access-window raise → boundary lowering. Levers: **S17** (writer-grain reconcile — primary) + **S18** (RO ±1 halo read window, same carrier proven for jacobi2d). S19/S20/S21 N/A. Shared with the gemm/2mm/3mm/correlation "writer lacks committed owner-dims" class — audit all post-clone `commitWriterPhysicalLayoutFacts` callsites for the same demote-then-commit-nothing ordering.
- **Scaling limit:** Fundamental — genuine loop-carried lexicographic dependence; wavefront is the only legal parallelism and is bounded by anti-diagonal wave width with serialized wave-to-wave fronts. Even when correct, expect ~1.0–1.3x at 2n (front-sync/halo-exchange bound), not gemm/correlation-class scaling. Small TSTEPS (10/20) adds pipeline fill/drain overhead. The win is correctness + block-distribution (exits the 11-fail bucket), not strong scaling.

### seissol/volume-integral

- **Pattern class** — Element-parallel two-stage dense GEMV/contraction (per `elem`: stage-1 reduce over `b`=N_BASIS into a private `buffer[q]`, stage-2 reduce over `q`=N_QUAD into `fluxOut[elem][b]`). Outer `elem` loop embarrassingly parallel; both reductions element-local; no in-place, no halo, no cross-element dependency. NOT a stencil, NOT scatter-add FEM.
- **Distributed arrays (owner-dims, block)** — SDE *authors* a valid dim-0 data-parallel plan but only on reads: `dofs` (id 2, read) = `block_parallel ownerDims=[0] blockShape=[5000,30] budget=[10000,30] muBlockCount=2`; `gradMatrix` (id 1) + `fluxMatrix` (id 3) = `replicated` (36×30, ~4 KB each, correctly small/read-only). The write array `fluxOut` (id 0) is **absent from `arrayLayout`** — no committed owner-dim.
- **Coarse spots** — Everything realizes coarse: 4× `db_alloc[...,<coarse>] route(-1)`, all 7 `db_acquire` `partitioning(<coarse>)`, sizes 1200000/4320/4320/1200000 B. Zero block DBs, zero owner map. Compute is one coarse `<intranode>` EDT over the full `%c0..%c10000` element range on rank 0; rank 1 idle. SDE planning byte-identical 1n vs 2n.
- **Movement** — None present, none needed. Ideal pattern is pure owner-compute block over `elem`: block `dofs`+`fluxOut` on dim 0, keep matrices replicated, run each block's EDT on its owner node. No halo, no reduce_scatter, no all_to_all — zero kernel-phase inter-node traffic.
- **Reduction** — Two element-local dense contractions (over `b`, then over `q`), both fully inside one `elem` iteration via a private `alloca` `buffer[36]`. NOT cross-owner; no reduce_scatter / partial→fine-result→combine required (unlike atax/bicg). The only coarse touch is the diagonal checksum `Σ_i fluxOut[i][i]` (a `<single>` serial read outside the timed kernel).
- **2n status** — Compiles AND passes (EXIT=0, Correct=YES, checksum `1.494466e+00` vs OMP). **NOT in the 11-fail set** — but fails *open*: silently coarse/single-node, correct because coarse-on-rank-0 is numerically fine. This is a NORTH-STAR (coarse-where-scaling-dies) target, the same fail-class-1 signature as gemm/2mm/3mm/correlation, just non-fatal.
- **Enhancement** — Commit a `block_parallel` WRITE layout for `fluxOut` (id 0) with `ownerDims=[0]`, owner-aligned to the already-committed `dofs` read block grain, by projecting the proven `elem` (loop dim 0) owner map onto the write target in `DistributionPlanning` (`selectSingleWriteLayoutFact`/`chooseMappedSdeOwnerLoopDims`). Then `RankExpandMu` rank-expands both MUs, `CreateDbs` emits per-block single-writer DBs (+replicated matrices) and `RealizeEdtDistribution` sets real routes — no contract bent, no movement op needed. **Lever: S17 (reader/writer-grain reconcile)** primary; **S19 (FEM one-owner-grid)** to force `dofs`/`fluxOut`/compute onto one elem-owned grid. S18/S20/S21 N/A (no halo, no repartition, no cross-owner K-reduction). Grain may go finer than node-count (budget `[10000,30]`); don't cap at 2. Shared lever with gemm/2mm/3mm/correlation and the FEM trio; harden `VerifySdeCoarseAvoidance` to fail-closed on owner-eligible-but-unlaid writers.
- **Scaling limit** — None fundamental: no recurrence, no in-place, no halo, no cross-node reduction. Cleanest scaler in the suite — once the writer layout lands, work splits N/nodes with zero kernel-phase comm, expected ~1.8–2.0x at 2n (bounded by per-node throughput + startup). Must be measured at `--size megalarge`; default N=10000 (~12 ms kernel) is too small to amortize startup.

### specfem3d/stress

- **Pattern class:** 3D point-stencil, halo width 1 (central-difference `±1` neighbor reads on x/y/z), six in-place pointwise `+=` outputs, `NREPS=1` (single sweep, no timestep loop); Jacobi-style (no cross-iteration RAW) — not seidel-class.
- **Distributed arrays (owner-dims, block):** reads only — `vx,vy` ownerDims=[2] (z-split, 2 blocks, blockShape [40,40,20]); `vz,mu,lambda` ownerDims=[0,1,2] (8 blocks, blockShape [20,20,20]). The six **write** outputs `sxx,syy,szz,sxy,sxz,syz` (arrayIds 0-5) declare `array_layout_root write` but carry **no arrayLayout entry / no owner-dims** — same defect class as gemm/2mm writers.
- **Coarse spots:** 100% coarse at runtime. SDE block facts (muBlockCount, ownerDims, blockShape) are dropped at the SDE→ARTS boundary; create-dbs emits all 11 DBs `<coarse> route(-1)` driven by ONE `arts.edt <sync> <intranode>`. Plus a serial `cu_region <single>` (`residual_source`) for the verification reduction.
- **Movement:** needed `su.halo` (width-1 face exchange on vx/vy/vz across the owner split). Present: **zero** — no halo/reduce_scatter/all_to_all. The ±1 windows are dissolved by SdeRankExpandMu into `divui/remui` block-local index math, so no access window survives for halo realization (S18 must run before/with RankExpandMu).
- **Reduction:** none in the kernel proper (pointwise accumulate). Only the verification checksum — diagonal sample `checksum += sxx[i][i][i]+…+syz[i][i][i]` over all six distributed outputs in a coarse single SU. Latent failure: would read distributed arrays off-owner once writers distribute; safe today only because writers are coarse.
- **2n status:** compile PASS through all stages (no fail-closed); run PASS Correct=YES (small NX=20, checksum `-9.936160e-04` matches 1n) but **NOT distributed** — 1 EDT, all coarse, speedup basis n/a. Coarse collapse is intrinsic, reproduces under the 2n arts.cfg at NX=40. Megalarge has only 64t_1n (no 2n collected — coarse single-EDT offers nothing).
- **Enhancement:** (a) **commit owner-dims on the six write outputs** — fully owner-parallel `owner_compute` stores, ALIGN to the inputs' owner axis so RealizeEdtDistribution emits per-block EDT waves instead of one coarse intranode EDT (granularity-preserving, removes coarse fallback). Seed logic: `LayoutAssignment.cpp:96-118` `isWriterParallelOwnerPosition` not firing (likely permuted store index `%11[%arg2,%arg1,%arg0]` vs loop order defeats `isSchedulingLoopDim`). (b) **S18 (halo read window)** — raise the ±1 windows before/with RankExpandMu so ARTS authors `su.halo` face-exchange (O(N²)) instead of full RO copy. (c) **S17 (reader/writer-grain reconcile)** — force coarse-writer → block to match reader block grain. (d) **S19 (one-owner-grid)** — all 12 i/j/k arrays share one owner grid so output blocks co-locate with the input blocks they read. Also lower the diagonal checksum as two-level reduction (per-block partials → fine result DB → combine). Not applicable: S20 (source==target grid), S21 (no K-contraction).
- **Scaling limit:** no fundamental serial bound (Jacobi-style, fully owner-parallel). Compute O(N³) vs halo O(N²) face-exchange is surface-to-volume favorable → **scaling class** (gemm/correlation-like), realistic ~1.3-1.8× at 2n (12 arrays × halo setup caps below ideal 2×). Caveat: fix spans coordinated S18 window-raise + writer owner-dim commit (memory `g2a_owner_strip_ro_halo_verdict` classed this RO-halo path architecture-scale because RankExpandMu dissolves the halo and the writer layout never commits — not a one-line gate). Shared with specfem3d/velocity (structurally identical), sw4lite/vel4sg, and the broader stencil-halo class (jacobi2d/jacobi-for already have the `chooseCollective halo` path).

### specfem3d/velocity

- **Pattern class**: 3D point-stencil, one-sided forward-difference halo (`arr[i+1]-arr[i]`, +1 high-side only), jacobi-style. Single perfectly-nested `k,j,i` loop, in-place `+=` to disjoint cells, no cross-iteration dependence. NREPS-wrapped (`affine.for 0 to 10`). Fully owner-parallel — no serial limit.
- **Distributed arrays (owner-dims, block)**: SDE authors block layouts for the **7 read arrays only** — stress `sxx/sxy/sxz/syy` `block[20,20,10]` ownerDims `[2]` muBlockCount 2; `rho/szz/syz` `block[10,10,10]` ownerDims `[0,1,2]` muBlockCount 8. Rank-expanded by `SdeRankExpandMu`. budgetBlockShape `[20,20,20]` (smaller than block ⇒ real partition exists). Owner-dim choice **inconsistent** ([2] vs [0,1,2]).
- **Coarse spots**: **Everything.** All 10 DBs `<coarse>` whole-array on `route(-1)`; whole SU lowers to **1 `arts.edt`**. Zero block DBs. The 3 **WRITE** arrays vx/vy/vz (ids 0,1,2) are **absent from `arrayLayout`** — no committed owner-dims — which forces ARTS to coarse-fall-back all 10 arrays including the read arrays that had block facts.
- **Movement**: Needed — `su.halo` (one-sided +1 read window on the 6 stress reads; k for ownerDim-[2], i/j/k for ownerDims-[0,1,2]). Emitted — **none** (no halo/reduce_scatter/all_to_all; one coarse DB ⇒ no movement generated).
- **Reduction**: None in kernel (writes are independent `+=` accumulations, not cross-element reduce). Only a tiny diagonal-sample verification checksum in a `<single>` `residual_source` region — serial coarse read, harmless, not the blocker.
- **2n status**: **Compiles + lowers clean (exit 0), Correct=YES** at small/large 1n and 2n — NOT in the 11-fail set; the stale `build_failed` results rows are from an older build. But **distributes nothing**: 1 coarse EDT, 10 coarse DBs, second node unused. Correct-but-coarse — the prototypical scaling-death case.
- **Enhancement (granularity-preserving fix + S17–S21 lever)**: Commit writer-side owner-dims so vx/vy/vz block-distribute matching the read partition. Two coupled gates in `DistributionPlanning.cpp`: (A) owner `k` loop is untiled (`su_iterate step 1`) so `physicalLayoutMatchesRealizedLoopSteps` (1442-1444) rejects the stencil commit — tile owner `k` to the budget block step; (B) `SdeRankExpandMu` dissolves the +1 halo into div/rem so `readStencilHaloForOwnerDim` (160) raises 0 — recover the one-sided forward halo through the rank-expanded indexing. **S18 (halo read window)** primary; **S17 (reader/writer-grain reconcile)** required to align the coarse logical-3D writers to the readers' owner-dim-2 budget block. Pick one consistent owner grid (dim 2, 2-way 192+halo) across all 10 arrays; share the halo exchange across the 3 writers. S19/S20/S21 N/A.
- **Scaling limit**: No fundamental serial bound — pure owner-parallel 10-array compute-dense stencil with low halo:volume ratio (one 288×288 plane vs 192×288×288 block volume). With per-block writers + thin one-sided halo, expect near-linear kernel scaling; realistic **2n ~1.6–1.9×**, on par with owner-aligned compute-dense scalers (conv/gemm). Only ceiling is halo-exchange latency at small sizes ⇒ gate on megalarge. Shared fix with **specfem3d/stress** (centered-stencil sister) and **sw4lite/vel4sg-base** (same owner-strip RO-halo family).

### sw4lite/vel4sg-base

- **Pattern class**: 3D 7-point velocity stencil (out-of-place, data-parallel over k). 3 writers `vx/vy/vz[i][j][k] += DT/rho * (d/dx,d/dy,d/dz of 6 stress arrays)`; reads carry ±1 neighbor offsets in x/y/z; writes are point-local. No reduction, no matmul, no self-read recurrence; NREPS=1. Correctly declined by `tryRealizeWavefrontSkew` (not Gauss-Seidel).
- **Distributed arrays (owner-dims, block)**: SDE commits *read-side* block layout on owner dim k (dim 2): stress reads + rho (array_id 3–9) `block_parallel, ownerDims=[2], blockShape=[20,20,10], muBlockCount=2` (rho id 9 = `replicated`). The 3 writers (array_id 0/1/2, `20x20x20`) have **no committed arrayLayout / ownerDims** — they stay monolithic.
- **Coarse spots**: ALL 10 DBs realized `<coarse>` at the ARTS boundary because the writers carry no block facts; CreateDbs coarse-falls-back every DB to keep reader/writer grain consistent. Plus two `<single>` `residual_source` regions: serial `init()` (`idx++` recurrence) and the diagonal checksum.
- **Movement**: NEEDED = `su.halo` width-1 on the k owner dim for stress reads 3,4,5,7 (±1 z-neighbor across the block seam). COMMITTED = none (0 halo/reduce_scatter/all_to_all/gather ops in the SDE plan).
- **Reduction**: none. Pure elementwise stencil write; the only sum-like construct is the host verification checksum (separate `<single>` region), not a distributed reduction.
- **2n status**: COMPILES + lowers (EXIT=0) and is **Correct=YES** at 2n (arts `4.295315124137e-07` == omp `4.295315053082e-07`, run `20260614_140108_054299`) — but collapses to a **single coarse `<intranode>` EDT**, so 2n ≈ 1n (~1.0×, second node idle). It does NOT fail closed; it silently degrades. (The prior "architecture-scale fail-closed" verdict is stale.)
- **Enhancement (granularity-preserving fix + S17–S21 lever)**: Commit writer block facts for single-owner-dim point-local stencil writers. Root gate = `DistributionPlanning.cpp:946` (`getLoopIndexedSingleWriterOutputShape` rejects writers with `ownerPhysicalDims.size() < 2`); vel4sg writers are owner-indexed on exactly one dim (k) so they never reach `commitWriterPhysicalLayoutFacts`. Lower the floor for the no-write-halo stencil case (or add a single-owner-dim writer-commit branch mirroring `:455–460`) so writers adopt `[20,20,10] ownerDims=[2]`; ARTS then authors `muBlockCount=2` per-block single-writer DBs instead of `<coarse>`. **S17** (reader/writer-grain reconcile) is the delivering mechanism — writers adopt the same k-block grain as the reads. **S18** (halo read window) supplies the `su.halo` width-1 k-window so ARTS does a 2-plane halo exchange, not a full-array gather. S19/S20/S21 do not apply (source==target layout, no K-reduction, regular grid not FEM scatter).
- **Scaling limit**: No fundamental serial limit — out-of-place, fully data-parallel over k, compute-bound (≈15 flops/point over 6 input planes; halo O(N²) vs compute O(N³), comm/compute→0 at megalarge). Expected ~2× kernel-proper at 2n, realistic 1.5–1.9× until the serial `init()`/checksum residual is distributed (reconstruct affine `idx=(i*NY+j)*NZ+k` per block to break the `idx++` recurrence, letting each node init its own k-block). Residual Amdahl ceiling is the init+checksum only. Shared root cause with sw4lite/rhs4sg-base and specfem3d/stress+velocity (same `:946` gate); S18 k-halo reuses the jacobi2d/conv `chooseCollective halo` + ARTS `perBlockHaloExchange` path.

---

## 5. Prioritized Enhancement Roadmap

Ordered by kernels-unblocked per unit risk. All entries are granularity-preserving (coarse → block, never the reverse) and fail-closed-or-transform at the owning layer's commit site, not as a downstream boundary repair.

### P1 — S17 single-owner writer-grain reconcile (matmul/elementwise writer class)

**Unblocks: gemm, 2mm, 3mm, correlation, bicg, atax (A-read), layernorm, volume-integral, specfem3d/stress, specfem3d/velocity (writer half), sw4lite/vel4sg-base — up to 11 kernels.**

The dominant defect across the suite: an init/elementwise/matmul-output **writer commits no owner-dims** (or an *over-ranked* `groupBlockCount` with a phantom non-owner slot, e.g. `[2,1]`/`[2,2]` vs rank-1 `ownerDims=[0]`). At 2n this is refused (gemm/2mm/3mm/correlation) or silently coarse (layernorm/volume-integral/specfem/sw4lite). Single fix at the SDE commit site: project the writer grouping fact onto the array's storage owner rank and author a `write` layout-root + `db_access_window` owner-aligned to the readers' already-committed grain. Audit every `commitWriterPhysicalLayoutFacts` callsite for the demote-then-commit-nothing ordering (seidel, jacobi2d) and the over-ranked grouping (gemm/correlation). **Highest leverage, lowest risk — pure additive writer commit, no new movement op.**

- gemm/2mm/correlation: project `groupBlockCount` to rank-1 owner (`commitBudgetReconciledLayout` / `commitCuGroupBlockCounts`); tighten `allExternalStoresCoverOwnerDims` so a contiguous non-owner dim is not counted.
- 3mm: accept rank-expanded single-owner init writers (`DistributionPlanning.cpp:1331/:1659`).
- layernorm/activations: have `commitWriterPhysicalLayoutViaMuType` author the arrayLayout write entry when absent, or stop `hasCommittedCuMuPartitionFacts` short-circuiting on a bare `groupBlockCount`.
- vel4sg/specfem: lower the `ownerPhysicalDims.size() < 2` floor (`DistributionPlanning.cpp:946`) for single-owner point-local stencil writers.
- volume-integral/seissol: project the proven `elem` owner map onto the write target (`selectSingleWriteLayoutFact`).

### P2 — S17 coarse→block DB realization for committed-block arrays (atax, bicg)

**Unblocks: atax, bicg (compile).**

A array carries a committed `block_parallel`/`ownerDims=[0,1]` layout but its `db_alloc` is still emitted `<coarse>`, so no read access window is raised and the boundary fails closed. Emit A's DB as `<block>` matching its committed layout; the existing access-window raise then authors A's read window in both SUs. Low risk (sibling-proven by `tmp`/`q`/`s` in the same kernels). Do **not** hard-fail the coarse path while fixing — bicg's broadcast operands rely on it.

### P3 — S18 + S17 stencil halo path (stencil/halo class)

**Unblocks real distribution for: jacobi-for, poisson-for, jacobi2d, convolution-2d, convolution-3d (already pass but coarse); seidel-2d (compile); specfem3d/stress+velocity, sw4lite (the read-halo half).**

Two coupled, higher-risk pieces:
1. **Classification through rank-expand.** `SdeRankExpandMu` dissolves the ±1 neighborhood into `div/rem` index math before `classifyPattern` runs, so no halo window survives. Either run classification before rank-expand or recover offsets from the div/rem form.
2. **Inter-locality halo committer + ARTS path.** Lift the single-locality stencil cap (`getStencilWorkerTarget`, DistributionPlanning.cpp:78–84) to `getInterLocalityTargetWorkers`, add the dead `homeLayoutFromCommittedPhysical`/`recordHomeLayout` tier-2 recovery to the `RedistributionEdges` collector, and wire the landed jacobi2d `perBlockHaloExchange`/`chooseCollective halo` path for the remaining stencils. Emit `sizes=[N] <block>` allocs (not coarse `sizes=[1]`) so `hasMultipleAllocationBlocks` passes.

Classified "architecture-scale" by prior verdicts — sequence carefully; jacobi2d is the reference for the ARTS mechanism. 2D stencils are comm-bound (~1.0× even when fixed); the 3D stencils (conv-3d, specfem, sw4lite) are the real scalers here.

### P4 — S21 two-level reduction split (full/partial reduction class)

**Unblocks: stream, activations (compile, via softmax/checksum), batchnorm (compile); scaling tail of atax Step-2 and every kernel's diagonal checksum.**

Author reductions over distributed arrays as block/tile partials → small fine result DB → combine, never a coarse `mode=in` gather. Two pieces:
- **Reduction matcher generalization** (batchnorm): accept a perfectly-nested reduction whose outer loop is the committed owner block dim (`ScalarBlockReduction` currently rejects nested `scf.for`).
- **Cross-owner reduce_scatter** (atax Step-2): fire the redist reduction-edge gate on `getPartialReductionAttr()` (`RedistributionEdges.cpp:585,595-613`) and repair the ARTS `PartialReductionSplitMaterialization` combine body.
- **DB-promote small reduction targets** (activations softmax) so the internode EDT stops capturing a raw heap memref (`Dialect.cpp:321`).

Medium risk (touches the ARTS combine materializer + the dead CODIR `buildDepResultDimMaps`); shared enforcement point (`verifyRawSuAccessCoveredByDep`) means one mechanism covers the class.

### P5 — S17 unclassified owner-local read admission (pooling, activations)

**Unblocks: pooling (compile), activations (read-admission half).**

Relax `getUnclassifiedOwnerSliceLayout` (`MemoryUnitRealization.cpp:127-132`) to admit an external read whose owner dims match the writer's committed owner-local block, instead of requiring read==write-array. Low risk and self-contained; the layout is already fully block — only the admission gate blocks it. (Do **not** treat the 2×2 pooling window as a halo — it is owner-local over spatial; batch is the owner.)

### P6 — S20 repartition / S21 K-tile (scaling beyond row-parallel, deferred)

**Improves (does not unblock): 3mm (F gather), correlation (data all_to_all), gemm/2mm (column-block B at high node count).**

Insert a real producer-side `su.all_to_all`/repartition when a consumer reads a producer under a different owner/contraction layout, and adopt 2-D C-tile + K-split only when 2 nodes saturate. Lowest priority — these are speedup-ceiling refinements, not compile/distribution unblockers, and the primitive needs no new ARTS mechanism.

### Excluded from in-bounds fixes

**seidel-2d scaling** (genuine loop-carried Gauss-Seidel dependence — wavefront is the only legal parallelism; the win is correctness + block distribution via P1/P3, not strong scaling) and the **residual single-node ~3× stream gap** (runtime/codegen: AVX2/NUMA, orthogonal to the 2n distribution fixes).
---

## Appendix M — Category-A 2n realizability chain: measured root cause (matmul / init-writer class)

This appendix records the **measured** (not inferred) failure chain for the
Category-A kernels (gemm, 2mm, 3mm, correlation, bicg) at 2 nodes, established
by instrumenting the live v4 passes at HEAD `6194f3549`. The experimental
patches that produced this trace were **reverted** — the baseline (1n 21/21,
SDE lit 29/29) is preserved. This is the evidence base for any future attempt;
it supersedes the earlier inference that Category-A is a single init-writer
owner-rank fix.

### The chain (each layer was reached only after fixing the one above it)

1. **LayoutAssignment does not author the write-role `arrayLayout` for a
   `writerViaMu` block-parallel writer.**
   `LayoutAssignment.cpp:656-668` routes such a writer to `writerCommits[suId]`
   and skips `updates[suId].entries.push_back(entry)`, so
   `op.setArrayLayoutAttr` never emits the write-role layout. The committer
   `commitWriterPhysicalLayoutViaMuType` only *rewrites* an existing layout
   (`rewriteWriterArrayLayoutToPhysicalShape` early-returns on a null attr,
   `SdeCommittedFactUtils.h:352-354`), so the init writer's owner-dim identity
   is never durably recorded. *Authoring it advances the compile past the init
   owner-rank error — but exposes layer 2. It also changes the structural-
   pipeline lit IR (an identity arrayLayout entry now appears at 1n), so it is
   not byte-identical even though 1n behaviour is unchanged (21/21 held).*

2. **`MemoryUnitRealization` fail-closes on the now-distributed matmul kernel.**
   `canRealizeCommittedOwnerSlices` (`MemoryUnitRealization.cpp:185-206`) admits
   matmul/elementwise/stencil by classification, but the Category-A compute and
   init kernels are **classification = none** (measured). The unclassified path
   `getUnclassifiedOwnerSliceLayout` (`:102-182`) requires every external read
   to also be a write (in-place elementwise), which a matmul (reads A,B; writes
   C) violates — even though `findConsistentLoopIndexedOutputShapeWithOwnerDims`
   returns true (the output C is provably owner-local). *Relaxing the gate to
   admit owner-consistent, accumulator-free unclassified output advances past
   this — but exposes layer 3.*

3. **The ARTS boundary refuses coarse realization of a now-distributed DB.**
   `recordCoarseSuAccess` (`SdeToArtsBoundaryDepAnalysis.cpp:1104-1113`) errors
   "touches a non-coarse DB without committed SDE access windows; refusing
   coarse SU realization" because RankExpandMu authored **no access windows**
   for the init/matmul SU.

4. **RankExpandMu authors no windows because the kernel is unclassified.**
   `supportsRankExpandedAccessWindows` (`MuLayoutRewriter.cpp:51-70`) returns
   false when `queryStructuredClassification` yields none (`:58-59`).

### Root cause (single, foundational)

All four layers reduce to one fact: **the Category-A kernels are unclassified
by the v4 SDE structured-op classifier.** `queryStructuredClassification`
(`SuLoopAccessQuery.cpp:274`) returns none because `analyzeSuLoopAccesses`
(`SuLoopAccessAnalysis.cpp:1173`) bails at **`collectMemrefAccesses`**
(measured: `bail=collectMemrefAccesses ivs=3`, 10×). The bail is **not** a
`tryBuildIndexingMap` failure and **not** `float**` indirection — cgeist has
already flattened the arrays to clean `memref<128x128xf32>` with 2-D affine
indices. It is `sawAccess == false`: the kernels use a **scalar-accumulator
matmul/reduction** structure —

```
for i:                          # arg0  (owner)
  for j:                        # arg1  (owner)
    %acc = memref.alloca() : memref<f32>     # scalar temp
    for k:                      # arg2  (contraction, innermost)
      %a = load A[i,k]; %b = load B[k,j]
      %t = load %acc[]; store (%t + %a*%b), %acc[]   # rank-0, skipped
    store (beta*C[i,j] + alpha*%acc), C[i,j]         # output store at j-level
```

`collectPerfectNest` stops at the `j` level (the `alloca` breaks perfect
nesting), so the analyzer's `innermostBody` holds only the rank-0 accumulator
accesses (all skipped at `collectMemrefAccessesImpl`'s rank-0 guard) — the real
A/B reads sit one loop deeper and the real C store sits one loop shallower.
`sawAccess` stays false → no summary → no classification → the whole chain
above fails closed at 2n.

### Why 1n is unaffected

At 1 node nothing is distributed, so no kernel ever acquires a committed owner
slice; `hasPhysicalOwnerSliceLayout` is false and every gate above
short-circuits to success. The classifier gap is **2n-only**, which is why the
1n recovery (21/21) is blind to it.

### What this means for the fix

Category-A 2n is **not** an init-writer owner-rank fix and **not** a per-gate
relaxation — relaxing layers 1+2 only walks the error down to layers 3+4. The
load-bearing change is in the **classifier**: `collectPerfectNest` /
`collectMemrefAccesses` must recognise the scalar-accumulator matmul/reduction
pattern (imperfect nest with a rank-0 accumulator temp), so these kernels
classify as `matmul` / `reduction` with the real A/B reads and the real C
output store attributed to the correct loop levels. Once classified, layers
1-4 admit them by their existing classification arms — no per-gate relaxation
needed. This is foundational SDE-analysis work (its own work item), not a
scaling-lever patch, and must be validated by 2n checksum (not just compile)
because a mis-attributed accumulator would miscompile silently.

This is consistent with the standing classification of the matmul 2n scalers as
architecture-scale (see §P6 and the megalarge scaling history).

---

## Appendix N — Category-A 2n distributed-WRITE realization: the full 9-layer trace

Appendix M established that the Category-A kernels are unclassified at 2n. This
appendix records the *complete* downstream chain, mapped function-by-function by
instrumenting the live v4 passes at HEAD `aecc7cb1d` with the path-clearing
experiments (Class-A write-layout authoring + the MemoryUnitRealization
unclassified-admission relaxation) applied. Every experimental patch was
**reverted**; baseline preserved (1n 21/21, SDE lit 29/29). This is the
implementation roadmap for the foundational fix — it is *not* a single patch.

### The unifying root cause

The v4 SDE→ARTS distributed-write path assumes facts authored at `sde-planning`
survive to the ARTS boundary. They do not: **RankExpandMu deliberately strips
the committed `arrayLayout`/owner facts** (design intent — "owner dims are
`recover(structure)`", `RankExpandMu.cpp` header), rewriting bodies into
`div`/`rem` block-coordinate form. But the structure-recovery helpers that are
supposed to replace those facts are incomplete, so the fact is lost at every
layer that re-derives it. The same gap recurs for **classification, owner-dim
identity, and block geometry**.

### The chain (each layer reached only after clearing the one above)

1. **`LayoutAssignment.cpp:656-668`** — a `writerViaMu` block-parallel writer
   never authors its write-role `arrayLayout` (routed to `writerCommits` only).
   Clearing it advances past the init-writer owner-rank error.
2. **`MemoryUnitRealization.cpp:185-206`** — `canRealizeCommittedOwnerSlices`
   fail-closes on the now-distributed matmul because it is unclassified and the
   unclassified path requires in-place elementwise shapes. Relaxing it to admit
   an owner-consistent, accumulator-free output advances past it.
3. **Classification is never durable.** `queryStructuredClassification`
   (`SuLoopAccessQuery.cpp:274`) re-derives from the body every call and
   *prefers* re-derivation over the stamped attr (`stamped=-1` everywhere,
   measured). Post-RankExpandMu the body is `div`/`rem` (still affine, so
   re-derivation can succeed with a *wrong* answer) or carries
   `arts.db_access_window` markers (`MemWrite` effect → `collectMemrefAccesses`
   default-bail). Net: witnesses go unclassified/misclassified at the boundary.
4. **`recognizeExpandedBlockGridMu` false-positives a replicated array.** For
   replicated B (`memref<128x128xf32>`), the reader fact is replicated → no
   block fact → it falls back to `recoverMuPhysicalLayoutFromExpandedType`, which
   reads the *square logical* `128x128` as a 1-D owner grid (`ownerDims=[0]`,
   `blockExtents=[128]`, `gridCounts=[128]`). `findOwnerIterationExtents` then
   needs an extent of 16384 and fails → B starved of windows.
5. **`hasUnsupportedCommittedWriter`** (`MuAccessWindow.cpp:150`) returns true
   when the committed writer is unclassified (layer 3), so `queryAccessWindows`
   *skips the write-window spec* for the init writer.
6. Even with the write spec emitted (after clearing 3-5), **`readCommitted­Physical­Layout`** (`SdeToArtsBoundaryDepAnalysis.cpp:77`) returns nullopt: the
   write-window dep has `ownerDimCount=1` and `validExtents` but **no owner-dim
   identity** (`arrayOwnerDims` null, SU owner attr null, `recoverCommitted­Physical­Layout` fails — the writer's fact was stripped at layer's root).
7. **`getArrayOwnerDimsForWindow`** (`:423`) returns empty because
   `source.getArrayLayoutAttr()` no longer carries the writer fact. Adding a
   structure-recovery fallback from the window's expanded MU type still did not
   resolve the owner dims — the window's `mu` is cast back to the logical type
   before the boundary reads it, so the expanded grid is no longer visible at
   that point (layer 8, unresolved).

After all of 1-7 were cleared, **gemm 2n still failed to compile** at the same
`recordCoarseSuAccess` site — confirming at least one further unresolved layer.
Compile success is also not the finish line: 2n **checksum** correctness is a
separate gate untouched here.

### What the fix actually is

A coherent **fact-durability** design, not per-gate patches. Two viable shapes:
- **(A) Preserve committed facts through RankExpandMu** — keep the
  classification, owner dims, and block geometry as durable attrs the boundary
  reads directly (stop re-deriving from a mutated body). Aligns with the charter
  ("commit facts, consume them; do not re-derive"). The reverted experiments
  proved each individual fact can be stamped 1n-inert, but they must be
  preserved *coherently across all layers* to compose.
- **(B) Make structure-recovery total and correct** — fix
  `recoverMuPhysicalLayoutFromExpandedType` / `recognizeExpandedBlockGridMu` to
  never confuse a replicated logical array with an owner grid, to fire for write
  windows, and to read `div`/`rem` classification — and ensure the expanded type
  is still visible at every boundary read (it is cast back to logical too early).

Either path is multi-week foundational work touching `LayoutAssignment`,
`MemoryUnitRealization`, `RankExpandMu`, `SuLoopAccess{Analysis,Query}`,
`MuLayoutRewriter`, `MuAccessWindow`, and the `SdeToArtsBoundary*` passes
together, gated on 2n checksum (a mis-attributed owner dim or accumulator
miscompiles silently). It is correctly out of scope for an incremental change
and must not be landed piecemeal on `v4`.

### Addendum — the coherent fact-durability approach was also attempted, and also bottoms out

A second, principled attempt implemented the "preserve facts through RankExpandMu"
fix (approach A above) end to end: stamp `structuredClassification` durably,
make `queryStructuredClassification` prefer the stamp, fix the replicated-array
false-positive in `recognizeExpandedBlockGridMu`, and — the key missing piece —
**stamp the owner-dim identity as a durable `ownerDims` attr on every SU that
accesses an expanded MU** (collected pre-rewrite, because `rewriter.apply(mu)`
invalidates `mu.getMemref().getUsers()` — iterating it afterward segfaults).

This **cleared the coarse-boundary blocker** that had stuck the per-gate attempt:
the pure block-parallel init writer finally realized as a windowed (not coarse)
SU. gemm 2n then advanced to a *new* verifier error at the init writer
(`gemm.c:19`): **`groupBlockCount whose rank does not match the committed owner
rank`** — the init writer carries `groupBlockCount=[2,1]` (per-array-dim, rank 2)
while the recovered owner identity is `[0]` (per-owner, rank 1). The distribution
facts are stored in **mutually inconsistent formats** (per-array-dim vs
per-owner), and stamping any one of them coherently surfaces the next mismatch.

Conclusion across both attempts (~60 build cycles): Category-A 2n is not a finite
bug list. Every layer cleared exposes another fact-reconciliation gap because the
v4 pipeline lacks a single canonical, durable representation of the distribution
facts (classification, owner dims, block extents, grid counts) that all of
LayoutAssignment / RankExpandMu / MuAccessWindow / the SdeToArtsBoundary passes
agree on. The real work is that canonical-fact redesign, validated end to end on
2n **checksum** (compile success is necessary but not sufficient — a mis-stamped
owner dim miscompiles silently). All experiments were reverted; baseline
preserved (1n 21/21, SDE lit 29/29).
