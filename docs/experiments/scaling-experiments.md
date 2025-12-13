# CARTS scaling experiments: strong, weak, and distributed crossover

This document summarizes what the current CARTS infrastructure enables today, what it makes hard, and a concrete experimental plan to measure:

- Strong scaling (fixed problem, more cores)
- Weak scaling (more cores, proportionally larger problem)
- The “shared-memory wall”: where OpenMP (single-node shared memory) stops being viable and distributed memory becomes necessary
- Cases where CARTS can show clear potential (even if single-node OpenMP is faster)

## What the current infrastructure already gives you

### Benchmark build/run plumbing

- **One repo, two variants** (apples-to-apples):
  - **OpenMP baseline**: builds with `carts clang ... -fopenmp -O3` into `*_omp`
  - **CARTS/ARTS**: compiles through the CARTS pipeline into `*_arts` and uses an `arts.cfg` runtime config
  - Implemented in the shared benchmark Makefile fragment: `external/carts-benchmarks/common/carts.mk`
- **Unified benchmark CLI**: `carts benchmarks ...` shells out to `external/carts-benchmarks/benchmark_runner.py` and supports:
  - `list`, `build`, `run`, `clean`, `report`
  - JSON export via `carts benchmarks run ... -o results.json`
- **Standardized output parsing**:
  - `kernel.<name>: <sec>s`, `checksum: <value>` (from `include/arts/Utils/Benchmarks/CartsBenchmarks.h`)
  - Optional per-worker timing (`parallel.*`, `task.*`) when `CARTS_ENABLE_TIMING` is enabled

### Runtime knobs (ARTS)

ARTS is already a distributed runtime; `arts.cfg` supports (among others):

- `threads=<n>`: workers per node
- `nodeCount=<k>`, `nodes=<list>`, `masterNode=<host>`
- `launcher=ssh|slurm|lsf`
- `pinStride`, topology printing, counters/introspection folders

See `external/arts/sampleConfigs/arts.cfg` for a fuller template.

## Advantages, limitations, opportunities

### Advantages

- **End-to-end pipeline exists**: OpenMP → MLIR → CARTS passes → LLVM IR → executable (`README.md`).
- **Built-in A/B comparison**: every benchmark can build OpenMP and CARTS variants under one Makefile/CLI.
- **Verification hooks**: checksum extraction/verification is a first-class concept in the runner.
- **Multiple “views” of the program**: sequential MLIR, OpenMP MLIR, metadata, concurrency logs (`common/carts.mk` targets).
- **Distributed-memory path is real**: ARTS supports multi-node execution and explicit data movement via datablocks.

### Limitations (important for scaling studies)

- **`carts benchmarks run` is single-node oriented**:
  - No built-in orchestration for multi-node job launch (Slurm/SSH) in the CLI itself
  - No built-in sweep over threads/nodes; it runs one configuration per invocation
- **Problem-size control is inconsistent across benchmarks**:
  - Some benchmarks use PolyBench-style dataset macros, others use direct `NI/NJ/NK` macros, and some may take runtime args
  - The benchmark runner does not currently expose a generic `--cflags/--extra-cflags` or `--run-args` pass-through to automate size sweeps
- **Reproducibility controls are not centralized**:
  - Thread pinning/NUMA policy, turbo/boost state, and affinity env-vars are not enforced by the harness
- **Two benchmarking paths exist**:
  - `external/carts-benchmarks/benchmark_runner.py` is active and integrated with `carts benchmarks`
  - `tools/benchmark/carts-benchmark.py` appears to assume an `examples/` layout that does not exist in this repo (today it won’t find `examples/parallel`)

### Opportunities (high leverage for your experiment campaign)

- Extend `carts benchmarks` to accept:
  - `--env KEY=VAL` (e.g., `OMP_NUM_THREADS`, affinity vars)
  - `--cflags/--extra-cflags` (for compile-time sizes like `-DNI=...`)
  - `--args` (for benchmarks that take runtime sizes/iterations)
  - `sweep` subcommand to generate a full scaling matrix and emit CSV/JSON
- Add **standard size targets** across suites (mini/small/medium/large/xlarge) and keep READMEs consistent with actual macros.
- Add a **cluster mode** (`launcher=slurm`) wrapper that writes a job script, runs, and harvests results automatically.
- Build a **roofline-oriented report** (effective bandwidth + arithmetic intensity) for “why scaling stops”.

## Experimental design: what we measure and why

### Core metrics

For each benchmark and configuration (variant × problem size × resources), record:

