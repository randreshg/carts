# CARTS Single-Rank Experiment Runbook

## Overview

This document contains the actual commands to execute for understanding CARTS behavior on a single rank (single node). Run these in order.

---

## Prerequisites

```bash
# Ensure CARTS is built
carts build --all

# Verify benchmarks are available
carts benchmarks list
```

## Counter Support (Optional)

To collect ARTS performance counters, rebuild ARTS with counter support:

```bash
# Level 1: ArtsID metrics (EDT execution times, invocations)
carts build --arts --counters=1

# Level 2: Deep captures (detailed per-EDT data)
carts build --arts --counters=2
```

**Note**: Without this step, `--counters=1` will show a warning and no counter data will be generated.

---

## Debug Mode

Use `--debug` to troubleshoot benchmark issues:

```bash
# Level 1: Show commands being executed
carts benchmarks run polybench/gemm --size small --threads 2 --debug=1

# Level 2: Write full output to log files (for ARTS debug output)
carts benchmarks run polybench/gemm --size small --threads 2 --debug=2
# Check logs at: polybench/gemm/logs/
#   - build_arts.log, build_openmp.log (build output)
#   - arts_2t.log, omp_2t.log (runtime output)
```

---

## Experiment 1: Correctness Validation

**Goal**: Verify all benchmarks produce correct results across thread counts.

```bash
# Quick correctness check - all benchmarks, small size, 2 threads
carts benchmarks run --size small --threads 2 -o results/correctness_2t

# Correctness at higher thread counts
carts benchmarks run --size small --threads 8 -o results/correctness_8t
carts benchmarks run --size small --threads 16 -o results/correctness_16t
```

---

## Experiment 2: Strong Scaling (Fixed Problem Size)

**Goal**: Measure speedup as threads increase with fixed problem size.

### 2A. Compute-Bound Benchmark (GEMM)

```bash
# GEMM - O(N³) compute, should scale well
carts benchmarks run polybench/gemm --size medium --threads 1,2,4,8,16 \
    -o results/gemm_strong_medium --trace

# Larger problem for better scaling visibility
carts benchmarks run polybench/gemm --size large --threads 1,2,4,8,16 \
    -o results/gemm_strong_large --trace
```

### 2B. Memory-Bound Benchmark (Jacobi2D)

```bash
# Jacobi2D - O(N²) memory access pattern, will hit bandwidth wall
carts benchmarks run polybench/jacobi2d --size medium --threads 1,2,4,8,16 \
    -o results/jacobi2d_strong_medium --trace

carts benchmarks run polybench/jacobi2d --size large --threads 1,2,4,8,16 \
    -o results/jacobi2d_strong_large --trace
```

### 2C. Task-Based Benchmark (Jacobi Task Dep)

```bash
# Task dependency benchmark - tests ARTS task scheduling
carts benchmarks run kastors-jacobi/jacobi-task-dep --size small --threads 1,2,4,8,16 \
    -o results/jacobi_taskdep_strong --trace
```

---

## Experiment 3: Overhead Decomposition (With Counters)

**Goal**: Understand where time is spent using ARTS counters.

**Prerequisite**: ARTS must be rebuilt with counter support (see Counter Support section above).

```bash
# Create counter output directory
mkdir -p results/counters

# GEMM with counters - see EDT creation/execution breakdown
carts benchmarks run polybench/gemm --size medium --threads 4 \
    --counters=1 --counter-dir results/counters/gemm_4t \
    -o results/gemm_4t --trace

carts benchmarks run polybench/gemm --size medium --threads 8 \
    --counters=1 --counter-dir results/counters/gemm_8t \
    -o results/gemm_8t --trace

# Jacobi2D with counters
carts benchmarks run polybench/jacobi2d --size medium --threads 4 \
    --counters=1 --counter-dir results/counters/jacobi2d_4t \
    -o results/jacobi2d_4t --trace

carts benchmarks run polybench/jacobi2d --size medium --threads 8 \
    --counters=1 --counter-dir results/counters/jacobi2d_8t \
    -o results/jacobi2d_8t --trace
```

**Output**:
- `results/gemm_4t.json` - Benchmark results (timing, checksums, speedup)
- `results/counters/gemm_4t/n0_t*.json` - ARTS counter data (EDT metrics)

---

## Experiment 4: Problem Size Sensitivity

**Goal**: Understand cache effects and how problem size affects overhead ratio.

```bash
# GEMM across sizes (small fits L2, medium fits L3, large is DRAM)
carts benchmarks run polybench/gemm --size small --threads 8 \
    -o results/gemm_size_small --trace
carts benchmarks run polybench/gemm --size medium --threads 8 \
    -o results/gemm_size_medium --trace
carts benchmarks run polybench/gemm --size large --threads 8 \
    -o results/gemm_size_large --trace

# Custom sizes for finer granularity
carts benchmarks run polybench/gemm --threads 8 --cflags "-DNI=256 -DNJ=256 -DNK=256" \
    -o results/gemm_256 --trace
carts benchmarks run polybench/gemm --threads 8 --cflags "-DNI=512 -DNJ=512 -DNK=512" \
    -o results/gemm_512 --trace
carts benchmarks run polybench/gemm --threads 8 --cflags "-DNI=1024 -DNJ=1024 -DNK=1024" \
    -o results/gemm_1024 --trace
carts benchmarks run polybench/gemm --threads 8 --cflags "-DNI=2048 -DNJ=2048 -DNK=2048" \
    -o results/gemm_2048 --trace
```

