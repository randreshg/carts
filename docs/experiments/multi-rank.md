# CARTS Multi-Rank Experiment Runbook

## Overview

This document contains the actual commands to execute for understanding CARTS behavior in **multi-rank** mode (distributed memory; `nodeCount > 1`). Run these in order.

Terminology used here:
- **rank**: one ARTS process (often “one per node” on a cluster, but you can run many ranks on one host for local testing)
- **nodes**: the number of ranks (`carts benchmarks run --nodes ...`)
- **threads**: worker threads per rank (`carts benchmarks run --threads ...`)
- **total cores** (approx): `node_count * threads`

This runbook is designed to help you measure:

- **Strong scaling**: fixed global problem size, more total cores (more ranks/nodes)
- **Weak scaling**: more total cores, proportionally larger global problem
- The **shared-memory wall**: where single-node OpenMP stops scaling and multi-rank becomes necessary
- Where CARTS can show **clear potential** even if single-node OpenMP is faster (e.g., scaling past one node, comm/compute overlap, dependency-heavy DAGs)

Related tuning notes:
- `docs/heuristics/multi_rank/db_granularity_and_ownership.md` (DB granularity + ownership)

---

## Prerequisites

```bash
# Ensure CARTS is built
carts build --all

# Verify benchmarks are available
carts benchmarks list

# Create output directories
mkdir -p results/multi_rank results/multi_rank/counters results/multi_rank/configs
```

---

## ARTS launch modes (pick one)

### Option A: Local multi-rank on one machine (SSH launcher)

This runs multiple ARTS ranks on the same host via SSH to `localhost`.

1) Ensure SSH works without prompts:

```bash
ssh localhost true
```

2) Create a “local multi-rank” config with enough `localhost` entries for your largest intended `--nodes`.

```bash
cat > results/multi_rank/configs/arts_local_8r.cfg <<'EOF'
[ARTS]
launcher=ssh
masterNode=localhost

# IMPORTANT: list at least as many hosts as the largest nodeCount you plan to test.
# (repeats are fine for single-host multi-rank)
nodes=localhost,localhost,localhost,localhost,localhost,localhost,localhost,localhost

# The benchmark runner will overwrite threads/nodeCount per run.
threads=1
nodeCount=1

# Keep network threads minimal for single-host runs unless debugging transport
outgoing=1
incoming=1
ports=1

# Optional: basic pinning/topology
pinStride=1
printTopology=0
EOF
```

### Option B: Multi-node cluster (Slurm launcher)

On Slurm, the benchmark runner wraps execution with `srun` when `--launcher slurm` and `--nodes > 1`.

Notes:
- ARTS reads `SLURM_NNODES`/`SLURM_NODELIST` from the environment; you typically don’t need `nodes=...` in `arts.cfg`.
- You may still want `--arts-config external/carts-benchmarks/arts.cfg` to reuse other defaults (ports/protocol/counters), but it’s optional.

Example (run inside a Slurm allocation):

```bash
carts benchmarks run polybench/jacobi2d --size small \
  --threads 8 --nodes 4 --launcher slurm \
  -o results/multi_rank/slurm_sanity_jacobi2d_4r_8t --trace
```

---

## Experiment 0: Multi-rank sanity check (are we actually running >1 rank?)

**Goal**: Confirm that the run produced per-rank counter files (e.g., `n0_...`, `n1_...`) and did not silently fall back to single rank.

Run a tiny multi-rank job with counters enabled:

```bash
# Using --counters (auto-creates results/counters/)
carts benchmarks run polybench/jacobi2d --size small \
  --threads 2 --nodes 2 --launcher ssh \
  --arts-config results/multi_rank/configs/arts_local_8r.cfg \
  --counters -o results/multi_rank/sanity_jacobi2d_2r_2t --trace

# Or explicit counter directory:
carts benchmarks run polybench/jacobi2d --size small \
  --threads 2 --nodes 2 --launcher ssh \
  --arts-config results/multi_rank/configs/arts_local_8r.cfg \
  --counter-dir results/multi_rank/counters/sanity_jacobi2d_2r_2t \
  -o results/multi_rank/sanity_jacobi2d_2r_2t --trace
```

