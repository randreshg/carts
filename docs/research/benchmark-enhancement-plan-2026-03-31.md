# Benchmark Enhancement Plan — Consolidated Investigation (Revised)

Date: 2026-03-31
Base commit: `602dd428`
Baseline geometric mean: **1.2648x** (23 benchmarks, 64 threads)

## Revision History

- **v1**: 5-agent investigation (initial proposals)
- **v2**: 5-agent adversarial review (corrections below marked with [CORRECTED])

## Investigation Scope

5 expert agents conducted deep pipeline-level analysis of the 7 lagging
benchmarks (speedup < 1.0x) plus 1 borderline case. A second team of 5
adversarial reviewers then fact-checked every claim against compiled IR and
source code. This plan reflects all corrections.

## Implementation Constraints For Follow-Up Work

Any compiler optimization landed from this plan should follow these rules:

1. Avoid hardcoded benchmark-specific constants where a small cost model can be
   derived from existing compiler facts.
2. Keep policy computation centralized in a narrow analysis/heuristics helper,
   not spread through pass rewrites.
3. Make the chosen value explainable from observable inputs such as static trip
   count, estimated nested work, worker count, epoch/task count, frontier
   width, or DB acquire shape.
4. Keep an explicit fallback when the cost model lacks enough information so
   behavior remains predictable.
5. Add focused contract tests for the chosen policy inputs and benchmark runs
   for the resulting performance effect.

Practical implication: proposals below that currently mention literal retuning
values should be implemented as small, localized cost models with documented
inputs and bounded outputs, not as permanently baked magic numbers.

## Per-Benchmark Root Cause Summary

### 1. activations (0.65x)

**Root causes:**

| # | Severity | Issue | Detail |
|---|----------|-------|--------|
| A1 | CRITICAL | Pathological DB fragmentation (69,906 blocks of 15 elements) | [CORRECTED] Root cause is a **metadata propagation/preservation bug**: softmax's `tripCount=99` and `locationKey="activations.c:197:5"` survive onto a later fused elementwise loop where the true trip count is 1,048,576. `computeCoarsenedBlockHint` then computes `blockSize = ceil(99/7) = 15`, and the bad hint poisons ALL 7 DB allocations via `min()` aggregation in `DbPartitioning.cpp:2036`. Current artifacts suggest the fix surface is likely in loop-cloning/fusion metadata preservation (for example `ElementwisePipelinePattern`) rather than raw metadata extraction alone. |
| A2 | HIGH | 4 sequential epochs for independent outputs | Confirmed. 7 activation functions fused into 4 loop groups, each in a separate epoch. No data dependency between groups. |
| A3 | MEDIUM | `<inout>` mode drift on write-only EDTs | Confirmed. Caused by tail-block partial write from bad block size (1M mod 15 = 1). Would self-fix if A1 is fixed (1M / 65536 = 16 exactly, no tail). |
| A4 | LOW | Serial verification with per-block indirection | Confirmed. 489K db_ref operations. Would self-fix with A1. |

Treat A1 as a compiler metadata-integrity bug first and an activations tuning
issue second. Tactical containment should land quickly, but the systemic
follow-up should track `docs/research/metadata-hardening-plan-2026-03-31.md`
so metadata-driven optimizations stop depending on heuristic recovery.

### 2. stream (0.55x)

**Root causes (corrected diagnosis):**

The initial hypothesis "nested omp.wsloop inside serial loop creates 1 serial
EDT" is WRONG. The compiler CORRECTLY creates [CORRECTED] **8** (not 16)
block-partitioned task EDTs per kernel (matching `worker_threads=8` in
arts.cfg). The real bottlenecks are:

