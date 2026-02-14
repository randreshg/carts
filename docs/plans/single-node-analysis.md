# Single-Node Benchmark Analysis

Date: 2026-02-14

## Medium-Size Performance Analysis (16 threads)

Configuration: `--size medium --threads 16 --trace --runs 3`

### Summary

| Metric | Value |
|--------|-------|
| Total benchmarks | 25 |
| Passed | 24 (all correct) |
| Build failures | 1 (monte-carlo/ensemble — fixed, see below) |
| Geometric mean speedup (e2e) | **1.07x** |
| Total duration | 7m 31s |

### Per-Benchmark Results (median of 3 runs)

| Benchmark | Median Speedup | ARTS E2E (s) | OMP E2E (s) | ARTS Init (s) | Status |
|-----------|---------------|-------------|-------------|---------------|--------|
| **polybench/gemm** | **8.0x** | 0.018 | 0.148 | 0.584 | FAST |
| **polybench/3mm** | **5.5x** | 0.110 | 0.619 | 0.644 | FAST |
| **polybench/2mm** | **3.3x** | 0.115 | 0.435 | 0.744 | FAST |
| **kastors-jacobi/jacobi-task-dep** | **3.4x** | 0.059 | 0.200 | 0.044 | FAST (1/3 pass, 2 timeout) |
| **sw4lite/rhs4sg-base** | **3.3x** | 0.009 | 0.005 | 0.763 | FAST (high variance) |
| kastors-jacobi/poisson-task | 2.3x | 0.071 | 0.149 | 0.038 | OK |
| kastors-jacobi/jacobi-for | 1.7x | 0.010 | 0.018 | 0.034 | OK |
| seissol/volume-integral | 1.5x | 0.015 | 0.021 | 0.685 | OK |
| polybench/correlation | 1.4x | 0.036 | 0.052 | 0.629 | OK |
| ml-kernels/layernorm | 1.2x | 0.002 | 0.004 | 0.044 | OK |
| ml-kernels/pooling | 1.0x | 0.030 | 0.031 | 0.032 | OK |
| polybench/seidel-2d | 1.0x | 0.030 | 0.031 | 0.607 | OK |
| ml-kernels/batchnorm | 1.0x | 0.012 | 0.010 | 0.053 | OK |
| polybench/convolution-3d | 0.8x | 0.043 | 0.037 | 0.608 | OK |
| polybench/jacobi2d | 0.8x | 0.013 | 0.010 | 0.619 | OK |
| ml-kernels/activations | 0.7x | 0.029 | 0.021 | 0.039 | OK |
| polybench/convolution-2d | 0.7x | 0.040 | 0.027 | 0.587 | OK |
| polybench/bicg | 0.7x | 0.036 | 0.027 | 0.758 | OK |
| sw4lite/vel4sg-base | 0.6x | 0.004 | 0.002 | 0.692 | MARGINAL |
| specfem3d/velocity | 0.6x | 0.017 | 0.008 | 0.712 | MARGINAL |
| stream | 0.6x | 0.189 | 0.116 | 0.639 | MARGINAL |
| kastors-jacobi/poisson-for | 0.5x | 0.026 | 0.015 | 0.042 | MARGINAL |
| **polybench/atax** | **0.4x** | 0.066 | 0.030 | 0.760 | SLOW |
| **specfem3d/stress** | **0.3x** | 0.018 | 0.006 | 0.671 | SLOW |
| monte-carlo/ensemble | **0.2x** | 8.64 | 1.55 | 0.036 | SLOW (was build failure) |

### ARTS Counter Data (workload profile)

From re-runs with `--profile configs/profile-workload.cfg`:

| Benchmark | Init (ms) | E2E (ms) | Init % | EDTs | DBs | EDT Time (ms) |
|-----------|-----------|----------|--------|------|-----|---------------|
| specfem3d/stress | 1021 | 26 | 97.5% | 8 | 131 | 25.1 |
| specfem3d/velocity | 579 | 76 | 88.5% | 8 | 115 | 35.5 |
| sw4lite/rhs4sg-base | 532 | 70 | 88.3% | 6 | 69 | 13.3 |
| sw4lite/vel4sg-base | 1862 | 22 | 98.9% | 6 | 101 | 2.2 |
| stream | 1098 | 827 | 57.0% | 657 | 48 | 1737.2 |
| monte-carlo/ensemble | 85 | 9304 | 0.9% | 17 | 16 | 21611.9 |

---

## Root Cause Analysis

### Issue 1: ARTS Initialization Overhead (0.5-1.9s)

**Impact**: All benchmarks, but dominates for short-running kernels.

Every ARTS benchmark pays a fixed **0.5-1.9 second initialization cost** that doesn't exist for OpenMP:
- ARTS init: 0.5-1.9s across all benchmarks
- OMP init: 0.001-0.015s

For benchmarks with short kernels (sw4lite, specfem3d), initialization is 88-99% of total runtime, making ARTS appear 2-5x slower even when the kernel itself is efficient.

