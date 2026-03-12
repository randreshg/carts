# CARTS Analysis Guide

This guide teaches agents and developers how to understand, run, and debug CARTS optimizations. It covers the complete pipeline from C/C++ source to ARTS runtime executable.

## Table of Contents

1. [Quick Reference Card](#quick-reference-card)
2. [Pipeline Overview](#pipeline-overview)
3. [Pipeline Stages](#pipeline-stages)
4. [Partition Mode Algorithm](#partition-mode-algorithm)
5. [Pass-Level Debug Reference](#pass-level-debug-reference)
6. [Common Issues & Troubleshooting](#common-issues--troubleshooting)
7. [Example Workflows](#example-workflows)
8. [Distributed Runtime Debug](#distributed-runtime-debug)

---

## Quick Reference Card

### Essential Commands

```bash
# Build CARTS
carts build

# Build ARTS runtime with full debug logging
carts build --arts --debug 3

# Generate MLIR from C/C++
carts cgeist <file>.c -O0 --print-debug-info -S --raise-scf-to-affine           # Sequential
carts cgeist <file>.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine  # Parallel

# Run pipeline (stop at specific stage)
carts compile <file>.mlir --stop-at=<stage>

# Run pipeline (dump all intermediate stages)
carts compile <file>.mlir --all-stages -o ./stages/

# Collect metadata (dual-compilation)
carts compile <file>_seq.mlir --collect-metadata

# Debug a specific pass
carts compile <file>.mlir --stop-at=<stage> --debug-only=<pass> 2>&1

# Full compilation and execution
carts compile <file>.c -O3

# Full compilation with diagnostics
carts compile <file>.c -O3 --diagnose

# Run focused lit regressions with the bundled toolchain
carts lit tests/contracts/for_dispatch_clamp.mlir
carts lit --suite contracts
```

### Pipeline Stage Names (for --stop-at)

| Stage | Name                   | Purpose                      |
|-------|------------------------|------------------------------|
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
| 11 | `edt-distribution` | Distribution selection + for lowering |
| 12 | `concurrency-opt` | DB partitioning |
| 13 | `epochs` | Epoch synchronization |
| 14 | `pre-lowering` | Prepare for LLVM |
| 15 | `arts-to-llvm` | Final LLVM conversion |

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

```mermaid
flowchart TB
  SRC["C/C++ Source (.c/.cpp)"]
  CG["carts cgeist<br/>Polygeist frontend"]

  subgraph MLIR["MLIR pipeline"]
    direction TB

    subgraph P1["Phase 1: Normalization & Analysis"]
      direction TB
      S1["1) canonicalize-memrefs<br/>Normalize memref operations"]
      S2["2) collect-metadata<br/>Extract loop/array metadata"]
      S3["3) initial-cleanup<br/>Dead code elimination"]
      S1 --> S2 --> S3
    end

    subgraph P2["Phase 2: OpenMP → ARTS transformation"]
      direction TB
      S4["4) openmp-to-arts<br/>OMP parallel → ARTS EDTs"]
      S5["5) edt-transforms<br/>EDT structure optimization"]
      S6["6) loop-reordering<br/>Cache-optimal loop order"]
      S4 --> S5 --> S6
    end

    subgraph P3["Phase 3: DB management"]
      direction TB
      S7["7) create-dbs<br/>Create DataBlock allocations"]
      S8["8) db-opt<br/>Optimize DB access modes"]
      S9["9) edt-opt<br/>EDT fusion & optimization"]
      S7 --> S8 --> S9
    end

    subgraph P4["Phase 4: Concurrency & synchronization"]
      direction TB
      S10["10) concurrency<br/>Build EDT concurrency graph"]
      S11["11) edt-distribution<br/>Distribution selection + ForLowering"]
      S12["12) concurrency-opt<br/>DB partitioning"]
      S13["13) epochs<br/>Epoch synchronization"]
      S10 --> S11 --> S12 --> S13
    end

    subgraph P5["Phase 5: Lowering to LLVM"]
      direction TB
      S14["14) pre-lowering<br/>EDT/DB/Epoch lowering"]
      S15["15) arts-to-llvm<br/>Final ARTS → LLVM conversion"]
      S14 --> S15
    end

    P1 --> P2 --> P3 --> P4 --> P5
  end

  EMIT["--emit-llvm"]

  subgraph POST["Post-lowering optimizations"]
    direction TB
    O1["AliasScopeGen<br/>LLVM alias metadata for vectorizer"]
    O2["LoopVectorizationHints<br/>LLVM loop hints (!llvm.loop)"]
    O3["PrefetchHints<br/>Software prefetch intrinsics"]
    O1 --> O2 --> O3
  end

  OUT["LLVM IR (.ll) → Executable"]

  SRC --> CG --> MLIR --> EMIT --> POST --> OUT
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
carts compile <file>.mlir --stop-at=canonicalize-memrefs
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
carts compile <file>_seq.mlir --collect-metadata
```

**Debug Command:**
```bash
carts compile <file>_seq.mlir --collect-metadata --debug-only=collect_metadata 2>&1
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

```text
[DEBUG] [collect_metadata] Collecting loop metadata...
[DEBUG] [collect_metadata] Collecting memref metadata...
```

---

### Stage 3: initial-cleanup

**Purpose:** Remove dead code and simplify control flow after metadata collection.

**Run Command:**
```bash
carts compile <file>.mlir --initial-cleanup
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
carts compile <file>.mlir --stop-at=openmp-to-arts
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=openmp-to-arts --debug-only=convert_openmp_to_arts 2>&1
```

**Passes Executed:**
- `ConvertOpenMPToArts` - Transform OMP parallel regions to ARTS EDTs
- `DeadCodeElimination` - Remove dead OMP dependency proxies
- `CSE` - Common subexpression elimination

**Transformations:**

| OpenMP            | ARTS                          |
|-------------------|-------------------------------|
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
carts compile <file>.mlir --stop-at=edt-transforms
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=edt-transforms --debug-only=edt 2>&1
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
carts compile <file>.mlir --stop-at=loop-reordering
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=loop-reordering --debug-only=loop_reordering,kernel_transforms 2>&1
```

**Passes Executed:**
- `LoopReordering` - Apply loop interchange transformations
- `ArtsKernelTransforms` - Kernel-form transforms such as matmul reduction
  distribution
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
carts compile <file>.mlir --stop-at=create-dbs
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=create-dbs --debug-only=create_dbs 2>&1
```

**Passes Executed:**
- `CreateDbs` - Identify external allocations, create DB operations
- `PolygeistCanonicalize` - Canonicalize polygeist operations
- `CSE`, `SymbolDCE`, `Mem2Reg` - Cleanup passes

**Allocation Strategies:**

| Strategy | Outer Dims | Use Case |
|----------|-----------|----------|
| Coarse-grained | `sizes=[1]` | Single DB for entire array |
| Fine-grained | `sizes=[N]` | One DB per element |
| Blocked | `sizes=[numChunks]` | Multiple DBs matching workers |

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
carts compile <file>.mlir --stop-at=db-opt
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=db-opt --debug-only=db 2>&1
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
carts compile <file>.mlir --stop-at=edt-opt
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=edt-opt --debug-only=edt,arts_loop_fusion 2>&1
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

**Purpose:** Build EDT concurrency graph and apply pre-lowering `arts.for` hints.

**Run Command:**
```bash
carts compile <file>.mlir --stop-at=concurrency
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=concurrency --debug-only=concurrency,arts_for_optimization 2>&1
```

**Passes Executed:**
- `Concurrency` - Build EDT concurrency graph
- `ArtsForOptimization` - Add access-pattern + partitioning hints
- `PolygeistCanonicalize` - Cleanup pass

---

### Stage 11: edt-distribution

**Purpose:** Select distribution strategy and lower `arts.for` loops.

**Reference Doc:**
- `docs/heuristics/distribution/distribution.md`

**Run Command:**
```bash
carts compile <file>.mlir --stop-at=edt-distribution
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=edt-distribution --debug-only=edt_distribution,for_lowering 2>&1
```

**Passes Executed:**
- `EdtDistribution` - Annotate strategy (`distribution_kind`, `distribution_pattern`)
- `ForLowering` - Lower `arts.for` into task EDT work chunks

---

### Stage 12: concurrency-opt

**Purpose:** Optimize concurrent execution with DB partitioning and concurrency rewrites.

**Reference Doc:**
- `docs/heuristics/partitioning/partitioning.md`

**Run Command:**
```bash
carts compile <file>.mlir --stop-at=concurrency-opt
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=concurrency-opt --debug-only=db,db_partitioning 2>&1
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
| H1.1 | Read-only single-node, no partition capability | **Coarse** |
| H1.1b | Read-only + all block-capable acquires are full-range | **Coarse** |
| H1.2 | Mixed direct writes + indirect reads | **Block** (indirect uses full-range) |
| H1.2b | Mixed direct + indirect writes | **Block** (indirect uses full-range) |
| H1.3 | Stencil or indexed patterns | **Stencil** (if supported) or **Block/ElementWise** |
| H1.4 | Uniform direct access | **Block** |
| H1.5 | Multi-node system | **Block** if supported, else **ElementWise** |
| H1.6 | Non-uniform, no capability | **Coarse** |
| H1.7 | Per-acquire offset mismatch | **Full-range** acquire (per-acquire only) |

**Blocked Access Pattern:**
```mlir
// Global index → chunk index + local index
%chunk_idx = arith.divui %global_idx, %block_size
%local_idx = arith.remui %global_idx, %block_size
%ref = arts.db_ref %ptr[%chunk_idx] : memref<?xmemref<T>> -> memref<T>
memref.load/store %ref[%local_idx]
```

---

## Partition Mode Algorithm

This section provides a comprehensive guide to how CARTS determines the partitioning strategy for DataBlocks. The algorithm spans three compilation stages and involves capability analysis, heuristic decision-making, and IR transformation.

### Overview

Partition mode determines how a DataBlock's data is distributed:

| Mode | Allocation Structure | Use Case |
|------|---------------------|----------|
| **Coarse** | `sizes=[1], elementSizes=[N]` | Single DB for entire array |
| **Blocked** | `sizes=[numChunks], elementSizes=[blockSize]` | Loop-based parallel access |
| **ElementWise** | `sizes=[N], elementSizes=[1]` | Fine-grained from depend clauses |
| **Stencil** | `sizes=[numChunks], elementSizes=[blockSize+halo]` | Stencil patterns with halos |

### Master Flowchart: Three-Stage Partition Mode Assignment

```mermaid
flowchart TB
    subgraph Stage7["Stage 7: CreateDbs"]
        direction TB
        S7_1[Identify memrefs escaping to parallel EDTs]
        S7_2{Has DbControlOps<br/>from depend clauses?}
        S7_3[Extract indices from<br/>depend clause patterns]
        S7_4{Indices match<br/>access patterns?}
        S7_5["Set partition=fine_grained<br/>canElementWise=true"]
        S7_6{Has chunk offsets/sizes<br/>in depend clause?}
        S7_7["Set partition=blocked<br/>canBlocked=true"]
        S7_8["Set partition=coarse<br/>default"]
        S7_9[Create DbAllocOp + DbAcquireOps]

        S7_1 --> S7_2
        S7_2 -->|Yes| S7_3
        S7_2 -->|No| S7_8
        S7_3 --> S7_4
        S7_4 -->|Yes| S7_5
        S7_4 -->|No| S7_6
        S7_5 --> S7_9
        S7_6 -->|Yes| S7_7
        S7_6 -->|No| S7_8
        S7_7 --> S7_9
        S7_8 --> S7_9
    end

    subgraph Stage11["Stage 11: EdtDistribution + ForLowering"]
        direction TB
        S11_1[Find arts.for inside parallel EDT]
        S11_2[Classify pattern and select distribution_kind]
        S11_3[Annotate distribution attributes on arts.for]
        S11_4[Lower to worker task EDTs]
        S11_5[Clone DbAcquireOp for worker task]

        S11_1 --> S11_2 --> S11_3 --> S11_4 --> S11_5
    end

    subgraph Stage12["Stage 12: DbPartitioning"]
        direction TB
        S12_1[For each DbAllocOp]
        S12_2{Already partitioned?<br/>partition != coarse}
        S12_3[Skip - keep existing]
        S12_4[Gather all child DbAcquireOps]
        S12_5[Build PartitioningContext<br/>from per-acquire analysis]
        S12_6[Query Heuristics H1.1-H1.6]
        S12_7[Apply DbRewriter]
        S12_8[Update allocation sizes]

        S12_1 --> S12_2
        S12_2 -->|Yes| S12_3
        S12_2 -->|No| S12_4
        S12_4 --> S12_5 --> S12_6 --> S12_7 --> S12_8
    end

    Stage7 --> Stage11 --> Stage12
```

---

### Stage 7: CreateDbs - Initial Partition Mode

CreateDbs analyzes OpenMP `depend` clauses (converted to DbControlOps) to determine initial partitioning capability.

#### DbControlOp Analysis

```c
// OpenMP source with depend clause
#pragma omp task depend(inout: A[i])
{
    A[i] = compute(A[i]);
}
```

Becomes:
```mlir
// DbControlOp carries the index pattern
arts.db_control %A_ptr mode(inout) indices(%i)
```

#### CreateDbs Decision Tree

```mermaid
flowchart TB
    START[DbControlOps collected for memref]

    Q1{All accesses have<br/>consistent indices?}
    Q2{Pinned dim count > 0?}
    Q3{Has chunk offsets/sizes?}

    R1["partition = fine_grained<br/>canElementWise = true"]
    R2["partition = blocked<br/>canBlocked = true"]
    R3["partition = coarse<br/>no structural info"]

    START --> Q1
    Q1 -->|Yes| Q2
    Q1 -->|No| R3
    Q2 -->|Yes| R1
    Q2 -->|No| Q3
    Q3 -->|Yes| R2
    Q3 -->|No| R3
```

**Key Code Location**: `lib/arts/Passes/Transformations/CreateDbs.cpp:595-750`

```cpp
// Build PartitioningContext from DbControlOp analysis
ctx.canElementWise = !isRankZero && accessPatternInfo.isConsistent &&
                     accessPatternInfo.pinnedDimCount > 0 &&
                     accessPatternInfo.allAccessesHaveIndices;
ctx.canBlocked = !isRankZero && accessPatternInfo.hasChunkDeps &&
                 accessPatternInfo.blockSizesAreConsistent;

// Set partition attribute on DbAllocOp
PromotionMode promotionMode = decision.isBlocked() ? PromotionMode::blocked
                              : decision.isFineGrained() ? PromotionMode::fine_grained
                              : PromotionMode::coarse;
setPartitionMode(dbAllocOp, promotionMode);
```

---

### Stage 11 Detail: EdtDistribution + ForLowering

The distribution stage is split into two responsibilities:
- `EdtDistributionPass` classifies each `arts.for` pattern and sets typed distribution attributes.
- `ForLowering` consumes those attributes and lowers loops by delegating to strategy-specific task lowerings.

#### Distribution-Lowering Mechanism

```mermaid
flowchart LR
    subgraph Before["After Concurrency Stage"]
        FOR["arts.for ..."]
    end

    subgraph Dist["EdtDistributionPass"]
        PLAN["Set distribution attrs<br/>kind + pattern + version"]
    end

    subgraph Lower["ForLowering + strategy helper"]
        TASK["Create worker EDTs"]
        ACQ["Rewrite DbAcquire ranges<br/>through AcquireRewritePlanning"]
    end

    Before --> Dist --> Lower
```

#### Attribute Contract

`EdtDistributionPass` annotates each `arts.for` with the distribution plan:
- `distribution_kind` (`block`, `block_cyclic`, `tiling_2d`)
- `distribution_pattern` (e.g., `uniform`, `stencil`, `triangular`, `matmul`)
- `distribution_version` (typed contract/versioning for downstream passes)

#### Lowering Contract

`ForLowering` is orchestration-only:
- resolve worker topology through `DistributionHeuristics`
- select `EdtTaskLoopLowering` implementation from `distribution_kind`
- call acquire rewrite planning (`AcquireRewritePlanning`) and apply rewrites
- keep strategy-specific bounds and acquire math in strategy classes, not in the pass body

**Fallback behavior**:
- missing/invalid distribution attrs fall back to `block` strategy
- `--partition-fallback` still controls coarse vs element-wise behavior inside DbPartitioning for unsupported/non-affine cases

---

### Stage 12: DbPartitioning - Final Decision Algorithm

DbPartitioning is the final decision point. It analyzes all acquires for an allocation and applies heuristics to choose the optimal partitioning.

#### Per-Acquire Analysis Flowchart

```mermaid
flowchart TB
    START[For each DbAcquireOp]

    Q1{Has partition attribute<br/>from CreateDbs/lowered acquires?}
    Q2{partition == blocked?}
    Q3{Offset depends on<br/>loop IV?}
    Q4{partition == fine_grained?}

    R1["thisAcquireCanBlocked = true<br/>Range-local blocked acquire"]
    R2["thisAcquireCanBlocked = true<br/>needsFullRange = true<br/>indirect access"]
    R3["thisAcquireCanElementWise = true"]
    R4["Neither capability<br/>coarse fallback"]

    CHECK["Validate with canPartitionWithOffset()"]

    START --> Q1
    Q1 -->|Yes| Q2
    Q1 -->|No| R4
    Q2 -->|Yes| CHECK
    Q2 -->|No| Q4
    CHECK --> Q3
    Q3 -->|Yes| R1
    Q3 -->|No| R2
    Q4 -->|Yes| R3
    Q4 -->|No| R4
```

#### `canPartitionWithOffset()` - IV Dependence Check

This critical function determines if an acquire's access pattern is derived from the partition offset:

```cpp
// DbAcquireNode.cpp:923-1001
bool DbAcquireNode::canPartitionWithOffset(Value offset) {
    // Zero offset always valid (full array access)
    if (ValueUtils::isZeroConstant(offset))
        return true;

    // Collect all memory access operations
    DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
    collectAccessOperations(dbRefToMemOps);

    for (auto &[dbRef, memOps] : dbRefToMemOps) {
        for (Operation *memOp : memOps) {
            Value accessIndex = getFirstAccessIndex(memOp);

            // Check if access index is derived from partition offset
            // e.g., A[chunk_offset + local_idx] → offset appears in access
            if (!isIndexDerivedFromOffset(accessIndex, offset))
                return false;
        }
    }
    return true;
}
```

**What This Validates**:
- If offset = `%chunk_start` and access is `A[%chunk_start + %local]` → **Valid** (IV-dependent)
- If offset = `%chunk_start` but access is `A[nodelist[%i]]` → **Invalid** (indirect, needs full-range)
- If offset appears with a constant stride (`offset * C`) or in a non-leading
  dimension → **Invalid** (unsafe for blocked partitioning)

#### Building PartitioningContext

```cpp
// DbPartitioning.cpp:970-1030
for each DbAcquireNode {
    // Read partition attribute from acquire (set by CreateDbs and lowering rewriters)
    auto acquireMode = getPartitionMode(acquire.getOperation());

    bool thisAcquireCanBlocked =
        acquireMode && *acquireMode == PromotionMode::blocked;
    bool thisAcquireCanElementWise =
        acquireMode && *acquireMode == PromotionMode::fine_grained;

    // Validate blocked capability with IV dependence check
    if (thisAcquireCanBlocked) {
        Value offsetForCheck = getPartitionOffset(acqNode, &acqInfo);
        if (offsetForCheck && !acqNode->canPartitionWithOffset(offsetForCheck)) {
            if (hasIndirectAccess) {
                // Indirect access: still blocked but needs full-range
                thisAcquireCanBlocked = true;  // allocation blocked
                needsFullRange = true;          // this acquire gets all chunks
            } else if (acqNode->hasLoads() && !acqNode->hasStores()) {
                // Read-only direct access with mismatched offset: full-range
                thisAcquireCanBlocked = true;
                needsFullRange = true;
            } else {
                // Direct access but offset not derived from access (writes)
                thisAcquireCanBlocked = false;  // fall back to coarse
            }
        }
    }

    // Build AcquireInfo for heuristic voting
    AcquireInfo info;
    info.canBlocked = thisAcquireCanBlocked;
    info.canElementWise = thisAcquireCanElementWise;
    ctx.acquires.push_back(info);
}

// Aggregate capabilities via voting
ctx.canBlocked = ctx.anyCanBlocked();
ctx.canElementWise = ctx.anyCanElementWise();
```

---

### Heuristics Decision Flow

```mermaid
flowchart TB
    CTX[PartitioningContext built]

    subgraph Heuristics["Heuristic Evaluation (Order in code)"]
        H11["H1.1: Read-only single-node, no capability"]
        H11b["H1.1b: Read-only + all full-range block acquires"]
        H12["H1.2: Mixed direct writes + indirect reads"]
        H12b["H1.2b: Mixed direct + indirect writes"]
        H13["H1.3: Stencil / indexed patterns"]
        H14["H1.4: Uniform direct access"]
        H15["H1.5: Multi-node"]
        H16["H1.6: Non-uniform + no capability"]
        FALLBACK["Fallback: user preference"]

        H11 --> H11b --> H12 --> H12b --> H13 --> H14 --> H15 --> H16 --> FALLBACK
    end

    CTX --> Heuristics

    Heuristics --> DECISION[PartitioningDecision]
```

#### Heuristics Summary Table

| ID | Condition | Result |
|----|-----------|--------|
| H1.1 | Read-only single-node, no partition capability | **Coarse** |
| H1.1b | Read-only + all block-capable acquires are full-range | **Coarse** |
| H1.2 | Mixed direct writes + indirect reads | **Block** (indirect full-range) |
| H1.2b | Mixed direct + indirect writes | **Block** (indirect full-range) |
| H1.3 | Stencil or indexed patterns | **Stencil** (if supported), else **Block/ElementWise** |
| H1.4 | Uniform direct access | **Block** |
| H1.5 | Multi-node | **Block** if supported, else **ElementWise** |
| H1.6 | Non-uniform + no capability | **Coarse** |

**Note:** H1.7 is a per-acquire decision (full-range vs specific block). It is
computed during acquire analysis and does not change the allocation-level mode.

---

### Mixed Mode: Blocked + Full-Range Acquires

When both IV-dependent and indirect accesses exist for the same allocation:

```mermaid
flowchart TB
    subgraph Allocation["Blocked Allocation"]
        ALLOC["sizes=[numChunks]<br/>elementSizes=[blockSize]"]
    end

    subgraph Workers["Worker Tasks (IV-dependent)"]
        W1["Worker 0: acquire offset=0, size=1"]
        W2["Worker 1: acquire offset=1, size=1"]
        WN["Worker N: acquire offset=N, size=1"]
    end

    subgraph Indirect["Indirect Reader (Full-Range)"]
        IR["Indirect read: acquire offset=0, size=numChunks<br/>Access via: data[nodelist[i]]"]
    end

    Allocation --> Workers
    Allocation --> Indirect
```

**Example: LULESH-style nodelist pattern**

```c
// Worker writes (IV-dependent) → blocked with index
for (int e = chunk_start; e < chunk_end; e++) {
    elemData[e] = compute(...);  // Direct: A[chunk_start + local]
}

// Indirect reads (not IV-dependent) → full-range acquire
for (int e = 0; e < numElems; e++) {
    for (int n = 0; n < 4; n++) {
        int nodeIdx = nodelist[e*4 + n];  // Indirect index
        nodeData[nodeIdx] += ...;          // Need all of nodeData
    }
}
```

---

### Heuristics Architecture Assessment

#### Current Structure (as of this codebase)

Heuristics are centralized in the core analysis layer:

```
include/arts/Analysis/ArtsHeuristics.h   # PartitioningDecision, PartitioningContext
lib/arts/Analysis/ArtsHeuristics.cpp     # evaluatePartitioningHeuristics(...)
```

Key entry points:

- `evaluatePartitioningHeuristics(...)` returns a `PartitioningDecision`.
- `HeuristicsConfig::getPartitioningMode(...)` wraps evaluation and records
  decisions for diagnostics.
- `HeuristicsConfig::getAcquireDecisions(...)` applies H1.7 (per-acquire
  full-range decisions).

**Assessment**: The current single-function structure keeps policy clear and
traceable, while still recording decisions for diagnostics.

| Aspect | Current | Notes |
|--------|---------|-------|
| LOC | single file (`ArtsHeuristics.cpp`) | Centralized and traceable |
| Classes | none (function-based) | Simpler than class registry |
| Context fields | `PartitioningContext` | Rich enough for H1.x decisions |
| Evaluation | linear chain | Very fast |

**Why it is reasonable**:
1. **Extensibility**: Add new branches in `evaluatePartitioningHeuristics(...)`
2. **Separation**: Heuristics decide; DbPartitioning applies
3. **Diagnostics**: `recordDecision()` captures rationale for tooling

---

### Key Code References

| Stage | File | Key Lines | Purpose |
|-------|------|-----------|---------|
| CreateDbs | `lib/arts/Passes/Transformations/CreateDbs.cpp` | 595-750 | Initial partition mode |
| ForLowering | `lib/arts/Passes/Transformations/ForLowering.cpp` | task-lowering pipeline | Apply distribution strategy via helper contracts |
| DbPartitioning | `lib/arts/Passes/Optimizations/DbPartitioning.cpp` | 970-1030 | Per-acquire analysis |
| DbPartitioning | `lib/arts/Passes/Optimizations/DbPartitioning.cpp` | 1032-1061 | Context aggregation |
| IV Check | `lib/arts/Analysis/Graphs/Db/DbAcquireNode.cpp` | 923-1001 | canPartitionWithOffset |
| Heuristics | `lib/arts/Analysis/ArtsHeuristics.cpp` | policy entrypoints | getPartitioningMode + acquire decisions |

### Stage 13: epochs

**Purpose:** Create epoch synchronization for EDT groups.

**Run Command:**
```bash
carts compile <file>.mlir --stop-at=epochs
```

**Passes Executed:**
- `PolygeistCanonicalize` - Canonicalize operations
- `CreateEpochs` - Create ARTS epochs for synchronization

---

### Stage 14: pre-lowering

**Purpose:** Final transformations before LLVM conversion.

**Run Command:**
```bash
carts compile <file>.mlir --stop-at=pre-lowering
```

**Debug Command:**
```bash
carts compile <file>.mlir --stop-at=pre-lowering --debug-only=parallel_edt_lowering,db_lowering,edt_lowering,epoch_lowering,arts_data_pointer_hoisting 2>&1
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

### Stage 15: arts-to-llvm

**Purpose:** Final conversion of ARTS dialect to LLVM dialect.

**Run Command:**
```bash
carts compile <file>.mlir --stop-at=arts-to-llvm
# or for complete pipeline:
carts compile <file>.mlir
```

**Debug Command:**
```bash
carts compile <file>.mlir --debug-only=convert_arts_to_llvm 2>&1
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
carts compile <file>.mlir --emit-llvm
```

**Debug Command:**
```bash
carts compile <file>.mlir --emit-llvm --debug-only=arts_alias_scope_gen,arts_loop_vectorization_hints,arts_prefetch_hints 2>&1
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
| Db | `db` | 8,12 | Adjust DB modes |
| DbPartitioning | `db_partitioning` | 12 | Partition for parallelism |
| Edt | `edt` | 5,9,12 | EDT analysis/optimization |
| EdtInvariantCodeMotion | `edt_invariant_code_motion` | 5 | Hoist invariant code |
| EdtPtrRematerialization | `edt_ptr_rematerialization` | 5 | Optimize pointer deps |
| ArtsLoopFusion | `arts_loop_fusion` | 9 | Fuse independent loops |
| Concurrency | `concurrency` | 10 | Build concurrency graph |
| ForLowering | `for_lowering` | 11 | Lower arts.for with strategy-selected helpers |
| CreateEpochs | `create_epochs` | 13 | Create epoch sync |
| ParallelEdtLowering | `parallel_edt_lowering` | 14 | Lower parallel EDTs |
| DbLowering | `db_lowering` | 14 | Lower DBs to pointers |
| EdtLowering | `edt_lowering` | 14 | Lower EDTs to runtime |
| EpochLowering | `epoch_lowering` | 14 | Lower epochs |
| DataPointerHoisting | `arts_data_pointer_hoisting` | 14 | Hoist pointer loads |
| ScalarReplacement | `scalar_replacement` | 14 | Mem2reg for reductions |
| ConvertArtsToLLVM | `convert_arts_to_llvm` | 15 | Final ARTS lowering |
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
carts compile gemm.mlir --stop-at=loop-reordering --debug-only=loop_reordering 2>&1 | tee debug.log

# Debug DB partitioning decisions
carts compile gemm.mlir --stop-at=concurrency-opt --debug-only=db,db_partitioning 2>&1 | tee debug.log

# Debug multiple passes
carts compile gemm.mlir --debug-only=loop_reordering,create_dbs,db_partitioning 2>&1 | tee debug.log

# Debug post-lowering optimizations
carts compile gemm.mlir --emit-llvm --debug-only=arts_alias_scope_gen,arts_loop_vectorization_hints,arts_prefetch_hints 2>&1 | tee debug.log
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

**Cause:** Element-wise allocation creating millions of tiny DBs.

**Fix:** Use blocked allocation with div/mod index transformation:
```mlir
// Fine-grained (slow for large N)
%db = arts.db_alloc ... { outer_dims = [0], sizes = [%N] }

// Blocked (efficient)
%num_chunks = arith.divui %N, %block_size
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

**Fix:** Redirect stderr: `carts compile ... 2>&1 | tee debug.log`

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
carts compile example_seq.mlir --collect-metadata

# Step 3: Generate parallel MLIR
carts cgeist example.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine -o example.mlir

# Step 4: Inspect each pipeline stage
carts compile example.mlir --stop-at=canonicalize-memrefs -o stages/01_canonicalize.mlir
carts compile example.mlir --stop-at=openmp-to-arts -o stages/04_openmp_to_arts.mlir
carts compile example.mlir --stop-at=create-dbs -o stages/07_create_dbs.mlir
carts compile example.mlir --stop-at=concurrency-opt -o stages/12_concurrency_opt.mlir

# Or dump all stages at once
carts compile example.mlir --all-stages -o stages/

# Step 5: Debug specific pass if needed
carts compile example.mlir --stop-at=create-dbs --debug-only=create_dbs 2>&1 | tee debug_createdb.log

# Step 6: Full execution
carts compile example.c -O3
```

### Workflow 2: Debug DB Partitioning Decision

```bash
# Generate MLIR
carts cgeist example.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine -o example.mlir

# Stop at concurrency-opt and debug
carts compile example.mlir --stop-at=concurrency-opt --debug-only=db,db_partitioning 2>&1 | tee db_debug.log

# Look for partitioning decisions in output:
# [INFO] [db_partitioning] Analyzing DB partitioning...
# [DEBUG] [db_partitioning] H1 heuristic: read-only, keeping coarse
# [DEBUG] [db_partitioning] H2 heuristic: cost model suggests partition

# Inspect the MLIR to see allocation strategy
grep -A2 "arts.db_alloc" example_concurrency-opt.mlir
```

### Workflow 3: Debug Loop Vectorization

```bash
# Generate LLVM IR with hints
carts compile example.mlir --emit-llvm --debug-only=arts_loop_vectorization_hints,arts_prefetch_hints 2>&1 | tee vec_debug.log

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
carts compile example.c -O3 -o example_arts
./example_arts > arts_output.txt

# Compare outputs
diff seq_output.txt arts_output.txt

# If different, debug with diagnostics
carts compile example.c -O3 --diagnose --diagnose-output=diag.json
```

### Workflow 5: Full Pipeline Inspection

```bash
# Dump all stages + LLVM IR
carts compile example.mlir --all-stages -o stages/

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
# stages/example_edt-distribution.mlir
# stages/example_concurrency-opt.mlir
# stages/example_epochs.mlir
# stages/example_pre-lowering.mlir
# stages/example_arts-to-llvm.mlir
# stages/example.ll

# Compare transformation between stages
diff stages/example_create-dbs.mlir stages/example_concurrency-opt.mlir
```

## Distributed Runtime Debug

Use this workflow when multi-node runs hang, timeout, or show uneven work distribution.

### 1. Build ARTS with runtime debug enabled

```bash
# Raw ARTS runtime levels:
#   0 = errors only
#   1 = warnings
#   2 = info
#   3 = debug

carts build --arts --debug 3 --profile=profile-workload.cfg
```

### 2. Run a targeted multi-node benchmark with logs retained

```bash
carts benchmarks run polybench/2mm \
  --size small \
  --arts-config docker/arts-docker-2node.cfg \
  --nodes 2 \
  --threads 4 \
  --debug 2
```

Notes:
- `--debug 2` stores full benchmark stdout/stderr in per-run logs (`arts.log`, `omp.log`).
- Correctness verification is enabled by default (ARTS checksum compared to OpenMP).
- For `nodes > 1`, speedup is intentionally hidden as an unfair comparison.

### 3. Inspect artifacts

```bash
# Replace with your run directory
cd external/carts-benchmarks/results/<timestamp>/

# Benchmark/run logs
find . -name 'arts.log' -o -name 'omp.log'

# Counter artifacts
find . -name 'cluster.json' -o -name 'n0.json' -o -name 'n1.json'
```

### 4. Thread-count sanity for multinode runs

If runtime output shows `Number of Threads = 2` followed by `Killed`, increase threads.
Use at least 4 worker threads per node to avoid starving runtime communication/progress threads:

```bash
carts benchmarks run <bench> --nodes 2 --threads 4 --arts-config docker/arts-docker-2node.cfg
```

---

## Reference Files

- **Pipeline Definition:** `tools/run/carts-compile.cpp`
- **CLI Interface:** `tools/carts_cli.py`
- **Pass Registry:** `include/arts/Passes/ArtsPasses.td`
- **Pass Headers:** `include/arts/Passes/ArtsPasses.h`
- **Debug Macros:** `include/arts/Utils/ArtsDebug.h`
- **Pass Implementations:** `lib/arts/Passes/*.cpp`

---

## Version Information

This guide covers CARTS with the following features:
- 15 pipeline stages
- 27+ optimization passes
- New passes: LoopReordering, PrefetchHints, AliasScopeGen, LoopVectorizationHints
- Dual-compilation metadata strategy
- Twin-diff runtime support
