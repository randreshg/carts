# CARTS Benchmarks Analysis Plan

## Objective
Perform comprehensive testing and analysis of CARTS benchmarks on a single rank to:
1. Document each benchmark's behavior and characteristics
2. Verify timer macro correctness through compiler passes
3. Understand performance differences vs OpenMP
4. Identify optimization opportunities
5. Research DB partitioning improvements

## Scope
- **Output**: `/opt/carts/external/carts-benchmarks/analysis.md`
- **Type**: Research only (no code changes)
- **Benchmarks**: All available benchmarks (polybench, kastors, ml-kernels, scientific apps)

---

## Reference: Existing Analysis Files

Use the existing `docs/analysis.md` files as templates and context:

**In tests/examples/**:
- `jacobi/docs/analysis.md` - Stencil bounds checking, dependency patterns
- `dotproduct/docs/analysis.md` - Reduction handling, partitioning
- `matrix/docs/analysis.md` - Dense linear algebra patterns
- `smith-waterman/docs/analysis.md` - Complex dependency DAGs

**In external/carts-benchmarks/**:
- `polybench/gemm/docs/analysis.md` - BLAS operation analysis
- `polybench/3mm/docs/analysis.md` - Loop reordering candidate
- `kastors-jacobi/jacobi-task-dep/docs/analysis.md` - Task dependencies
- `ml-kernels/*/docs/analysis.md` - ML kernel patterns

**Key structure of existing analysis files**:
1. Navigation & build instructions
2. Pipeline stage checkpoints with MLIR snippets
3. Inline examples showing arts.db_alloc, arts.edt, arts.epoch operations
4. Technical deep dives (bounds checking, reduction handling, etc.)

---

## Phase 1: Environment Setup and Initial Benchmark Run

### 1.1 Run Benchmarks on Single Rank (Default Size)
```bash
carts benchmarks run --trace
```
- Capture baseline results for all benchmarks
- Record timing data, checksums, and any errors
- Identify which benchmarks pass/fail

### 1.2 Run Benchmarks with Large Size
```bash
carts benchmarks run --size large --trace
```
- Compare results with default size
- Identify any size-dependent failures or issues

---

## Phase 1.5: Document Optimization Pipeline Per Benchmark

### Optimization Pipeline Stages (in order)
```
1. canonicalize-memrefs  - Normalize memref operations (inlining, CSE)
2. collect-metadata      - Extract loop/array metadata for optimization
3. initial-cleanup       - Remove dead code, simplify control flow
4. openmp-to-arts        - Convert OpenMP parallel regions to ARTS EDTs
5. edt-transforms        - Optimize EDT (Event Driven Task) structure
6. create-dbs            - Create DataBlocks for inter-task communication
7. db-opt                - Optimize DataBlock operations
8. edt-opt               - Further EDT optimizations (loop fusion)
9. concurrency           - Add concurrency annotations
10. concurrency-opt      - Optimize concurrent execution (partitioning)
11. epochs               - Add epoch synchronization
12. pre-lowering         - Prepare for LLVM lowering (ptr hoisting, scalar replacement)
13. arts-to-llvm         - Final lowering to LLVM IR
14. additional-opt       - Post-pipeline classical optimizations (-O3)
15. llvm-ir-emission     - Generate LLVM IR with alias scope metadata
```

### Key Heuristics to Document

**H1: Read-only Allocation Heuristic** (`DbPartitioning.cpp:230-234`)
- On single-node: keep read-only memrefs as coarse allocation
- Rationale: No write contention to relieve, fine-graining adds overhead

**H2: Cost Model Heuristic** (`HeuristicsConfig.cpp:25-188`)
- Evaluates fine-grained vs coarse-grained allocation
- Parameters: totalDBs, totalDBsPerEDT, innerBytes
- Thresholds depend on mode (SingleThreaded, IntraNode, InterNode)

**H4: Twin-Diff Heuristic** (`DbPartitioning.cpp:179-189`)
- On single-node: disable twin-diff (no remote nodes to send diffs to)
- Twin allocation + memcpy + diff computation = pure overhead on single-node

**Block Distribution** (`ForLowering.cpp:55-91`)
- `chunkSize = ceil(totalIterations / numWorkers)`
- Each worker processes iterations: `[start, start + count)`

### How to Capture Optimizations Applied
For each benchmark, run:
```bash
# Dump all pipeline stages
carts run --all-stages benchmark.mlir -o ./stages/

# Export heuristic decisions
carts-run benchmark.mlir --diagnose --diagnose-output diag.json