- `T(p)`: wall-clock time (use `kernel.*` timers where available)
- **Strong scaling**:
  - speedup `S(p) = T(1) / T(p)`
  - efficiency `E(p) = S(p) / p`
- **Weak scaling**:
  - efficiency `Ew(p) = T(p0, N0) / T(p, N(p))` (keep work per core constant)
- **Overheads (optional but recommended)**:
  - from `CARTS_ENABLE_TIMING`: `T_parallel`, `T_task`, and `T_overhead = T_parallel − T_task`
  - ARTS counters (if enabled): task counts, DB acquire/release events, comm volume proxies
- **“Shared-memory wall” indicators**:
  - capacity pressure: memory footprint vs node RAM, paging/oom
  - bandwidth saturation: scaling flattens, cache-miss rate grows, effective BW hits a ceiling

### Reproducibility controls (baseline expectations)

For every run series, record:

- machine info (`lscpu`, RAM), OS/kernel, compiler versions
- `OMP_NUM_THREADS`, `OMP_PROC_BIND`, `OMP_PLACES`
- ARTS config used (`arts.cfg` content) and CARTS git SHA
- number of repetitions, warmup policy, and statistic reported (median recommended)

## The scaling experiment matrix

### Benchmark set (minimal but representative)

Pick at least one from each category:

- **Compute-bound**: `polybench/gemm`, `polybench/2mm`, `polybench/3mm`
- **Memory-bandwidth bound (stencils)**: `polybench/jacobi2d`, `polybench/convolution-2d`, `polybench/convolution-3d`, `polybench/fdtd-2d`
- **Dependency/task-graph**: `kastors-jacobi/*task*` (or any OpenMP `depend` benchmark)
- **“Real app” proxy**: one of `sw4lite/*`, `specfem3d/*`, `seissol/*` (whichever is easiest to size/drive)

This mix increases the chance of finding “CARTS potential” cases: irregularity, dependencies, or communication-heavy kernels.

### Strong scaling (single node)

For each benchmark:

1. Choose a **fixed problem size** that yields `T(1) ≥ 2–5s` (to reduce noise).
2. Sweep threads: `p ∈ {1, 2, 4, 8, …, Pmax}` where `Pmax` includes:
   - physical cores
   - optionally SMT (e.g., 2× cores) to show oversubscription behavior
3. Run both variants:
   - OpenMP: set `OMP_NUM_THREADS=p`
   - CARTS/ARTS: set `threads=p` in `arts.cfg`
4. Compute `S(p)` and `E(p)` for both.

### Weak scaling (single node)

Define a rule for how the problem grows with `p` so **work/core stays ~constant**.

Common rules (choose the one that matches the kernel’s asymptotic work):

- 2D stencil / 2D conv (`O(N^2)` work): `N(p) = N0 * sqrt(p)`
- 3D stencil (`O(N^3)` work): `N(p) = N0 * cbrt(p)`
- GEMM (`O(N^3)` work): `N(p) = N0 * cbrt(p)` (if matrices are roughly cubic)

Then for each `p`:

- Build/run at `N(p)` (compile-time macro override or runtime arg, depending on benchmark).
- Report weak-scaling efficiency `Ew(p)` and the slope of runtime vs `p`.

## When OpenMP “doesn’t work”: defining the crossover point

OpenMP doesn’t have a single failure point; it hits **two distinct walls**:

1. **Capacity wall (hard stop)**: the problem does not fit in a single node’s memory (OOM/paging makes it unusable).
2. **Bandwidth / synchronization wall (soft stop)**: it fits, but adding threads doesn’t help (efficiency collapses).

Define two practical criteria to decide “we need distributed memory now”:

- **Capacity criterion**: estimated footprint ≥ 60–70% of node RAM (or any observed paging/OOM).
- **Efficiency criterion**: for the fixed-size strong-scaling curve, `E(p) < 0.5` for all `p ≥ p*` (pick the earliest sustained drop).

Once either is met, move to distributed-memory experiments, because additional nodes buy:

- more total RAM (capacity)
- more aggregate memory bandwidth (bandwidth-bound kernels)

## Distributed-memory experiments (ARTS / CARTS only)

### Distributed strong scaling

Fixed global problem size `N`, increase nodes:

- Keep `threads` per node fixed (e.g., one per core) and vary `nodeCount ∈ {1,2,4,8,…}`
- Measure `S_nodes(k) = T(1 node) / T(k nodes)` and efficiency

### Distributed weak scaling

Keep work per node constant:

- Choose per-node baseline size `Nnode0`, set `Nglobal(k)` with the same scaling rules as weak scaling, but with `p = k * threads_per_node`
- Measure how runtime changes as you add nodes