Quick check: you should see at least `n0_*.json` and `n1_*.json` in the counter directory.

```bash
ls -1 results/multi_rank/counters/sanity_jacobi2d_2r_2t | head
```

**Note**: The runner automatically kills any process holding the ARTS port (default: 34739) before each run, preventing port-in-use failures from lingering processes.

If you only see `n0_...` files, re-check:
- you passed an `--arts-config` that contains a valid `nodes=...` list (SSH mode), and
- `--nodes` is `> 1`.

Also note: the OpenMP baseline that `carts benchmarks run` executes is always **single-process / single-node**; only the CARTS/ARTS variant uses `--nodes`.

---

## Experiment 1: Correctness validation across ranks

**Goal**: Verify checksums remain consistent as you increase `nodeCount`.

```bash
# Keep threads per rank small at first; scale nodes
for n in 1 2 4; do
  carts benchmarks run polybench/jacobi2d --size small \
    --threads 2 --nodes $n --launcher ssh \
    --arts-config results/multi_rank/configs/arts_local_8r.cfg \
    -o results/multi_rank/correctness_jacobi2d_${n}r_2t
done
```

---

## Experiment 2: Strong scaling across ranks (fixed global problem size)

**Goal**: Measure speedup as total resources increase while the global problem size is fixed.

Pick one compute-bound and one bandwidth-bound benchmark.

### 2A. Compute-bound (GEMM)

```bash
# Fixed global problem size (large), fixed threads/rank, vary ranks
for n in 1 2 4; do
  carts benchmarks run polybench/gemm --size large \
    --threads 8 --nodes $n --launcher ssh \
    --arts-config results/multi_rank/configs/arts_local_8r.cfg \
    -o results/multi_rank/gemm_strong_large_${n}r_8t --trace
done
```

### 2B. Bandwidth-bound / stencil-like (Jacobi2D)

```bash
for n in 1 2 4; do
  carts benchmarks run polybench/jacobi2d --size large \
    --threads 8 --nodes $n --launcher ssh \
    --arts-config results/multi_rank/configs/arts_local_8r.cfg \
    -o results/multi_rank/jacobi2d_strong_large_${n}r_8t --trace
done
```

---

## Experiment 3: Weak scaling across ranks (constant work per core)

**Goal**: Keep work/core roughly constant as you increase `nodeCount`.

The runner’s weak scaling mode auto-adjusts compile-time size macros for many benchmarks (see `--weak-scaling` and `--base-size`).

### 3A. Jacobi2D weak scaling (2D work)

```bash
for n in 1 2 4; do
  carts benchmarks run polybench/jacobi2d \
    --threads 8 --nodes $n --launcher ssh \
    --arts-config results/multi_rank/configs/arts_local_8r.cfg \
    --weak-scaling --base-size 512 \
    -o results/multi_rank/jacobi2d_weak_base512_${n}r_8t --trace
done
```

### 3B. GEMM weak scaling (3D work)

```bash
for n in 1 2 4; do
  carts benchmarks run polybench/gemm \
    --threads 8 --nodes $n --launcher ssh \
    --arts-config results/multi_rank/configs/arts_local_8r.cfg \
    --weak-scaling --base-size 256 \
    -o results/multi_rank/gemm_weak_base256_${n}r_8t --trace
done
```

---

## Experiment 4: The shared-memory wall (show why multi-rank matters)

**Goal**: Identify the point where single-node OpenMP stops scaling (or hits memory limits), and contrast with multi-rank scaling.

### 4A. Establish single-node OpenMP scaling (baseline)

```bash
# OpenMP and ARTS will both run, but nodes=1 keeps this in shared memory.
carts benchmarks run polybench/jacobi2d --size large --nodes 1 --launcher ssh \
  --arts-config results/multi_rank/configs/arts_local_8r.cfg \
  --threads 1,2,4,8,16 \
  -o results/multi_rank/wall_jacobi2d_openmp_vs_arts_1node
```