| # | Severity | Issue | Detail |
|---|----------|-------|--------|
| S1 | CRITICAL | 40 epoch create/wait cycles | 4 kernels x 10 iterations, each with epoch barrier. OMP fork/join is near-zero. Confirmed by IR. |
| S2 | HIGH | [CORRECTED] 320 EDT creations in hot loop | 4 kernels x 10 iters x **8** EDTs (not 640/16). OMP reuses thread pool with zero dispatch overhead. |
| S3 | MEDIUM | DB acquire/release per invocation | 640-960 acquire operations in the hot loop. |
| S4 | MEDIUM | Serial timing code prevents kernel overlap | `carts_bench_get_time()` forces full serialization between kernels. |

### 3. seidel-2d (0.51-0.54x)

**Root causes: [CORRECTED] — original OMP correctness claim inverted.**

| # | Severity | Issue | Detail |
|---|----------|-------|--------|
| SD0 | HIGH | [CORRECTED] **ARTS wavefront deviates from serial, not OMP** | OMP `parallel for` technically breaks Gauss-Seidel cross-row dependency, but OMP produces **bit-identical** results to serial at ALL thread counts (1-64t). ARTS wavefront produces results that diverge 0.2-0.4% from serial at 4+ threads. The standard Polybench parallelization is accepted by the HPC community. **Do NOT dismiss this benchmark.** |
| SD1 | HIGH | Max frontier width = 5 out of 64 workers | 5x9 tile grid, wavefront has 17 ranks, peak of 5 concurrent tiles. Thread utilization ~8%. |
| SD2 | HIGH | 900 EDT creations with 2700 dependency registrations | 45 tiles x 20 timesteps, each with 3 dependency calls. |
| SD3 | MEDIUM | 20 epoch barriers | 1 per timestep, each blocking the main thread. |

### 4. atax (0.61x), bicg (0.63x), correlation (0.61x)

**Root causes: confirmed with corrected improvement estimates.**

| # | Severity | Issue | Detail |
|---|----------|-------|--------|
| B1 | HIGH | Serial inner reductions confirmed | Each EDT runs a serial O(N) reduction (`parallelClassification=3`, `hasInterIterationDeps=true`). Confirmed in compiled IR. [CORRECTED] However, these are **BLAS-2 memory-bandwidth-bound** operations. Parallelizing the reduction gives **marginal (1.0-1.2x)** improvement, not "2-4x" as originally claimed. |
| B2 | HIGH | bicg: fusion eliminates pipeline parallelism | Confirmed. Both `q=A*p` and `s=A^T*r` in single EDT body, single epoch. |
| B3 | MEDIUM | Full-range coarse matrix reads | Confirmed. Partitioning decisions are correct (H1.C2b). |
| B4 | MEDIUM | ET-5 doesn't fire for array reductions | [NEW] ET-5 `selectReductionStrategies()` requires **single-element coarse-grained DBs** (`hasSingleSize` + `isCoarseGrained`). It misses the per-row dot product reductions in block-partitioned arrays. Significant gap in reduction detection logic. |

### 5. jacobi2d (0.56x)

**Root causes: confirmed with corrected impact estimates.**

| # | Severity | Issue | Detail |
|---|----------|-------|--------|
| J1 | MEDIUM | [CORRECTED] 1000 epoch barriers — overhead is **~1-2%**, not 15-25% | Per-epoch gap = ~35-50us (main thread wakeup + 31 EDT creates + dispatch). Over 1000 epochs: 35-50ms out of 5333ms total = **0.7-1.0%**. CPS chain impact is marginal, not transformative. |
| J2 | HIGH | 31 blocks for 64 workers | Confirmed. `kPreferredMinCellsPerStencilTask = 2M` produces exactly 31 blocks (math verified: `minOwnedByWork = ceil(2097152/8190) = 257`, `maxWorkers = 8190/257 = 31`). |
| J3 | MEDIUM | Double-indirected `float **` | Per-row malloc creates non-contiguous memory. Source-level issue. |

### 6. convolution-2d (0.79x)

**Root causes: [CORRECTED] — original diagnosis was WRONG.**