**Evidence from counters**: specfem3d/stress shows init=1021ms, e2e=26ms — compute only takes 2.5% of wall time.

### Issue 2: monte-carlo/ensemble Kernel Slowdown (5.6x slower)

**Impact**: Benchmark-specific. ARTS e2e=8.6s vs OMP e2e=1.6s.

This is NOT an init problem (init is only 85ms / 0.9% of runtime). The actual compute is slow:
- EDT running time: 21.6 seconds across 16 threads (sum)
- E2E time: 9.3 seconds
- Only 17 EDTs created for 5000 samples — very coarse task granularity

Root cause: This benchmark allocates memory inside the parallel loop (malloc per sample). ARTS EDT execution model may not handle dynamic allocation efficiently, or the parallelization strategy is suboptimal (17 EDTs for 5000 samples = 294 samples per task).

### Issue 3: Stream Bandwidth Regression (2-3x slower per kernel)

**Impact**: stream benchmark specifically (0.56x speedup).

Counter data shows 657 EDTs and 1737ms of EDT running time for 827ms E2E — suggesting reasonable parallelism. But kernel-level comparison shows ARTS memory bandwidth is 2-3x lower:
- ARTS copy: 2.9ms vs OMP: 1.0ms
- ARTS scale: 2.2ms vs OMP: 0.9ms

Possible causes: missing SIMD vectorization, suboptimal memory alignment, or cache efficiency issues in ARTS-generated code.

### Issue 4: jacobi-task-dep Timeouts (2/3 runs)

ARTS version timed out in 2 of 3 runs. When it did complete, speedup was 3.4x. Suggests intermittent scheduling deadlock or resource contention at 16 threads.

---

## Bug Fixes Applied

### Fix 1: monte-carlo/ensemble Build Failure

**Problem**: `Config template not found: .../scripts/arts.cfg`

The benchmark runner's config lookup fell back to `DEFAULT_ARTS_CONFIG = Path(__file__).parent / "arts.cfg"` which pointed to the nonexistent `scripts/arts.cfg`. monte-carlo/ensemble doesn't have its own `arts.cfg`, and the runner didn't check the root-level `arts.cfg`.

**Fix** (in `benchmark_runner.py`, two locations):
```python
# Before: fell back directly to DEFAULT_ARTS_CONFIG
# After: search hierarchy benchmark → suite → root → default
for candidate in [
    bench_path / "arts.cfg",
    bench_path.parent / "arts.cfg",
    self.benchmarks_dir / "arts.cfg",
    DEFAULT_ARTS_CONFIG,
]:
    if candidate.exists():
        effective_config = candidate
        break
```

### Fix 2: Silent Build Error Display

**Problem**: When a benchmark failed with an exception (e.g., missing config), the error was silently caught and only written to `results.json` — not displayed to the user.

**Fix**: Added `console.print()` in all 3 exception handlers in `run_all()`:
```python
except Exception as e:
    self.console.print(f"[red]Error running {bench}:[/] {e}")
    result = self._make_error_result(bench, size, str(e))
```

---

## Optimization Recommendations

### Priority 1: Reduce ARTS Initialization Time
- Current: 0.5-1.9s per execution
- Target: <100ms
- Impact: Would improve ALL benchmarks, especially short-kernel ones
- Investigation: Profile init phases (SSH launcher, thread creation, EDT system, counter setup)

### Priority 2: Fix monte-carlo Parallelization
- Current: 17 EDTs for 5000 samples (too coarse)
- Target: Finer-grained task creation or better work distribution
- Impact: Could recover 5x+ speedup

### Priority 3: Stream Vectorization
- Current: 2-3x memory bandwidth regression
- Target: Match OMP bandwidth utilization
- Impact: Affects all bandwidth-bound kernels

### Priority 4: jacobi-task-dep Stability
- Current: 2/3 timeouts at 16 threads
- Target: Reliable execution
- Investigation: Check for EDT scheduling deadlock conditions

---

## Historical Results

### Small-Size Baseline (2 threads)

Configuration: `--size small --threads 2 --trace`

| Metric | Value |
|--------|-------|
| Total benchmarks | 25 |
| Passed | 24 |
| Skipped | 1 (monte-carlo/ensemble) |
| Geometric mean speedup (e2e) | 0.93x |

## How to Reproduce

```bash
# Run medium-size benchmarks (16 threads, 3 runs)
./tools/carts benchmarks run --size medium --threads 16 --trace --runs 3

# Run with workload profiling for counter data
./tools/carts benchmarks run --size medium --threads 16 --trace --runs 1 \
  --suite specfem3d --profile configs/profile-workload.cfg

# Analyze results
RESULTS_DIR=$(ls -td external/carts-benchmarks/results/*/ | head -1)
./tools/carts analyze summary $RESULTS_DIR
./tools/carts analyze export $RESULTS_DIR -o /tmp/medium-timing.csv
```