### Practical ARTS config knobs to exercise

Start with `external/arts/sampleConfigs/arts.cfg` and vary:

- `launcher=slurm` (preferred on clusters) or `launcher=ssh` with `nodes=...`
- `nodeCount`, `threads`
- `pinStride` (for binding experiments)
- `protocol` / `netInterface` (if available in your build)

## Finding “CARTS potential”: experiments beyond raw speed

Single-node OpenMP may often win on mature kernels. CARTS can still be compelling if it:

- extends scaling beyond one node
- handles task dependencies/irregularity with less global synchronization
- overlaps communication and computation (distributed DAG execution)
- reduces data movement (diff/twin updates) for structured updates

Design experiments that stress those strengths:

- **Dependency-heavy wavefronts**: task graphs with `depend` (diagonals, pipelines).
- **Load-imbalance injection**: add irregular per-iteration work and compare OpenMP schedules vs CARTS tasking.
- **Granularity sweep**: vary chunk/tile sizes to find where task overhead dominates vs where locality improves.
- **NUMA sensitivity**: multi-socket runs with binding and memory policies.
- **Communication sensitivity (distributed)**: halo widths / update frequency (stencils) to see when ARTS overlap helps.

## ARTS introspection: turning scaling into actionable compiler feedback

ARTS has a counter/introspection subsystem that can attribute runtime costs and behaviors back to **compiler-assigned `arts_id`** values. This is the fastest way to answer “what overhead are we paying?” and “is the generated code quality improving?” with hard evidence.

### What ARTS can already measure (today)

The runtime already has counter hooks for:

- **EDT lifecycle**: `edtCreateCounter`, `signalEdtCounter`, `edtRunningTime`, `numEdtsCreated`, `numEdtsAcquired`, `numEdtsFinished`
- **Events**: `eventCreateCounter`, `persistentEventCreateCounter`, `signalEventCounter`
- **DB lifecycle & data movement**: `dbCreateCounter`, `getDbCounter`, `putDbCounter`, `remoteMemoryMove`, `remoteBytesSent`, `remoteBytesReceived`
- **Memory pressure**: `mallocMemory`, `callocMemory`, `freeMemory`, `memoryFootprint`
- **Compiler-hint effectiveness**:
  - Acquire-mode overrides: `acquireReadMode`, `acquireWriteMode`, `ownerUpdatesSaved`, `ownerUpdatesPerformed`
  - Twin/diff protocol: `twinDiffUsed`, `twinDiffSkipped`, `twinDiffBytesSaved`, `twinDiffComputeTime`
- **Per-`arts_id` aggregates** (when the compiler tags work/DBs):
  - EDTs: invocations + `total_exec_ns` (`artsIdEdtMetrics`)
  - DBs: invocations + `bytes_local/bytes_remote/cache_misses` (`artsIdDbMetrics`, currently mostly useful for allocation/size unless extended)

Implementation pointers:

- Counter types and `arts_id` structures: `external/arts/core/inc/arts/introspection/Counter.h`, `external/arts/core/inc/arts/introspection/ArtsIdCounter.h`
- Where EDT exec time is recorded per `arts_id`: `external/arts/core/src/runtime/Runtime.c`
- Where DB `arts_id` is recorded on create: `external/arts/core/src/runtime/memory/DbFunctions.c`
- Where compiler hints are applied and counted (acquire-mode, twin-diff): `external/arts/core/src/runtime/memory/DbFunctions.c`
- Counter configuration file: `external/arts/counter.cfg`

### What CARTS already provides to join with runtime data

Most benchmarks produce compiler metadata alongside the build artifacts:

- `build/<bench>.carts-metadata.json` includes `arts_id` → `location_key` mappings (e.g., `file.c:line:col`) and loop/memref characteristics.

This enables a closed-loop workflow:

1. Run the benchmark with counters enabled to emit `counterFolder/n<node>_t<thread>.json`.
2. Join those JSON files with `build/<bench>.carts-metadata.json` by `arts_id`.
3. Rank “expensive” or “suspicious” `arts_id`s and feed fixes back into the compiler passes (partitioning, fusion, hinting).

### Key “code quality” metrics to track over time

These are designed to be stable, comparable across commits, and directly actionable:

- **`arts_id` coverage**: fraction of runtime work that is attributable.
  - `coverage = (sum invocations in artsIdEdtMetrics) / numEdtsFinished`
  - A low value means CARTS is generating EDTs with `arts_id=0` (uninstrumented), limiting diagnosis.