| # | Severity | Issue | Detail |
|---|----------|-------|--------|
| CV1 | N/A | [CORRECTED] "110 epoch barriers" claim is **FALSE** | Compiled IR shows **only 1 epoch**. The compiler already hoists the NREPS=110 loop inside each worker EDT. There is NO epoch amortization opportunity. |
| CV2 | UNKNOWN | Actual gap source unidentified | The 0.79x gap needs fresh investigation. Possible causes: per-row malloc overhead, DB acquire per stencil halo, or block size mismatch. |

---

## Consolidated Enhancement Proposals (Revised Priority)

### Tier 1: High Impact, Low Risk — Verified

| ID | Proposal | Benchmarks | Expected Gain | Risk | Files |
|----|----------|-----------|---------------|------|-------|
| P1 | **Fix metadata propagation bug** (softmax tripCount=99 on fused activation loop) | activations | 2-5x | LOW | Likely `ElementwisePipelinePattern` / loop-cloning metadata preservation; verify at `DistributionHeuristics.cpp:307` |
| P2 | Replace the fixed intranode stencil task floor with a small cost model for owned-strip size | jacobi2d | 10-15% | MEDIUM (may regress sw4lite/specfem3d if the model is poorly bounded) | `DistributionHeuristics.cpp:321` |
| P4 | Epoch merging for independent output DBs, but extend fusion policy to reason about continuation-eligible tails and redundant acquire sets | activations | 1.3-1.5x | LOW-MEDIUM | `EpochOpt.cpp` |

### Tier 2: Medium Impact, Medium Risk — Verified

| ID | Proposal | Benchmarks | Expected Gain | Risk | Files |
|----|----------|-----------|---------------|------|-------|
| P6 | **Selective** anti-fusion for independent parallel loops with reductions, driven by a small profitability model rather than a blanket rule | bicg | ~1.3x | MEDIUM (must NOT be global — would regress 2mm/3mm) | `EdtStructuralOpt.cpp` (add reduction-aware fusability/profitability check) |
| P5 | Investigate ARTS wavefront numerical accuracy for seidel-2d | seidel-2d | N/A (diagnostic) | LOW | `Seidel2DWavefrontPattern.cpp` tile boundary analysis |
| P10r | **Loop hoisting** for STREAM (revised from "persistent workers"), selected by a simple epoch/task-overhead cost model | stream | 1.1-1.5x | MEDIUM | `PatternPipeline` or new pre-ForLowering pass (200-400 LOC) |

### Tier 3: Low Impact or High Risk — Revised Down

| ID | Proposal | Benchmarks | Expected Gain | Risk | Files |
|----|----------|-----------|---------------|------|-------|
| P3 | Keep CPS chain / finish-continuation enabled by default; treat it as baseline behavior, not a per-benchmark experiment | jacobi2d | [CORRECTED] **1-2%** (not 15-25%) | LOW (self-selecting) | `Compile.cpp:199` default + targeted fallback tests |
| P8 | Tighten `<inout>` back to `<out>` for tail-block | activations | Self-fixes with P1 | LOW | `DbModeTightening.cpp` |
| P11r | Seidel tile tuning through a bounded wavefront cost model (revised from "rank coalescing") | seidel-2d | Uncertain | MEDIUM | `DistributionHeuristics.cpp` (tile params only; cross-rank merging is semantically invalid) |
| P13 | NUMA-aware coarse allocation | atax, bicg | 1.2-1.5x | MEDIUM | ARTS runtime |
| P14 | Temporal blocking for Jacobi | jacobi2d | 10-20% | HIGH | New pass |

### Removed / Invalidated