Interpretation:
- Look at OpenMP kernel times across threads; scaling flattening indicates the shared-memory wall (bandwidth/synchronization/NUMA).

### 4B. Scale beyond one node (multi-rank)

```bash
for n in 1 2 4; do
  carts benchmarks run polybench/jacobi2d --size large \
    --threads 16 --nodes $n --launcher ssh \
    --arts-config results/multi_rank/configs/arts_local_8r.cfg \
    -o results/multi_rank/wall_jacobi2d_${n}r_16t
done
```

What “success” looks like:
- Even if OpenMP is faster at `n=1`, CARTS shows potential if it keeps scaling as you increase `nodeCount` (or if it enables problem sizes that don’t fit comfortably on one node).

---

## Experiment 5: Communication/ownership diagnostics (counters)

**Goal**: Turn multi-rank scaling into actionable data about remote traffic and update protocols.

This is where multi-rank tuning differs most from single-rank; keep the heuristics doc nearby:
- `docs/heuristics/multi_rank/db_granularity_and_ownership.md`

```bash
# Using --counters for automatic counter directory:
for n in 1 2 4; do
  carts benchmarks run polybench/jacobi2d --size large \
    --threads 8 --nodes $n --launcher ssh \
    --arts-config results/multi_rank/configs/arts_local_8r.cfg \
    --counters -o results/multi_rank/jacobi2d_counters_${n}r_8t
done

# Or with explicit --counter-dir for per-experiment separation:
for n in 1 2 4; do
  carts benchmarks run polybench/jacobi2d --size large \
    --threads 8 --nodes $n --launcher ssh \
    --arts-config results/multi_rank/configs/arts_local_8r.cfg \
    --counter-dir results/multi_rank/counters/jacobi2d_${n}r_8t \
    -o results/multi_rank/jacobi2d_counters_${n}r_8t
done
```

Counters to inspect (multi-rank oriented):
- `remoteBytesSent`, `remoteBytesReceived`
- `ownerUpdatesPerformed`, `ownerUpdatesSaved`

---

## Experiment 6: “CARTS potential” on dependency-heavy tasks

**Goal**: Look for cases where multi-rank DAG execution can help even if OpenMP wins on a single node.

```bash
for n in 1 2 4; do
  carts benchmarks run kastors-jacobi/jacobi-task-dep --size small \
    --threads 8 --nodes $n --launcher ssh \
    --arts-config results/multi_rank/configs/arts_local_8r.cfg \
    -o results/multi_rank/jacobi_taskdep_${n}r_8t --trace
done
```

If scaling is unexpectedly flat, check the known “task centralization” pitfall for internode `parallel_for`:
- `docs/heuristics/multi_rank/db_granularity_and_ownership.md`

---

## Key metrics to extract

From each JSON result file:

1. **Strong-scaling speedup**: use `results[].timing.*` fields across `node_count` (or compute from kernel times)
2. **Weak-scaling efficiency**: constant work/core → look for near-constant kernel time as `node_count` increases
3. **Correctness**: `results[].verification.correct`
4. **Kernel time**: `results[].run_arts.kernel_timings` vs `results[].run_omp.kernel_timings`
5. **“Shared-memory wall” evidence**: OpenMP time flattens while CARTS continues for `node_count > 1`

From counter files in `results/multi_rank/counters/*/`:

1. **Remote traffic**: `remoteBytesSent`, `remoteBytesReceived`
2. **Update behavior**: `ownerUpdatesSaved`, `ownerUpdatesPerformed`
3. **Per-`arts_id` hot spots**: `artsIdMetrics.edts[].total_exec_ns` and `invocations`

---

## Quick sanity check (run first)

```bash
carts benchmarks run polybench/gemm --size small --threads 2 --nodes 2 --launcher ssh \
  --arts-config results/multi_rank/configs/arts_local_8r.cfg \
  --counters -o results/multi_rank/sanity_gemm_2r_2t --trace
```
