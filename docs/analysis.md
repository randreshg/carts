# CARTS Optimization Analysis Guide

This guide teaches agents and developers how to understand, run, and debug CARTS optimizations. It covers the complete pipeline from C/C++ source to ARTS runtime executable.

## Table of Contents
1. [Quick Reference Card](#quick-reference-card)
2. [Pipeline Overview](#pipeline-overview)
3. [Pipeline Stages](#pipeline-stages)
4. [Pass-Level Debug Reference](#pass-level-debug-reference)
5. [Common Issues & Troubleshooting](#common-issues--troubleshooting)
6. [Example Workflows](#example-workflows)

---

## Quick Reference Card

### Essential Commands

```bash
# Build CARTS
carts build

# Generate MLIR from C/C++
carts cgeist <file>.c -O0 --print-debug-info -S --raise-scf-to-affine           # Sequential
carts cgeist <file>.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine  # Parallel

# Run pipeline (stop at specific stage)
carts run <file>.mlir --stop-after=<stage>

# Run pipeline (dump all intermediate stages)
carts run <file>.mlir --all-stages -o ./stages/

# Collect metadata (dual-compilation)
carts run <file>_seq.mlir --collect-metadata

# Debug a specific pass
carts run <file>.mlir --stop-after=<stage> --debug-only=<pass> 2>&1

# Full compilation and execution
carts execute <file>.c -O3

# Full compilation with diagnostics
carts execute <file>.c -O3 --diagnose
```

### Pipeline Stage Names (for --stop-at)

| Stage | Name | Purpose |
|-------|------|---------|
| 1 | `canonicalize-memrefs` | Normalize memref operations |
| 2 | `collect-metadata` | Extract loop/array metadata |
| 3 | `initial-cleanup` | Remove dead code |
| 4 | `openmp-to-arts` | Convert OpenMP to ARTS |
| 5 | `edt-transforms` | Optimize EDT structure |
| 6 | `loop-reordering` | Cache-optimal loop order |
| 7 | `create-dbs` | Create DataBlocks |
| 8 | `db-opt` | Optimize DB modes |
| 9 | `edt-opt` | EDT fusion |
| 10 | `concurrency` | Build concurrency graph |
| 11 | `concurrency-opt` | DB partitioning |
| 12 | `epochs` | Epoch synchronization |
| 13 | `pre-lowering` | Prepare for LLVM |
| 14 | `arts-to-llvm` | Final LLVM conversion |

### Debug Types (for --debug-only)

```bash
--debug-only=collect_metadata
--debug-only=loop_reordering
--debug-only=create_dbs
--debug-only=db
--debug-only=db_partitioning
--debug-only=edt
--debug-only=concurrency
--debug-only=for_lowering
--debug-only=edt_lowering
--debug-only=convert_arts_to_llvm
--debug-only=arts_alias_scope_gen
--debug-only=arts_loop_vectorization_hints
--debug-only=arts_prefetch_hints
--debug-only=arts_data_pointer_hoisting
```

---

## Pipeline Overview

```
C/C++ Source (.c/.cpp)
        │
        ▼ carts cgeist (Polygeist frontend)
┌─────────────────────────────────────────────────────────────────┐
│                      MLIR PIPELINE                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ PHASE 1: NORMALIZATION & ANALYSIS                       │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │ 1. canonicalize-memrefs  │ Normalize memref operations  │   │
│  │ 2. collect-metadata      │ Extract loop/array metadata  │   │
│  │ 3. initial-cleanup       │ Dead code elimination        │   │
│  └─────────────────────────────────────────────────────────┘   │
│                         │                                       │
│                         ▼                                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ PHASE 2: OPENMP → ARTS TRANSFORMATION                   │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │ 4. openmp-to-arts        │ OMP parallel → ARTS EDTs     │   │
│  │ 5. edt-transforms        │ EDT structure optimization   │   │
│  │ 6. loop-reordering       │ Cache-optimal loop order     │   │
│  └─────────────────────────────────────────────────────────┘   │
│                         │                                       │
│                         ▼                                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ PHASE 3: DATABLOCK MANAGEMENT                           │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │ 7. create-dbs            │ Create DataBlock allocations │   │
│  │ 8. db-opt                │ Optimize DB access modes     │   │
│  │ 9. edt-opt               │ EDT fusion & optimization    │   │
│  └─────────────────────────────────────────────────────────┘   │
│                         │                                       │
│                         ▼                                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ PHASE 4: CONCURRENCY & SYNCHRONIZATION                  │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │ 10. concurrency          │ Build EDT concurrency graph  │   │
│  │ 11. concurrency-opt      │ DB partitioning & twin-diff  │   │
│  │ 12. epochs               │ Epoch synchronization        │   │
│  └─────────────────────────────────────────────────────────┘   │
│                         │                                       │
│                         ▼                                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ PHASE 5: LOWERING TO LLVM                               │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │ 13. pre-lowering         │ EDT/DB/Epoch lowering        │   │
│  │ 14. arts-to-llvm         │ Final ARTS → LLVM conversion │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
        │
        ▼ --emit-llvm
┌─────────────────────────────────────────────────────────────────┐
│                   POST-LOWERING OPTIMIZATIONS                   │
├─────────────────────────────────────────────────────────────────┤
│ • AliasScopeGen           │ LLVM alias metadata for vectorizer │
│ • LoopVectorizationHints  │ LLVM loop hints (!llvm.loop)       │
│ • PrefetchHints           │ Software prefetch intrinsics       │
└─────────────────────────────────────────────────────────────────┘
        │
        ▼
    LLVM IR (.ll) → Executable
```

### Key Concepts

**EDT (Event-Driven Task):** ARTS unit of parallel execution. Created from OpenMP parallel regions/tasks.

**DataBlock (DB):** ARTS memory abstraction for inter-task communication. Handles data dependencies automatically.

**Epoch:** Synchronization barrier grouping related EDTs.

**Twin-Diff:** Runtime mechanism to handle overlapping memory writes between parallel workers.

---

## Pipeline Stages

### Stage 1: canonicalize-memrefs

**Purpose:** Normalize memref operations and inline functions to prepare for analysis.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=canonicalize-memrefs
```

**Passes Executed:**
- `LowerAffine` - Lower affine dialect operations
- `CSE` - Common subexpression elimination
- `Inliner` - MLIR function inlining
- `ArtsInliner` - ARTS-specific inlining
- `CanonicalizeMemrefs` - Normalize memref allocations and accesses
- `DeadCodeElimination` - Remove unused code

**What to Look For:**
- Nested pointer arrays (`memref<?xmemref<?xT>>`) converted to multi-dimensional memrefs (`memref<?x?xT>`)
- Functions inlined for better analysis scope
- Simplified control flow

**Common Issues:**
- **Wrapper allocas with circular aliases:** Pointer swap patterns (like in Jacobi double-buffering) may not canonicalize properly
  - *Fix:* Use explicit double-buffering without pointer swap

---

### Stage 2: collect-metadata

**Purpose:** Analyze sequential code to extract loop bounds, array dimensions, and access patterns for dual-compilation strategy.

**Run Command:**
```bash
carts run <file>_seq.mlir --collect-metadata
```

**Debug Command:**
```bash
carts run <file>_seq.mlir --collect-metadata --debug-only=collect_metadata 2>&1
```

**Passes Executed:**
- `RaiseSCFToAffine` - Convert SCF loops to affine for analysis
- `CollectMetadata` - Extract and export metadata to JSON

**Output Artifacts:**
- `.carts-metadata.json` - Contains loop IDs, memref IDs, dimensions, access patterns
- `<file>_arts_metadata.mlir` - MLIR with metadata annotations

**What to Look For:**
- Loop trip counts (static vs dynamic)
- Array dimensions inferred from access patterns (e.g., `i*128+j` → `[128,128]`)
- Memory access patterns for partitioning decisions

**Debug Output:**
```
[DEBUG] [collect_metadata] Collecting loop metadata...
[DEBUG] [collect_metadata] Collecting memref metadata...
```

---

### Stage 3: initial-cleanup

**Purpose:** Remove dead code and simplify control flow after metadata collection.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=initial-cleanup
```

**Passes Executed:**
- `LowerAffine` - Lower remaining affine operations
- `CSE` - Common subexpression elimination
- `CanonicalizeFor` - Canonicalize for loops

---

### Stage 4: openmp-to-arts

**Purpose:** Transform OpenMP parallel constructs into ARTS EDT operations.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=openmp-to-arts
```

**Debug Command:**
```bash
carts run <file>.mlir --stop-after=openmp-to-arts --debug-only=convert_openmp_to_arts 2>&1
```

**Passes Executed:**
- `ConvertOpenMPToArts` - Transform OMP parallel regions to ARTS EDTs
- `DeadCodeElimination` - Remove dead OMP dependency proxies
- `CSE` - Common subexpression elimination

**Transformations:**
| OpenMP | ARTS |
|--------|------|
| `omp.parallel` | `arts.edt<parallel>` |
| `omp.task` | `arts.edt<task>` |
| `omp.wsloop` | `arts.for` |
| `omp.taskwait` | `arts.barrier` |
| `depend(in:x)` | DB acquire with `mode=in` |
| `depend(out:x)` | DB acquire with `mode=out` |
| `depend(inout:x)` | DB acquire with `mode=inout` |

**What to Look For:**
```mlir
// Before (OpenMP)
omp.parallel {
  omp.wsloop for (%i) : i64 = (%lb) to (%ub) step (%step) {
    // loop body
  }
}

// After (ARTS)
arts.edt <parallel> {
  arts.for (%i) = (%lb) to (%ub) step (%step) {
    // loop body
  }
}
```

---

### Stage 5: edt-transforms

**Purpose:** Optimize EDT structure through invariant code motion and pointer rematerialization.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=edt-transforms
```

**Debug Command:**
```bash
carts run <file>.mlir --stop-after=edt-transforms --debug-only=edt 2>&1
```

**Passes Executed:**
- `Edt` (initial) - Initial EDT transformation
- `EdtInvariantCodeMotion` - Hoist loop-invariant code from EDT regions
- `DeadCodeElimination` - Remove dead code
- `SymbolDCE` - Dead symbol elimination
- `EdtPtrRematerialization` - Optimize pointer dependencies

---

### Stage 6: loop-reordering

**Purpose:** Reorder inner loops for cache-optimal memory access patterns based on metadata analysis.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=loop-reordering
```

**Debug Command:**
```bash
carts run <file>.mlir --stop-after=loop-reordering --debug-only=loop_reordering,loop_transforms 2>&1
```

**Passes Executed:**
- `LoopReordering` - Apply loop interchange transformations
- `ArtsLoopTransforms` - Matmul transforms + optional tiling (reduction-aware)
- `CSE` - Common subexpression elimination

**Critical Ordering:** **MUST run BEFORE create-dbs** because:
1. CreateDbs uses SSA indices from DbControlOps
2. DbPartitioning analyzes loop IV relationships for chunk computation
3. Reordering after CreateDbs causes SSA changes → chunk computation fails

**Transformations:**
```mlir
// Matrix multiply: i-j-k (row-major C, bad locality for B)
// Reordered to: i-k-j (better cache reuse for B)

// Before
scf.for %i = %c0 to %M {
  scf.for %j = %c0 to %N {
    scf.for %k = %c0 to %K {
      // C[i,j] += A[i,k] * B[k,j]  <- B access has stride N
    }
  }
}

// After
scf.for %i = %c0 to %M {
  scf.for %k = %c0 to %K {
    scf.for %j = %c0 to %N {
      // C[i,j] += A[i,k] * B[k,j]  <- B access is contiguous
    }
  }
}
```

**Debug Output:**
```
[DEBUG] [loop_reordering] Found loop with reorder_nest_to: 3 loops
[INFO] [loop_reordering] Applied loop reordering to arts.for
[DEBUG] [loop_reordering] Auto-detected matmul pattern: 2 init ops, j-k nest
```

---

### Stage 7: create-dbs

**Purpose:** Create DataBlock abstractions for memory shared between EDTs.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=create-dbs
```

**Debug Command:**
```bash
carts run <file>.mlir --stop-after=create-dbs --debug-only=create_dbs 2>&1
```

**Passes Executed:**
- `CreateDbs` - Identify external allocations, create DB operations
- `PolygeistCanonicalize` - Canonicalize polygeist operations
- `CSE`, `SymbolDCE`, `Mem2Reg` - Cleanup passes

**Allocation Strategies:**

| Strategy | Outer Dims | Use Case |
|----------|-----------|----------|
| Coarse-grained | `sizes=[1]` | Single datablock for entire array |
| Fine-grained | `sizes=[N]` | One datablock per element |
| Chunked | `sizes=[numChunks]` | Multiple datablocks matching workers |

**What to Look For:**
```mlir
// DataBlock allocation
%db = arts.db_alloc %memref : memref<?x?xf64> -> !arts.db<memref<?x?xf64>>
  { outer_dims = [1], sizes = [1] }  // Coarse-grained

// Acquire before EDT access
%view = arts.db_acquire %db [%idx] { mode = "in" } : !arts.db<...> -> memref<...>

// Release after EDT access
arts.db_release %view : memref<...>

// Free at end of scope
arts.db_free %db : !arts.db<...>
```

---

### Stage 8: db-opt

**Purpose:** Optimize DataBlock access modes based on actual load/store operations.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=db-opt
```

**Debug Command:**
```bash
carts run <file>.mlir --stop-after=db-opt --debug-only=db 2>&1
```

**Passes Executed:**
- `Db` - Adjust DB modes based on access patterns
- `PolygeistCanonicalize`, `CSE`, `Mem2Reg` - Cleanup passes

**Mode Optimization:**
- Loads only → `mode = "in"`
- Stores only → `mode = "out"`
- Both → `mode = "inout"`

---

### Stage 9: edt-opt

**Purpose:** EDT fusion and further optimization.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=edt-opt
```

**Debug Command:**
```bash
carts run <file>.mlir --stop-after=edt-opt --debug-only=edt,arts_loop_fusion 2>&1
```

**Passes Executed:**
- `PolygeistCanonicalize` - Canonicalize operations
- `Edt` (full analysis) - EDT optimization
- `LoopFusion` - Fuse independent arts.for loops
- `CSE` - Common subexpression elimination

**Loop Fusion:**
```mlir
// Before: Two independent loops with barrier
arts.edt <parallel> {
  arts.for(%i) to(%N) {
    // compute E[i]
  }
  // implicit barrier
  arts.for(%i) to(%N) {
    // compute F[i] (independent of E)
  }
}

// After: Fused loop
arts.edt <parallel> {
  arts.for(%i) to(%N) {
    // compute E[i]
    // compute F[i]
  }
}
```

---

### Stage 10: concurrency

**Purpose:** Build EDT concurrency graph and lower parallel loops to chunked tasks.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=concurrency
```

**Debug Command:**
```bash
carts run <file>.mlir --stop-after=concurrency --debug-only=concurrency,for_lowering 2>&1
```

**Passes Executed:**
- `Concurrency` - Build EDT concurrency graph
- `ForLowering` - Lower arts.for in parallel EDTs to chunked tasks
- `PolygeistCanonicalize`, `CSE` - Cleanup passes

**For Lowering Transformation:**
```mlir
// Before
arts.for (%i) = (%c0) to (%N) {
  // loop body
}

// After (conceptual)
%num_workers = arts.get_total_workers
%chunk_size = ceildiv(%N, %num_workers)
%worker_id = arts.get_worker_id
%chunk_start = %worker_id * %chunk_size
%chunk_end = min(%chunk_start + %chunk_size, %N)
scf.for %i = %chunk_start to %chunk_end {
  // loop body
}
```

---

### Stage 11: concurrency-opt

**Purpose:** Optimize concurrent execution with DB partitioning and twin-diff analysis.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=concurrency-opt
```

**Debug Command:**
```bash
carts run <file>.mlir --stop-after=concurrency-opt --debug-only=db,db_partitioning 2>&1
```

**Passes Executed:**
- `DeadCodeElimination` - Remove dead code
- `Edt` - Convert parallel EDTs to single EDTs
- `Db` - Re-optimize DataBlocks
- `DbPartitioning` - Partition DBs for fine-grained parallel access
- `PolygeistCanonicalize`, `CSE`, `Mem2Reg` - Cleanup passes

**Partitioning Decision (Heuristics):**

| Heuristic | Condition | Result |
|-----------|-----------|--------|
| H1 | Read-only on single node | Keep coarse-grained |
| H2 | Cost model (innerBytes vs minInnerBytes) | Partition if beneficial |
| Stencil | Detected stencil pattern | Generate bounds checks |

**Twin-Diff Policy:**
- **DEFAULT:** `twin_diff = TRUE` (safe, handles potential overlap)
- **DISABLE:** Only when PROVEN non-overlapping:
  1. Fine-grained allocation with DbControlOps from OpenMP depend clauses
  2. Successful partitioning with proven disjoint acquires
  3. DbAliasAnalysis proving disjoint acquires

**Chunked Access Pattern:**
```mlir
// Global index → chunk index + local index
%chunk_idx = arith.divui %global_idx, %chunk_size
%local_idx = arith.remui %global_idx, %chunk_size
%ref = arts.db_ref %ptr[%chunk_idx] : memref<?xmemref<T>> -> memref<T>
memref.load/store %ref[%local_idx]
```

---

### Stage 12: epochs

**Purpose:** Create epoch synchronization for EDT groups.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=epochs
```

**Passes Executed:**
- `PolygeistCanonicalize` - Canonicalize operations
- `CreateEpochs` - Create ARTS epochs for synchronization

---

### Stage 13: pre-lowering

**Purpose:** Final transformations before LLVM conversion.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=pre-lowering
```

**Debug Command:**
```bash
carts run <file>.mlir --stop-after=pre-lowering --debug-only=parallel_edt_lowering,db_lowering,edt_lowering,epoch_lowering,arts_data_pointer_hoisting 2>&1
```

**Passes Executed:**
- `ParallelEdtLowering` - Lower parallel EDTs to task graphs
- `DbLowering` - Lower DataBlocks to opaque pointers
- `EdtLowering` - Lower EDTs to runtime function calls
- `DataPointerHoisting` - Hoist data pointer loads out of loops
- `ScalarReplacement` - Memory to register promotion for reductions
- `EpochLowering` - Lower epochs and propagate GUIDs

**EDT Lowering Steps:**
1. Analyze EDT region for free variables and dependencies
2. Outline EDT region to function with ARTS runtime signature
3. Insert parameter packing before EDT
4. Insert parameter/dependency unpacking in outlined function
5. Replace EDT with `artsEdtCreate` call returning GUID
6. Add dependency management (`artsRecordInDep`, `artsSignalEdtNull`)

---

### Stage 14: arts-to-llvm

**Purpose:** Final conversion of ARTS dialect to LLVM dialect.

**Run Command:**
```bash
carts run <file>.mlir --stop-after=arts-to-llvm
# or for complete pipeline:
carts run <file>.mlir
```

**Debug Command:**
```bash
carts run <file>.mlir --debug-only=convert_arts_to_llvm 2>&1
```

**Passes Executed:**
- `ConvertArtsToLLVM` - Convert ARTS operations to LLVM
- `PolygeistCanonicalize`, `CSE`, `Mem2Reg` - Cleanup passes
- `ControlFlowSink` - Sink control flow operations

---

### Post-Pipeline: emit-llvm

**Purpose:** Generate LLVM IR with performance hints.

**Run Command:**
```bash
carts run <file>.mlir --emit-llvm
```

**Debug Command:**
```bash
carts run <file>.mlir --emit-llvm --debug-only=arts_alias_scope_gen,arts_loop_vectorization_hints,arts_prefetch_hints 2>&1
```

**Passes Executed:**
- `ConvertPolygeistToLLVM` - Final Polygeist lowering
- `AliasScopeGen` - Generate LLVM alias scope metadata
- `LoopVectorizationHints` - Attach vectorization hints
- `PrefetchHints` - Insert software prefetch intrinsics

**AliasScopeGen:**
- Creates unique alias scopes for each data array
- Enables LLVM vectorizer to prove non-aliasing
- Sets alignment=8 on data pointer loads

**LoopVectorizationHints:**
- Auto-detects vector width (8 for f32, 4 for f64)
- Creates `!llvm.loop` metadata for innermost loops
- Adds `llvm.assume` for non-negative loop bounds

**PrefetchHints:**
- Detects strided memory access patterns
- Inserts `llvm.prefetch` for large-stride accesses (>64 bytes)
- Prefetch distance based on stride size:
  - >=4KB: 2 iterations ahead
  - 1-4KB: 4 iterations ahead
  - 128B-1KB: 6-10 iterations ahead

---

## Pass-Level Debug Reference

### Complete Pass Table

| Pass | DEBUG_TYPE | Stage | Purpose |
|------|-----------|-------|---------|
| CollectMetadata | `collect_metadata` | 2 | Extract loop/array metadata |
| ConvertOpenMPToArts | `convert_openmp_to_arts` | 4 | OMP → ARTS transformation |
| CanonicalizeMemrefs | `canonicalize_memrefs` | 1 | Normalize memref operations |
| LoopReordering | `loop_reordering` | 6 | Cache-optimal loop order |
| CreateDbs | `create_dbs` | 7 | Create DataBlock allocations |
| Db | `db` | 8,11 | Adjust DB modes |
| DbPartitioning | `db_partitioning` | 11 | Partition for parallelism |
| Edt | `edt` | 5,9,11 | EDT analysis/optimization |
| EdtInvariantCodeMotion | `edt_invariant_code_motion` | 5 | Hoist invariant code |
| EdtPtrRematerialization | `edt_ptr_rematerialization` | 5 | Optimize pointer deps |
| ArtsLoopFusion | `arts_loop_fusion` | 9 | Fuse independent loops |
| Concurrency | `concurrency` | 10 | Build concurrency graph |
| ForLowering | `for_lowering` | 10 | Lower arts.for to chunks |
| CreateEpochs | `create_epochs` | 12 | Create epoch sync |
| ParallelEdtLowering | `parallel_edt_lowering` | 13 | Lower parallel EDTs |
| DbLowering | `db_lowering` | 13 | Lower DBs to pointers |
| EdtLowering | `edt_lowering` | 13 | Lower EDTs to runtime |
| EpochLowering | `epoch_lowering` | 13 | Lower epochs |
| DataPointerHoisting | `arts_data_pointer_hoisting` | 13 | Hoist pointer loads |
| ScalarReplacement | `scalar_replacement` | 13 | Mem2reg for reductions |
| ConvertArtsToLLVM | `convert_arts_to_llvm` | 14 | Final ARTS lowering |
| AliasScopeGen | `arts_alias_scope_gen` | emit-llvm | Alias metadata |
| LoopVectorizationHints | `arts_loop_vectorization_hints` | emit-llvm | Loop hints |
| PrefetchHints | `arts_prefetch_hints` | emit-llvm | Prefetch intrinsics |
| DeadCodeElimination | `dce` | various | Remove dead code |

### Debug Output Color Coding

All passes use the ARTS debug infrastructure with color-coded output:

| Color | Level | Meaning |
|-------|-------|---------|
| Blue (bold) | INFO | High-level progress messages |
| Magenta (bold) | DEBUG | Detailed transformation steps |
| Yellow (bold) | WARN | Potential issues |
| Red (bold) | ERROR | Errors (always printed) |

### Example Debug Session

```bash
# Debug loop reordering decisions
carts run gemm.mlir --stop-after=loop-reordering --debug-only=loop_reordering 2>&1 | tee debug.log

# Debug DB partitioning decisions
carts run gemm.mlir --stop-after=concurrency-opt --debug-only=db,db_partitioning 2>&1 | tee debug.log

# Debug multiple passes
carts run gemm.mlir --debug-only=loop_reordering,create_dbs,db_partitioning 2>&1 | tee debug.log

# Debug post-lowering optimizations
carts run gemm.mlir --emit-llvm --debug-only=arts_alias_scope_gen,arts_loop_vectorization_hints,arts_prefetch_hints 2>&1 | tee debug.log
```

---

## Common Issues & Troubleshooting

### 1. Switch Statements Not Supported

**Symptom:** Error during OpenMP-to-ARTS conversion mentioning multiple basic blocks.

**Cause:** Switch statements create multiple basic blocks in loop bodies, violating `scf.for` structure requirements.

**Fix:** Replace switch with separate inline functions or if-else chains.

**Example:** SPECFEM3D stress/velocity kernels

---

### 2. Collapse Clause Not Supported

**Symptom:** Error during OpenMP processing.

**Cause:** `#pragma omp ... collapse(N)` is not yet supported.

**Fix:** Remove `collapse(N)` from pragmas and manually restructure loops if needed.

**Example:** SW4Lite rhs4sg-base, vel4sg-base

---

### 3. Early Returns Not Supported

**Symptom:** Error about control flow in loop body.

**Cause:** Early return statements inside parallel loops violate structured control flow requirements.

**Fix:** Restructure to avoid early returns; use guard variables or restructure conditionals.

---

### 4. SSA Dominance Violations

**Symptom:** `db_free` placed outside loop where `db_alloc` is inside, causing SSA dominance error.

**Cause:** Control flow analysis places free in wrong scope.

**Fix:** Ensure `db_free` is in same block as `db_alloc`.

**Example:** SeisSol volume-integral

---

### 5. Pointer Swap Pattern Bugs

**Symptom:** Double-buffering with pointer swap doesn't work correctly.

**Cause:** Circular alias relationships in wrapper allocas confuse canonicalization.

**Fix:** Use explicit double-buffering without pointer swap:
```c
// Instead of swapping pointers
// Use explicit indexing: buffer[iter % 2] and buffer[(iter + 1) % 2]
```

**Example:** PolyBench jacobi2d

---

### 6. Checksum Verification Failures

**Symptom:** Checksum differs between sequential and ARTS execution.

**Cause:** For normalized data (like in layernorm/batchnorm), `sum(x)` produces near-zero values.

**Fix:** Use `sum(|x|)` instead of `sum(x)` for checksums:
```c
// Instead of: checksum += data[i];
checksum += fabs(data[i]);
```

**Example:** ML-kernels layernorm, batchnorm, activations

---

### 7. Large Dataset Timeouts

**Symptom:** Compilation or execution takes extremely long.

**Cause:** Element-wise allocation creating millions of tiny datablocks.

**Fix:** Use chunked allocation with div/mod index transformation:
```mlir
// Fine-grained (slow for large N)
%db = arts.db_alloc ... { outer_dims = [0], sizes = [%N] }

// Chunked (efficient)
%num_chunks = arith.divui %N, %chunk_size
%db = arts.db_alloc ... { outer_dims = [0], sizes = [%num_chunks] }
```

**Example:** ML-kernels activations (large dataset)

---

### 8. Makefile Naming Mismatch

**Symptom:** Build errors or wrong file compiled.

**Cause:** `EXAMPLE_NAME` doesn't match source file basename.

**Fix:** Ensure `EXAMPLE_NAME` in Makefile matches the `.c` file name exactly.

---

### 9. Loop Reordering Breaks Chunk Computation

**Symptom:** Wrong results after enabling loop reordering.

**Cause:** Loop reordering ran AFTER create-dbs, breaking SSA value relationships.

**Fix:** Ensure loop-reordering stage runs BEFORE create-dbs (this is the default order).

---

### 10. Missing Debug Output

**Symptom:** `--debug-only=<pass>` produces no output.

**Cause:** Debug output goes to stderr, not stdout.

**Fix:** Redirect stderr: `carts run ... 2>&1 | tee debug.log`

---

## Example Workflows

### Workflow 1: Analyze a New Benchmark

```bash
# Navigate to benchmark
cd /path/to/benchmark

# Build CARTS
carts build

# Step 1: Generate sequential MLIR for metadata
carts cgeist example.c -O0 --print-debug-info -S --raise-scf-to-affine -o example_seq.mlir

# Step 2: Collect metadata
carts run example_seq.mlir --collect-metadata

# Step 3: Generate parallel MLIR
carts cgeist example.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine -o example.mlir

# Step 4: Inspect each pipeline stage
carts run example.mlir --stop-after=canonicalize-memrefs -o stages/01_canonicalize.mlir
carts run example.mlir --stop-after=openmp-to-arts -o stages/04_openmp_to_arts.mlir
carts run example.mlir --stop-after=create-dbs -o stages/07_create_dbs.mlir
carts run example.mlir --stop-after=concurrency-opt -o stages/11_concurrency_opt.mlir

# Or dump all stages at once
carts run example.mlir --all-stages -o stages/

# Step 5: Debug specific pass if needed
carts run example.mlir --stop-after=create-dbs --debug-only=create_dbs 2>&1 | tee debug_createdb.log

# Step 6: Full execution
carts execute example.c -O3
```

### Workflow 2: Debug DB Partitioning Decision

```bash
# Generate MLIR
carts cgeist example.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine -o example.mlir

# Stop at concurrency-opt and debug
carts run example.mlir --stop-after=concurrency-opt --debug-only=db,db_partitioning 2>&1 | tee db_debug.log

# Look for partitioning decisions in output:
# [INFO] [db_partitioning] Analyzing datablock partitioning...
# [DEBUG] [db_partitioning] H1 heuristic: read-only, keeping coarse
# [DEBUG] [db_partitioning] H2 heuristic: cost model suggests partition

# Inspect the MLIR to see allocation strategy
grep -A2 "arts.db_alloc" example_concurrency-opt.mlir
```

### Workflow 3: Debug Loop Vectorization

```bash
# Generate LLVM IR with hints
carts run example.mlir --emit-llvm --debug-only=arts_loop_vectorization_hints,arts_prefetch_hints 2>&1 | tee vec_debug.log

# Look for:
# [DEBUG] [arts_loop_vectorization_hints] Auto-detected vector width: 8
# [DEBUG] [arts_loop_vectorization_hints] Unroll count: 4 (loads=16)
# [DEBUG] [arts_prefetch_hints] Found 3 strided accesses
# [INFO] [arts_prefetch_hints] Inserted 3 prefetches

# Inspect the generated LLVM IR
grep -A5 "llvm.loop" example.ll
grep "llvm.prefetch" example.ll
```

### Workflow 4: Compare Sequential vs ARTS Execution

```bash
# Run sequential version
gcc -O3 example.c -o example_seq
./example_seq > seq_output.txt

# Run ARTS version
carts execute example.c -O3 -o example_arts
./example_arts > arts_output.txt

# Compare outputs
diff seq_output.txt arts_output.txt

# If different, debug with diagnostics
carts execute example.c -O3 --diagnose --diagnose-output=diag.json
```

### Workflow 5: Full Pipeline Inspection

```bash
# Dump all 14 stages + LLVM IR
carts run example.mlir --all-stages -o stages/

# Files generated:
# stages/example_canonicalize-memrefs.mlir
# stages/example_collect-metadata.mlir
# stages/example_initial-cleanup.mlir
# stages/example_openmp-to-arts.mlir
# stages/example_edt-transforms.mlir
# stages/example_loop-reordering.mlir
# stages/example_create-dbs.mlir
# stages/example_db-opt.mlir
# stages/example_edt-opt.mlir
# stages/example_concurrency.mlir
# stages/example_concurrency-opt.mlir
# stages/example_epochs.mlir
# stages/example_pre-lowering.mlir
# stages/example_arts-to-llvm.mlir
# stages/example.ll

# Compare transformation between stages
diff stages/example_create-dbs.mlir stages/example_concurrency-opt.mlir
```

---

## Reference Files

- **Pipeline Definition:** `tools/run/carts-run.cpp`
- **CLI Interface:** `tools/carts_cli.py`
- **Pass Registry:** `include/arts/Passes/ArtsPasses.td`
- **Pass Headers:** `include/arts/Passes/ArtsPasses.h`
- **Debug Macros:** `include/arts/Utils/ArtsDebug.h`
- **Pass Implementations:** `lib/arts/Passes/*.cpp`

---

## Version Information

This guide covers CARTS with the following features:
- 14 pipeline stages
- 27+ optimization passes
- New passes: LoopReordering, PrefetchHints, AliasScopeGen, LoopVectorizationHints
- Dual-compilation metadata strategy
- Twin-diff runtime support