| ID | Proposal | Reason |
|----|----------|--------|
| P7 | Parallel inner-loop reduction (ET-5 lowering) | **Infeasible near-term** (500-1500 LOC). Improvement estimate **unrealistic** for BLAS-2 bandwidth-bound ops (~1.0-1.2x, not 2-4x). |
| P9 | Repetition amortization for conv2d | **Invalid** — conv2d already has only 1 epoch. The 110-rep loop is already inside worker EDTs. |
| P10 | Persistent workers for STREAM | **Infeasible** — no ARTS runtime support for persistent EDTs. Replaced by P10r (loop hoisting). |
| P11 | Wavefront rank coalescing | **Semantically invalid** — cross-rank merging breaks Seidel dependencies. Replaced by P11r (tile tuning). |
| P12 | 2D block partitioning for correlation | Complex (300+ LOC across 3 files), unclear benefit given BLAS-2 bandwidth bound. |
| P15 | Batch DB acquire | Runtime change, long-term. |

---

## Projected Impact (Conservative Estimates)

| Benchmark | Current | After Tier 1 | After Tier 1+2 | Notes |
|-----------|---------|-------------|----------------|-------|
| activations | 0.65x | **0.85-1.0x** (P1+P4) | 0.85-1.0x | P1 is a bug fix with large impact; P4 likely needs stronger fusion policy than the current consecutive-epoch check |
| jacobi2d | 0.56x | **0.60-0.65x** (P2) | 0.60-0.65x | P2 should be implemented as a strip-size cost model, not a new baked constant |
| conv2d | 0.79x | 0.79x (no Tier 1 fix) | 0.79x | [CORRECTED] No known compiler fix — needs fresh investigation |
| stream | 0.55x | 0.55x (no Tier 1 fix) | **0.60-0.80x** (P10r) | Loop hoisting is the realistic path |
| atax | 0.61x | 0.61x (no Tier 1 fix) | 0.61x | Bandwidth-bound; no near-term compiler fix |
| bicg | 0.63x | 0.63x (no Tier 1 fix) | **0.70-0.80x** (P6) | Anti-fusion enables pipeline overlap |
| correlation | 0.61x | 0.61x (no Tier 1 fix) | 0.61x | Bandwidth-bound; needs ET-5 long-term |
| seidel-2d | 0.51x | 0.51x (no Tier 1 fix) | 0.51x | Needs wavefront overhead reduction; ARTS also has accuracy gap |

Geometric mean improvement estimate: **1.26x -> ~1.30-1.35x** after Tier 1+2.
(Original estimate of 1.45-1.65x was unrealistic.)

---

## Cross-Cutting Themes (Revised)

### Theme 1: Metadata propagation bugs can cause catastrophic performance

The activations 15-element block size is caused by a single stale
`tripCount=99` metadata attribute from an unrelated loop. This is likely not
the only instance — a systematic audit of `arts.loop` metadata fidelity across
all benchmarks is warranted.

### Theme 2: Epoch overhead is NOT the dominant cost (revised)

[CORRECTED] For jacobi2d, epoch overhead is only ~1% of total runtime, not
15-25% as originally claimed. For STREAM, the 40-epoch overhead is more
significant because the kernels themselves are very fast (~2.6ms per iteration
for OMP). Epoch overhead matters most when per-epoch compute is small.

Finish-continuation / CPS scheduling should therefore be treated as baseline
latency cleanup that stays enabled by default, not as a headline benchmark
improvement lever by itself.

### Theme 3: BLAS-2 kernels are bandwidth-bound — parallelization alone won't fix them

[CORRECTED] atax, bicg, and correlation are memory-bandwidth limited. Even
with perfect parallel reductions, speedup is bounded by DRAM bandwidth. The
serial inner reduction is a theoretical bottleneck but NOT the practical
performance limiter. These benchmarks may be structurally disadvantaged for a
task-based runtime vs. a thread-pool runtime like OpenMP.

### Theme 4: EDT fusion needs reduction awareness

The bicg fusion (merging two independent parallel loops into one EDT) is the
one BLAS case where a compiler fix can help. The fusion eliminates pipeline
parallelism. A selective anti-fusion rule (when fused loops have independent
reductions) would help bicg without regressing 2mm/3mm which benefit from
fusion.