# Stop at specific stage for inspection
carts-run benchmark.mlir --stop-at=concurrency-opt -o after-opt.mlir
```

### Per-Benchmark Documentation Template
For each benchmark, document in analysis.md:

| Heuristic | Applied? | Rationale | Impact |
|-----------|----------|-----------|--------|
| H1 (Read-only coarse) | Yes/No | [reason] | [effect on performance] |
| H2 (Fine-grained cost) | Yes/No | [params used] | [allocation strategy] |
| H4 (Twin-diff disable) | Yes/No | [single-node?] | [memory overhead saved] |
| Block distribution | Yes/No | [chunk size] | [worker partitioning] |

---

## Phase 2: Timer Macro Verification

### 2.1 Verification Strategy
For each benchmark, verify `CARTS_KERNEL_TIMER_START` and `CARTS_KERNEL_TIMER_STOP` macros are not affected by:
- Common Sub-expression Elimination (CSE)
- Dead Code Elimination (DCE)
- Loop Invariant Code Motion (LICM)
- Other compiler optimizations

### 2.2 Verification Process
For each benchmark `example.mlir`:
```bash
# Generate LLVM IR with full optimization pipeline
carts run example.mlir --arts-to-llvm -o example.ll

# Inspect the generated IR for timer call positioning
grep -n "carts_bench_get_time\|kernel\." example.ll
```

### 2.3 Files to Verify (26 benchmark files using timer macros)
- `external/carts-benchmarks/polybench/*/` - gemm, 3mm, 2mm, jacobi2d, etc.
- `external/carts-benchmarks/kastors-jacobi/*/`
- `external/carts-benchmarks/ml-kernels/*/`

### 2.4 Expected Pattern in IR
```
call @carts_bench_get_time  ; START - before kernel
; ... kernel operations ...
call @carts_bench_get_time  ; STOP - after kernel
call @printf                ; timing output
```

---

## Phase 3: Individual Benchmark Analysis

### 3.1 Analysis Template for Each Benchmark
For each benchmark, document in `analysis.md`:

1. **Benchmark Overview**
   - Name, suite, purpose
   - Input/output characteristics
   - Computational pattern (stencil, BLAS, etc.)

2. **CARTS vs OpenMP Comparison**
   - Performance metrics (speedup/slowdown)
   - Timer placement verification
   - Generated MLIR/LLVM quality

3. **Optimization Opportunities**
   - Loop structure analysis
   - Memory access patterns
   - Partitioning effectiveness

4. **Recommendations**
   - Why faster/slower than OpenMP
   - Potential improvements

### 3.2 Key Benchmarks to Analyze

| Benchmark | Pattern | Priority |
|-----------|---------|----------|
| polybench/3mm | Matrix multiplication chain | High (loop reordering candidate) |
| polybench/gemm | Single GEMM | High (baseline BLAS) |
| polybench/jacobi2d | 2D stencil | High (common pattern) |
| polybench/2mm | Two matrix mults | Medium |
| kastors-jacobi/jacobi-task-dep | Task dependencies | Medium |

---

## Phase 4: Performance Investigation

### 4.1 Why CARTS Might Be Better Than OpenMP
- **Task-based parallelism**: Better load balancing
- **DB partitioning**: Fine-grained data locality
- **Twin-diff synchronization**: Reduced communication overhead
- **EDT execution model**: Asynchronous task execution

### 4.2 Why CARTS Might Be Worse Than OpenMP
- **Runtime overhead**: Task creation/scheduling cost
- **Suboptimal partitioning**: Coarse-grained chunking
- **Missing optimizations**: Loop reordering, vectorization hints
- **Memory management**: DB allocation overhead

### 4.3 Investigation Methods
```bash
# Profile with timing breakdown
carts benchmarks run polybench/gemm --trace --debug=2

# Compare ARTS vs OpenMP IR
diff <(carts run gemm.c -S --collect-metadata) \
     <(carts clang gemm.c -fopenmp -S -emit-llvm)
```

---

## Phase 5: Optimization Opportunities

### 5.1 Loop Reordering for polybench/3mm
Current limitation: Affine analysis is lost after lowering to SCF.

**Proposed solution:**
1. During `CollectMetadata` pass, analyze loop nesting using affine analysis
2. Attach optimal loop order as attribute (e.g., `arts.loop_order = [2, 0, 1]`)
3. Use this metadata in `ForLowering` to reorder loops

**Key files:**
- `/opt/carts/lib/arts/Passes/CollectMetadata.cpp`
- `/opt/carts/include/arts/Utils/Metadata/LoopMetadata.h`
- `/opt/carts/lib/arts/Passes/ForLowering.cpp`

### 5.2 DB Partitioning Improvements
Current partitioning (`DbPartitioning.cpp`):
- N-D first-dimension chunking
- Heuristic-based decisions

**Potential improvements:**
1. **Data locality-aware partitioning**: Use access pattern analysis
2. **Multi-dimensional chunking**: For 3D stencils (sw4lite, specfem3d)
3. **Adaptive chunk sizing**: Based on cache hierarchy

### 5.3 Other Optimization Candidates
- Vectorization hints from loop metadata
- Prefetching based on reuse distance
- Reduction optimization for commutative operations

---

## Phase 6: Documentation Output

### 6.1 analysis.md Structure
# CARTS Benchmarks Analysis

## Executive Summary
- Total benchmarks analyzed: N
- Pass rate: X%
- Average speedup vs OpenMP: Y.Zx

## Per-Benchmark Analysis

### polybench/3mm
**Overview**: Three matrix multiplications (A*B->E, C*D->F, E*F->G)
**Computational Pattern**: Dense linear algebra, triply-nested loops

**Optimizations Applied**:
| Stage | Pass | Effect |
|-------|------|--------|
| canonicalize-memrefs | CSE, Inliner | Normalized array accesses |
| collect-metadata | LoopAnalyzer | Detected trip counts, nesting |
| create-dbs | CreateDbs | Created DBs for A, B, C, D, E, F, G |
| concurrency-opt | DbPartitioning | H2: fine-grained chunking |

**Heuristics Applied**:
| Heuristic | Applied? | Rationale | Impact |
|-----------|----------|-----------|--------|
| H1 | Yes | A,B,C,D read-only | Coarse allocation |
| H2 | Yes | Large matrices | Fine-grained for E,F,G |
| H4 | Yes | Single-node | Twin-diff disabled |

**Performance**: X.Xx speedup vs OpenMP
**Why Better/Worse**: [analysis]
**Optimization Opportunities**: Loop reordering (ikj order for cache)

### polybench/gemm
[similar structure...]

## Timer Macro Verification Results
| Benchmark | Timer Start OK | Timer Stop OK | Notes |
|-----------|---------------|---------------|-------|
| polybench/3mm | Yes | Yes | Not moved by CSE |
| polybench/gemm | Yes | Yes | Correct placement |

## Performance Analysis
### Why CARTS Is Better
- [findings with specific benchmark evidence]

### Why CARTS Is Worse
- [findings with specific benchmark evidence]

## Optimization Recommendations
1. **Loop Reordering for 3mm** - Attach loop order metadata during affine analysis
2. **Multi-dim chunking for stencils** - Improve locality for jacobi2d, fdtd-2d
3. [prioritized list...]

## DB Partitioning Research
[findings and proposals]

---

## Deliverables

**Single Output File**: `/opt/carts/external/carts-benchmarks/analysis.md`

Contents:
1. **Benchmark Analysis** - Per-benchmark documentation with characteristics
2. **Timer Verification Results** - Confirmation that macros aren't affected by CSE/DCE
3. **Performance Comparison** - CARTS vs OpenMP with explanation of why better/worse
4. **Optimization Recommendations** - Prioritized proposals (e.g., loop reordering for 3mm)
5. **DB Partitioning Research** - Ideas for improved partitioning strategies

---

## Critical Files

### Benchmarks
- `/opt/carts/external/carts-benchmarks/` - All benchmark sources
- `/opt/carts/external/carts-benchmarks/benchmark_runner.py` - Runner implementation
- `/opt/carts/external/carts-benchmarks/common/carts.mk` - Build rules

### Existing Analysis Files (Reference)
- `/opt/carts/tests/examples/jacobi/docs/analysis.md` - Stencil pattern reference
- `/opt/carts/tests/examples/dotproduct/docs/analysis.md` - Reduction handling
- `/opt/carts/external/carts-benchmarks/polybench/gemm/docs/analysis.md` - BLAS reference

### Timer Macros
- `/opt/carts/include/arts/Utils/Benchmarks/CartsBenchmarks.h` - Timer definitions

### Optimization Pipeline
- `/opt/carts/tools/run/carts-run.cpp` - Main pipeline orchestration (setupXXX functions)
- `/opt/carts/tools/carts_cli.py` - CLI for --all-stages, --stop-at, --diagnose

### Optimization Passes
- `/opt/carts/lib/arts/Passes/CollectMetadata.cpp` - Metadata collection from affine analysis
- `/opt/carts/lib/arts/Passes/DbPartitioning.cpp` - H1, H4 heuristics, twin-diff decisions
- `/opt/carts/lib/arts/Passes/ForLowering.cpp` - Block distribution, worker partitioning

### Heuristics Infrastructure
- `/opt/carts/lib/arts/Analysis/HeuristicsConfig.cpp` - H2 cost model, thresholds
- `/opt/carts/include/arts/Analysis/HeuristicsConfig.h` - Heuristic interface
- `/opt/carts/lib/arts/Analysis/ArtsAnalysisManager.cpp` - Diagnostics export, decision tracking

### Machine Configuration
- `/opt/carts/.install/arts/etc/arts/arts.cfg` - Runtime configuration (threads, nodes, memory tier)

### Metadata Infrastructure
- `/opt/carts/include/arts/Utils/Metadata/LoopMetadata.h` - Loop metadata attributes
- `/opt/carts/include/arts/Analysis/Metadata/ArtsMetadataManager.h` - Metadata manager