- **Average overhead per useful task** (node-level):
  - `avg_create_ns = edtCreateCounter / numEdtsCreated`
  - `avg_exec_ns = edtRunningTime / numEdtsFinished`
  - Track `avg_create_ns / avg_exec_ns` as a “granularity sanity check”.
- **Hint effectiveness**:
  - `owner_update_avoid_rate = ownerUpdatesSaved / (ownerUpdatesSaved + ownerUpdatesPerformed)`
  - `twin_diff_roi = twinDiffBytesSaved / twinDiffComputeTime` (note: compute time is recorded in µs in current code)
- **DB pressure**:
  - `dbs_per_edt = numDbsCreated / numEdtsCreated`
  - `bytes_sent_per_work = remoteBytesSent / (sum total_exec_ns across artsIdEdtMetrics)`

### Experiments that directly leverage introspection

- **Overhead budget decomposition** (single node, then multi-node):
  - Enable the lifecycle counters above and show how their totals scale with threads/nodes and problem size.
- **Granularity sweep with attribution**:
  - Vary chunk/tile size in CARTS (or in the benchmark) and observe when `avg_create_ns` starts dominating `avg_exec_ns`.
- **Acquire-mode/twin-diff A/B**:
  - Compare runs with compiler hints enabled vs disabled (or forced modes) and use `ownerUpdatesSaved` + twin-diff counters to explain speedups/slowdowns.
- **Where distributed memory becomes necessary** (root-cause view):
  - When scaling collapses, use `memoryFootprint`, DB counts, and `remoteBytes*` to separate “capacity wall”, “bandwidth wall”, and “runtime overhead wall”.

Practical note: counter collection uses a capture thread when any counter is in `THREAD` or `NODE` mode; keep profiling runs separate from “headline performance” runs.

Practical note (current behavior): keep `artsIdEdtMetrics` / `artsIdDbMetrics` in `THREAD` mode and reduce offline; the in-runtime “NODE reduction” path is updated during periodic capture and can overcount because it merges cumulative per-thread tables repeatedly.

### Recommended counter profiles (drop-in)

ARTS’ counter configuration is compile-time generated from a config file. This repo includes a few ready-to-use profiles:

- Minimal `arts_id` attribution: `external/arts/counter.profile-artsid-only.cfg`
- Full overhead budget + hint effectiveness: `external/arts/counter.profile-overhead.cfg`
- Per-invocation trace (heavy): `external/arts/counter.profile-deep.cfg`

To build ARTS with a profile:

- Configure ARTS with `-DCOUNTER_CONFIG_PATH=...` (the ARTS `CMakeLists.txt` now exposes this as a cache variable).

To rank `arts_id` hotspots (join counters ↔ CARTS metadata):

- Use `carts report` (reads `counterFolder/n*_t*.json` + `build/<bench>.carts-metadata.json`):
  - `carts report --counters ./counters --metadata ./build/<bench>.carts-metadata.json --stride 1000 --top 50`

### Where `arts_id` is assigned (CARTS → ARTS linkage)

The linkage is two-step: CARTS assigns IDs into MLIR attributes, then the LLVM lowering selects the corresponding ARTS runtime APIs.

- **ID registry and metadata**: `IdRegistry` creates deterministic `arts.id` values from locations (`include/arts/Utils/Metadata/IdRegistry.h`, `lib/arts/Utils/Metadata/IdRegistry.cpp`). `CollectMetadata` exports loop/memref metadata (and their `arts_id`) to `build/<bench>.carts-metadata.json` (`lib/arts/Passes/CollectMetadata.cpp`).
- **EDTs**:
  - `EdtLowering` assigns `arts.create_id` (derived from `arts.id * idStride`) to the lowered `arts.edt_create` (`lib/arts/Passes/EdtLowering.cpp`).
  - `ConvertArtsToLLVM` detects `arts.create_id` and calls `artsEdtCreateWithArtsId` / `artsEdtCreateWithEpochArtsId` (`lib/arts/Passes/ConvertArtsToLLVM.cpp`).
- **DBs**:
  - `DbLowering` ensures DB allocs are heap-based and assigns `arts.create_id` while preserving `arts.id` on the alloc (`lib/arts/Passes/DbLowering.cpp`).
  - `ConvertArtsToLLVM` uses `arts.id` from `arts.db_alloc` as the base and calls `artsDbCreateWithGuidAndArtsId` for each created DB (`lib/arts/Passes/ConvertArtsToLLVM.cpp`).

Interpretation tip: EDT `arts_id` values in runtime output are often `base_id * idStride (+ offset)`. If `idStride=1000`, you can map an EDT runtime id back to the “base” metadata id with integer division.