### Theme 5: Seidel wavefront has an accuracy problem

[NEW] ARTS wavefront tiling for seidel-2d produces results that deviate
0.2-0.4% from the serial answer at 4+ threads, while OMP produces bit-identical
results to serial at all thread counts. The wavefront tile boundaries appear to
introduce numerical differences. This needs investigation independently of
performance.

### Theme 6: Convolution-2d gap is unexplained

[CORRECTED] The original diagnosis (110 epoch barriers) was wrong — conv2d has
only 1 epoch. The 0.79x gap needs fresh root cause analysis. Candidates:
per-row `double**` malloc, stencil halo acquire overhead, or block size effects.

### Theme 7: Future tuning work should be cost-model driven

The remaining opportunities are mostly not correctness bugs; they are policy
problems. Hardcoding values like stencil task floors, wavefront tile budgets,
or anti-fusion exceptions will make the pipeline brittle and benchmark-shaped.
The right follow-up is to keep small policy models close to
`DistributionHeuristics`, `EpochOpt`, and `EdtStructuralOpt`, with:

- compact inputs already available from analysis
- bounded outputs
- explicit safety fallbacks
- contract tests that pin the decision surface
- benchmark validation that proves the model generalizes beyond one kernel

---

## Recommended Implementation Order (Revised)

**Phase 1 (Bug fixes — immediate):**
1. P1 — Fix metadata propagation bug in activations (highest ROI fix in the plan)
2. Audit `arts.loop` metadata fidelity across loop-cloning/fusion transforms, then across other benchmarks

**Phase 2 (Policy tuning — with regression testing):**
3. P2 — Replace the fixed stencil task floor with a bounded intranode strip-size cost model (test against sw4lite, specfem3d first)
4. P4 — Strengthen epoch fusion for independent DB groups, accounting for continuation-enabled lowering

**Phase 3 (Structural — medium effort):**
5. P6 — Selective anti-fusion for bicg-style patterns using a profitability model
6. P10r — Loop hoisting for STREAM using an epoch/task-overhead model
7. P5 — Investigate ARTS wavefront accuracy for seidel-2d

**Phase 4 (Low priority / long-term):**
8. P3 — Keep continuation/CPS on by default and use explicit opt-out only for debugging or test isolation
9. P11r — Seidel tile tuning via a bounded wavefront model (if P5 resolves accuracy issue first)
10. Fresh investigation of conv2d gap

**Rerun full 64-thread sweep after each phase.**

---

## Flaws Found in v1 Plan (Summary)

| v1 Claim | v2 Correction | Impact |
|----------|--------------|--------|
| Activations: "block size from wrong dimension" | Metadata propagation bug (softmax tripCount on tanh loop) | Fix is more targeted |
| CPS chain: "15-25% improvement" | Actual: 1-2% (epoch overhead is ~35-50ms out of 5333ms) | P3 demoted to Tier 3 |
| STREAM: "16 EDTs per kernel" | Actual: 8 EDTs (worker_threads=8) | Minor correction |
| STREAM: "1.5-2.5x from persistent workers" | Infeasible; loop hoisting gives 1.1-1.5x | P10 replaced by P10r |
| Conv2d: "110 epoch barriers" | Actual: 1 epoch (compiler already hoists rep loop) | P9 invalidated |
| Seidel: "OMP computes incorrect GS" | OMP matches serial exactly; ARTS deviates 0.2-0.4% | P5 reframed as ARTS accuracy investigation |
| BLAS: "2-4x from parallel reductions" | Unrealistic for bandwidth-bound BLAS-2 (~1.0-1.2x) | P7 removed (infeasible + marginal) |
| Wavefront rank coalescing | Semantically invalid across ranks | P11 replaced by P11r (tile tuning) |
| Anti-fusion (P6) | Global disable would regress 2mm/3mm | Must be selective |
| Geo mean: "1.45-1.65x" | Realistic: ~1.30-1.35x | Expectations tempered |