---

## Experiment 5: ARTS vs OpenMP Comparison

**Goal**: Compare ARTS kernel time vs OpenMP kernel time directly.

```bash
# The --trace flag shows kernel timings for both ARTS and OpenMP variants
# Run key benchmarks and compare the kernel_timings in output

carts benchmarks run polybench/gemm --size large --threads 1,2,4,8,16 \
    -o results/comparison_gemm --trace

carts benchmarks run polybench/jacobi2d --size large --threads 1,2,4,8,16 \
    -o results/comparison_jacobi2d --trace

carts benchmarks run polybench/2mm --size medium --threads 1,2,4,8,16 \
    -o results/comparison_2mm --trace

carts benchmarks run polybench/3mm --size medium --threads 1,2,4,8,16 \
    -o results/comparison_3mm --trace
```

---

## Experiment 6: Weak Scaling (Single Node)

**Goal**: Constant work per thread as threads increase.

```bash
# Jacobi2D weak scaling (2D problem: N scales with sqrt(threads))
carts benchmarks run polybench/jacobi2d --threads 1,2,4,8,16 \
    --weak-scaling --base-size 512 \
    -o results/jacobi2d_weak --trace

# GEMM weak scaling (3D problem: N scales with cbrt(threads))
carts benchmarks run polybench/gemm --threads 1,2,4,8 \
    --weak-scaling --base-size 256 \
    -o results/gemm_weak --trace
```

---

## Experiment 7: Full Benchmark Suite Sweep

**Goal**: Comprehensive data across all working benchmarks.

```bash
# All PolyBench benchmarks, medium size, 8 threads
carts benchmarks run --suite polybench --size medium --threads 8 \
    -o results/polybench_suite_8t --trace

# Full thread sweep on suite
carts benchmarks run --suite polybench --size medium --threads 1,2,4,8,16 \
    -o results/polybench_suite_scaling
```

---

## Results Directory Structure

Each experiment creates a self-contained timestamped directory with JSON and artifacts together:

```
results/
├── correctness_2t_YYYYMMDD_HHMMSS/
│   ├── correctness_2t.json       # Benchmark results (timing, checksums)
│   └── polybench/*/build/...     # Build artifacts
├── correctness_8t_YYYYMMDD_HHMMSS/
│   └── ...
├── gemm_strong_medium_YYYYMMDD_HHMMSS/
│   ├── gemm_strong_medium.json
│   └── polybench/gemm/build/
│       ├── 1t_1n/                # Per-config build artifacts
│       ├── 2t_1n/
│       ├── 4t_1n/
│       ├── 8t_1n/
│       └── 16t_1n/
├── gemm_4t_YYYYMMDD_HHMMSS/      # With counters
│   ├── gemm_4t.json
│   └── polybench/gemm/build/4t_1n/
│       ├── artifacts/
│       └── runs/1/counters/      # Counter files
└── ...
```

Find all results: `find results/ -name "*.json" -path "*/results/*"`

**Debug logs** (when running with `--debug=2`):
```
polybench/gemm/logs/
├── build_arts.log                # ARTS build output
├── build_openmp.log              # OpenMP build output
├── arts_2t.log                   # ARTS runtime output (2 threads)
└── omp_2t.log                    # OpenMP runtime output
```

---

## Key Metrics to Extract

### From Benchmark Results (`results/*.json`)

1. **Speedup**: `timing.speedup` (OMP kernel time / ARTS kernel time)
2. **Efficiency**: `speedup / thread_count`
3. **Correctness**: `verification.correct`
4. **Kernel Time**: `run_arts.kernel_timings` vs `run_omp.kernel_timings`
5. **Overhead**: Total time - kernel time

### From Counter Files (`counters/*/n0_t*.json`)

**Requires**: ARTS built with `carts build --arts --counters=1`

1. **EDT Metrics**: `artsIdMetrics.edts[].total_exec_ns` - execution time per arts_id
2. **Invocations**: `artsIdMetrics.edts[].invocations` - how often each EDT runs
3. **Stall Time**: `artsIdMetrics.edts[].total_stall_ns` - time waiting for data

**Note**: Counter files are only generated when:
- ARTS is rebuilt with `--counters=1` or `--counters=2`
- Benchmark is run with `--counters=1 --counter-dir <path>`

---

## Quick Sanity Check (Run First)

```bash
# Single quick test to verify everything works
mkdir -p results
carts benchmarks run polybench/gemm --size small --threads 2 -o results/sanity --trace

# Check the results (find the timestamped JSON)
cat results/sanity_*/sanity.json | python3 -c "import json,sys; d=json.load(sys.stdin); print(f'ARTS: {d[\"results\"][0][\"run_arts\"][\"status\"]} | OMP: {d[\"results\"][0][\"run_omp\"][\"status\"]} | Correct: {d[\"results\"][0][\"verification\"][\"correct\"]}')"
```

---