## Recommended "first campaign" (minimal, high signal)

1. **Single-node strong scaling** on:
   - one compute-bound: **`polybench/gemm`** (0.99x speedup - best ARTS vs OMP parity)
   - one bandwidth-bound: **`polybench/jacobi2d`** (classic stencil)
   - one task-dependency: **`kastors-jacobi/jacobi-task-dep`**
2. **Single-node weak scaling** on the stencil (2D or 3D).
3. **Crossover sweep** on the stencil:
   - increase `N` until you see (a) RAM pressure and/or (b) efficiency collapse
4. **Distributed weak scaling** on the same stencil (CARTS only), showing scaling continues past OpenMP's wall.

### Quick benchmark results (small size, 2025-12-13)

| Benchmark | ARTS Kernel | OMP Kernel | Speedup | Category |
|-----------|-------------|------------|---------|----------|
| `polybench/gemm` | 0.013s | 0.013s | **0.99x** | Compute-bound |
| `polybench/correlation` | 0.002s | 0.003s | **1.09x** | Compute-bound |
| `ml-kernels/pooling` | 0.0003s | 0.0003s | **1.07x** | ML kernel |
| `polybench/jacobi2d` | 0.323s | 0.006s | 0.02x | Bandwidth-bound |
| `kastors-jacobi/jacobi-task-dep` | 0.140s | 0.010s | 0.08x | Task-dependency |

### Recommended test configuration for scaling studies

For meaningful scaling studies, use 2048x2048x2048 matrices (T(1) ≈ 16s):

```bash
cd external/carts-benchmarks/polybench/gemm
make clean && make all openmp CFLAGS="-DNI=2048 -DNJ=2048 -DNK=2048"

# Modify arts.cfg threads for each run:
sed -i '' "s/threads=.*/threads=N/" arts.cfg
./gemm_arts

# For OMP:
OMP_NUM_THREADS=N ./build/gemm_omp
```

### Strong scaling results (2025-12-13, MacBook Pro M3, 12 perf cores)

| Threads | ARTS (s) | OMP (s) | ARTS S(p) | OMP S(p) | ARTS E(p) | OMP E(p) |
|---------|----------|---------|-----------|----------|-----------|----------|
| 1       | 16.14    | 14.65   | 1.00x     | 1.00x    | 100.0%    | 100.0%   |
| 2       | 7.16     | 7.47    | **2.25x** | 1.96x    | **112.5%**| 98.0%    |
| 4       | 7.22     | 6.47    | 2.23x     | 2.26x    | 55.7%     | 56.5%    |
| 8       | 3.56     | 2.80    | 4.53x     | 5.22x    | 56.6%     | 65.2%    |
| 12      | 2.56     | 1.88    | 6.29x     | 7.80x    | 52.4%     | 65.0%    |
| 16      | 2.34     | 1.98    | 6.88x     | 7.41x    | 43.0%     | 46.3%    |

**Key observations:**
- **ARTS shows super-linear speedup at 2 threads** (112.5% efficiency) - likely cache locality
- **Memory bandwidth saturation at 4+ threads** - efficiency drops to ~56%
- **OMP scales better at high thread counts** due to lower runtime overhead
- **ARTS has ~10% single-thread overhead** vs OMP baseline

**Start with `polybench/gemm`** - it shows good scaling behavior and is ideal for validating the infrastructure.

## Command patterns (what to run today)

### OpenMP vs CARTS on a single node

- OpenMP threads: set `OMP_NUM_THREADS=<p>` when running `*_omp`.
- CARTS/ARTS threads: set `threads=<p>` in the `arts.cfg` you build/run with.

Example workflow (per-benchmark directory):

1. Build both variants (size target if supported):
   - `make clean && make large`
2. Run OpenMP:
   - `OMP_NUM_THREADS=16 ./build/<bench>_omp`
3. Run CARTS/ARTS:
   - `./build/<bench>_arts`

### Using the unified benchmark runner

```bash
# List all available benchmarks
carts benchmarks list

# Run gemm (recommended starting point - 0.99x ARTS vs OMP parity)
carts benchmarks run polybench/gemm --size large -o gemm_results.json

# Run all polybench benchmarks
carts benchmarks run polybench/* --size small -o polybench_results.json

# Run with custom arts.cfg
carts benchmarks run polybench/gemm --size large \
  --arts-config ./external/carts-benchmarks/polybench/gemm/arts.cfg \
  -o results.json
```

Note: today you'll still need to set `OMP_NUM_THREADS` and manage `arts.cfg` variants externally for sweeps.
