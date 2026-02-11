# Memory EDTs: Data Layout Transformation as First-Class Tasks

## Overview

Implement **Memory EDTs** - explicit EDTs that perform data layout transformations between producer and consumer EDTs. Based on the Memory Codelets paper ([arxiv 2302.00115](https://arxiv.org/abs/2302.00115)), this is designed as a task-graph-aware solution.

**Key Insight**: Instead of static non-task rewrites, Memory EDTs participate in the task graph with proper dependencies, enabling the runtime to schedule data transformations alongside computation.

## Motivation

Design requirements:
- Transformations must account for producer EDT dependencies.
- Transformations must be first-class tasks in the EDT dependency graph.
- Selection should use a cost model for profitability.

Memory EDTs solve these problems by making data layout transformations explicit, schedulable tasks.

## Scope

- **Initial**: Layout recoding (coarse/blocked/fine-grained) + gather operations
- **Future Extensions**: Streaming, compression, prefetching (extension points provided)
- **Insertion Mode**: Compiler-inserted with heuristics (future: artsmate-guided)

---

## Research Foundation: When Data Layout Transformations Are Profitable

### Academic Foundations

**Performance Impact** (from [Intel Memory Layout Transformations](https://www.intel.com/content/www/us/en/developer/articles/technical/memory-layout-transformations.html)):
- AOS→SOA transformations can yield **40-60% performance improvement** in real workloads
- In some cases, **10-100x performance differences** due to cache effects

**Cost Model Factors** (from [Region-Based Data Layout - CC 2024](https://dl.acm.org/doi/10.1145/3640537.3641571)):
- **Transformation overhead**: Copy cost = O(n × element_size)
- **Benefit**: Reduced cache misses, better vectorization, improved bandwidth
- **Break-even point**: When `benefit_per_access × num_accesses > transformation_cost`
- RebaseDL achieves **1.34x speedup** for transformed regions in SPEC CPU

**LULESH-Specific Results** (from [Data Layout Optimization for Portable Performance](https://link.springer.com/chapter/10.1007/978-3-662-48096-0_20)):
- Improvements range from **1.02x (Sandy Bridge) to 1.82x (POWER7)**
- Layout optimization achieves **78-95% of best manual layout**
- The algorithm found that optimal layout varies by architecture

### When Layout Transformation is Profitable

| Condition | Profitable? | Rationale |
|-----------|-------------|-----------|
| High reuse after transform | YES | Amortize transform cost over many accesses |
| SIMD-unfriendly access pattern | YES | Gather/scatter elimination enables vectorization |
| Indirect/gather access patterns | YES | Pre-gather into contiguous buffer |
| Memory bandwidth bound | YES | Contiguous access improves bandwidth utilization |
| Low compute-to-memory ratio | MAYBE | Transform overhead may dominate |
| Single-use data | NO | Transform cost not amortized |
| Already contiguous access | NO | No benefit from transformation |

### Key Use Cases from CARTS Benchmarks

#### 1. LULESH - Indirect Node Access (Primary Target)

Location: `external/carts-benchmarks/lulesh/lulesh.cpp:327-368`

```cpp
// CollectDomainNodesToElemNodes: 24 scattered reads per element
Index_t nd0i = nodelist[offset + 0];  // Read 8 indices
Index_t nd1i = nodelist[offset + 1];
// ... nd2i through nd7i ...

elemX[0] = x[nd0i];  // 8 scattered x[] reads
elemX[1] = x[nd1i];
// ...
elemY[0] = y[nd0i];  // 8 scattered y[] reads
// ...
elemZ[0] = z[nd0i];  // 8 scattered z[] reads
// ...
```

**Problem**: 24 cache misses per element, non-vectorizable
**Solution**: Memory EDT pre-gathers node data into element-local layout

#### 2. Stencil Kernels (Jacobi, Seidel)

Location: `external/carts-benchmarks/polybench/jacobi2d/jacobi2d.c`

```cpp
// 5-point stencil with pointer-to-pointer layout
B[i][j] = 0.2f * (A[i][j] + A[i-1][j] + A[i+1][j] + A[i][j-1] + A[i][j+1]);
```

**Problem**: Vertical neighbors `A[i±1][j]` require pointer chasing
**Solution**: Memory EDT transforms to contiguous 2D layout for halos

#### 3. Matrix Multiplication (GEMM, 3MM)

Location: `external/carts-benchmarks/polybench/gemm/gemm.c`

```cpp
sum += A[i][k] * B[k][j];  // B[k][j]: column access on row-major layout
```

**Problem**: Strided access on B matrix kills cache performance
**Solution**: Memory EDT transposes B or creates tiled layout

---

## Implementation Plan

### Phase 1: IR Operations and Data Structures

**1.1 Add MemoryEdtKind enum** (`include/arts/ArtsAttributes.td`)

```tablegen
def MemoryEdtKind : Arts_I32Enum<"MemoryEdtKind", [
  I32EnumAttrCase<"recode", 0>,    // Layout transformation (coarse↔blocked↔fine)
  I32EnumAttrCase<"gather", 1>,    // Indirect gather (LULESH nodelist pattern)
  I32EnumAttrCase<"scatter", 2>,   // Indirect scatter (inverse of gather)
  I32EnumAttrCase<"copy", 3>,      // Simple copy (locality optimization)
  I32EnumAttrCase<"prefetch", 4>,  // (future) Async prefetch
  I32EnumAttrCase<"stream", 5>,    // (future) Streaming
  I32EnumAttrCase<"compress", 6>   // (future) Compressed transfer
]>;
```

**1.1.1 Add GatherInfo struct** for LULESH-style indirect access:

```cpp
struct GatherInfo {
  Value indirectionArray;  // nodelist[] in LULESH
  int64_t elementsPerGather;  // 8 for LULESH (8 nodes per element)
  SmallVector<Value> sourceArrays;  // x[], y[], z[] arrays
};
```

**1.2 Add MemoryEdtOp** (`include/arts/ArtsOps.td`, near EdtOp ~line 510)

```tablegen
def MemoryEdtOp : Arts_Op<"memory_edt", [...]> {
  let arguments = (ins
    MemoryEdtKindAttr:$kind,
    AnyMemRef:$source_ptr,
    PartitionModeAttr:$source_layout,
    PartitionModeAttr:$dest_layout,
    I32:$route,
    // For gather/scatter operations:
    OptionalAttr<AnyMemRef>:$indirection_array,  // nodelist[] for LULESH
    OptionalAttr<I64Attr>:$elements_per_access,  // 8 for LULESH
    Variadic<AnyMemRef>:$dependencies
  );
  let results = (outs MemRefOf<[I64]>:$guid, AnyMemRef:$ptr);

  let extraClassDeclaration = [{
    bool isGather() const { return getKind() == MemoryEdtKind::gather; }
    bool isScatter() const { return getKind() == MemoryEdtKind::scatter; }
    bool isRecode() const { return getKind() == MemoryEdtKind::recode; }
  }];
}
```

**1.2.1 Gather Operation IR Example** (for LULESH):

```mlir
// Gather: transform node-indexed data to element-local layout
%elem_data_guid, %elem_data_ptr = arts.memory_edt gather
    source(%node_x_ptr : memref<?xf64>) [source_layout = blocked]
    -> [dest_layout = fine_grained]
    indirection(%nodelist : memref<?x8xi32>)
    elements_per_access(8)
    route(%r) dependencies(%node_x_ptr)
    : (memref<i64>, memref<?x8xf64>)
// Result: elem_data[elem][0..7] = x[nodelist[elem][0..7]]
```

**1.3 Add NodeKind::MemoryEdt** (`include/arts/Analysis/Graphs/Base/NodeBase.h`)

**1.4 Create MemoryEdtNode class** (`include/arts/Analysis/Graphs/Edt/MemoryEdtNode.h`)
- Wraps MemoryEdtOp
- Tracks source/dest layout info
- Integrates with EdtGraph

**1.5 Create MemoryEdtInfo struct** (`include/arts/Analysis/Edt/MemoryEdtInfo.h`)
- Source/dest layouts and dimensions
- Transfer size estimation
- Cost metrics for scheduling

### Phase 2: MemoryEdtPass Structure

**2.1 Register pass** (`include/arts/Passes/ArtsPasses.td`)

```tablegen
def MemoryEdt : Pass<"memory-edt", "mlir::ModuleOp"> {
  let options = [
    Option<"mode", "mode", "std::string", "\"auto\"",
           "Mode: auto|always|never|annotated">,
    Option<"costThreshold", "cost-threshold", "float", "0.8">
  ];
}
```

**2.2 Create pass implementation** (`lib/arts/Passes/Optimizations/MemoryEdtPass.cpp`)

Pass structure:

```cpp
void MemoryEdtPass::runOnOperation() {
  // Phase 1: Detect layout mismatches (using EdtDataFlowAnalysis)
  auto mismatches = detectLayoutMismatches(func);

  // Phase 2: Evaluate H1.6 heuristics
  for (auto &mismatch : mismatches) {
    if (shouldInsertMemoryEdt(mismatch)) {
      // Phase 3: Insert Memory EDT and rewire
      insertMemoryEdt(mismatch, builder);
    }
  }
}
```

**2.3 Add to pipeline** - Run after DbPartitioningPass (Stage 12, `concurrency-opt`)

### Phase 3: Heuristics (H1.6)

**3.1 Add LayoutMismatchAnalysis** (`lib/arts/Analysis/Edt/LayoutMismatchAnalysis.cpp`)
- Extend EdtDataFlowAnalysis to detect layout incompatibilities
- Track producer/consumer access patterns

**3.2 Implement H1.6 Heuristic** (`include/arts/Analysis/HeuristicsConfig.h`)

Key decision points:
- **H1.6.1**: Skip if layouts already compatible
- **H1.6.2**: Mixed blocked writes + indirect reads (LULESH pattern) → `recode`
- **H1.6.3**: Indirect array access via indirection array → `gather`
- **H1.6.4**: Stencil producer with gather consumer → `recode` or `gather`
- **H1.6.5**: Multi-node network optimization → `recode` with locality hints

**Gather Detection** (H1.6.3):

```cpp
// Detect LULESH-style indirect access: A[B[i]]
bool detectGatherPattern(DbAcquireOp acquire) {
  // 1. Find load operations using the acquire result
  // 2. Check if indices come from another load (indirection)
  // 3. Extract indirection array and access width
  // 4. Return true if pattern matches gather semantics
}
```

Cost model:

```cpp
float estimateTransformCost(ctx);   // Memory EDT overhead
float estimateFullRangeCost(ctx);   // H1.2 full-range overhead
// Insert Memory EDT if transformCost < fullRangeCost * threshold
```

### Phase 4: EDT Insertion and Rewiring

**4.1 Create MemoryEdtInserter** (`lib/arts/Transforms/MemoryEdtInserter.cpp`)

Insertion algorithm:
1. Create target allocation with new layout
2. Create Memory EDT with proper dependencies
3. Generate transformation loop (reuse DbLowering patterns)
4. Rewire consumer acquires to target allocation
5. Update block arguments and dependency lists

**4.2 Handle multiple consumers**
- Group consumers by (producer, sourceAlloc, targetMode)
- Create ONE Memory EDT per group
- All grouped consumers acquire from shared targetAlloc

**4.3 Dependency chain**:

```
Producer EDT → Memory EDT → Consumer EDT(s)
     ↓              ↓              ↓
sourceAlloc    (transforms)   targetAlloc
```

### Phase 5: Lowering

**5.1 Create MemoryEdtLoweringPass** (`lib/arts/Passes/Lowering/MemoryEdtLowering.cpp`)

Lowering expands MemoryEdtOp to:
1. New DbAllocOp for destination (with target layout)
2. Wrapped EdtOp with transformation loop body
3. Direct load/store operations in the generated task body

### Phase 6: Pipeline Alignment (Completed)

The active path is explicit task lowering through `MemoryEdtOp` and standard EDT infrastructure.

---

## Critical Files to Modify

| File | Changes |
|------|---------|
| `include/arts/ArtsAttributes.td` | Add MemoryEdtKind enum |
| `include/arts/ArtsOps.td` | Add MemoryEdtOp |
| `include/arts/Analysis/Graphs/Base/NodeBase.h` | Add NodeKind::MemoryEdt |
| `include/arts/Passes/ArtsPasses.td` | Register MemoryEdtPass |
| `include/arts/Analysis/HeuristicsConfig.h` | Add H1.6 context/decision types |
| `lib/arts/Analysis/Graphs/Edt/EdtGraph.cpp` | Handle MemoryEdtNode |
| `lib/arts/Analysis/Edt/EdtDataFlowAnalysis.cpp` | Add layout mismatch detection |

## New Files to Create

| File | Purpose |
|------|---------|
| `include/arts/Analysis/Graphs/Edt/MemoryEdtNode.h` | Node class for EdtGraph |
| `include/arts/Analysis/Edt/MemoryEdtInfo.h` | Metadata struct |
| `lib/arts/Analysis/Edt/LayoutMismatchAnalysis.cpp` | Detect layout mismatches |
| `lib/arts/Passes/Optimizations/MemoryEdtPass.cpp` | Main pass |
| `lib/arts/Transforms/MemoryEdtInserter.cpp` | EDT insertion logic |
| `lib/arts/Passes/Lowering/MemoryEdtLowering.cpp` | Lower MemoryEdtOp to EdtOp |

## Files to Remove

| File/Directory | Reason |
|----------------|--------|
| `include/arts/Transforms/Datablock/Versioned/` | Replaced by Memory EDTs |
| `lib/arts/Transforms/Datablock/Versioned/` | Replaced by Memory EDTs |

---

## Example: LULESH Pattern

**Before** (H1.2 full-range):

```mlir
// Producer: blocked writes
arts.edt parallel (%chunk_acq) { x[i] = ... }

// Consumer: full-range indirect reads (wasteful)
%full_acq = arts.db_acquire offsets[0] sizes[numChunks]
arts.edt parallel (%full_acq) { ... = x[nodelist[e]] }
```

**After** (Memory EDT with recode):

```mlir
// Producer: blocked writes
arts.edt parallel (%chunk_acq) { x[i] = ... }

// Memory EDT: transform blocked → fine-grained
%fg_guid, %fg_ptr = arts.memory_edt recode
    source(%x_ptr) [source_layout = blocked]
    -> [dest_layout = fine_grained]
    route(%r) dependencies(%x_ptr)

// Consumer: fine-grained indexed reads (efficient)
%elem_acq = arts.db_acquire indices[%nodeIdx]
arts.edt parallel (%elem_acq) { ... = x_fg[nodelist[e]] }
```

**After** (Memory EDT with gather):

```mlir
// Producer: blocked writes to node arrays
arts.edt parallel (%chunk_acq) { x[i] = ...; y[i] = ...; z[i] = ... }

// Memory EDT: gather node data into element-local layout
%elem_x_guid, %elem_x_ptr = arts.memory_edt gather
    source(%x_ptr) [source_layout = blocked]
    -> [dest_layout = fine_grained]
    indirection(%nodelist) elements_per_access(8)
    route(%r) dependencies(%x_ptr)

// Consumer: contiguous element-local reads (SIMD-friendly)
arts.edt parallel (%elem_x_acq) {
  // elem_x[elem][0..7] is now contiguous - can vectorize!
  ... = elem_x[e][0] + elem_x[e][1] + ...
}
```

---

## CLI Usage

```bash
# Enable with auto heuristics (default)
carts run input.mlir --memory-edt

# Force always/never
carts run input.mlir --memory-edt --memory-edt-mode=always
carts run input.mlir --memory-edt --memory-edt-mode=never

# Only process annotated allocations
carts run input.mlir --memory-edt --memory-edt-mode=annotated

# Adjust cost threshold
carts run input.mlir --memory-edt --memory-edt-cost-threshold=0.5
```

---

## Lowering Strategy: Memory EDT → ARTS Runtime

### Lowering Pipeline

Memory EDTs lower through the existing infrastructure with minimal runtime changes:

```
MemoryEdtOp
    ↓ (MemoryEdtLoweringPass)
EdtOp + DbAllocOp + transformation loop
    ↓ (EdtLoweringPass - existing)
artsEdtCreate() + outlined function
```

### Key Lowering Steps

**Step 1: MemoryEdtLoweringPass** (new pass)

```mlir
// Input:
%guid, %ptr = arts.memory_edt recode
    source(%src_ptr) [source_layout = blocked]
    -> [dest_layout = fine_grained]
    route(%r) dependencies(%src_ptr)

// Output:
%dest_alloc = arts.db_alloc[<inout>, <heap>, <write>, <fine_grained>]
    sizes[%total] elementSizes[1]
arts.edt single intranode route(%r) (%src_ptr) {
  // Transformation loop (generated)
  scf.for %i = 0 to %total {
    // Load from source layout
    %src_chunk = arith.divui %i, %block_size
    %src_offset = arith.remui %i, %block_size
    %src_base = arts.db_ref %src_ptr[%src_chunk]
    %val = memref.load %src_base[%src_offset]
    // Store to dest layout
    memref.store %val, %dest_ptr[%i]
  }
  arts.yield
}
```

**Step 2: Reuse Existing EdtLowering**

The EDT wrapper lowers through standard `EdtLoweringPass`:
- `artsEdtCreate()` with paramv containing packed metadata
- Dependency recording via `artsRecordDep()`
- Outlined function `__arts_edt_N` with transformation body

### Runtime Integration

**No New Runtime APIs Needed** - Memory EDTs use existing infrastructure:
- `artsEdtCreate()` - create the transformation EDT
- `artsDbCreateWithGuid()` - allocate destination datablock
- `artsRecordDep()` - record dependency on source datablock
- `artsSignalEdt()` - signal completion to consumer EDTs

**Optional Future Runtime Extensions**:
- `artsMemEdtCreate()` - specialized creation with layout hints
- `artsAsyncMemcpy()` - hardware-accelerated copy for simple cases
- `artsGatherScatter()` - optimized gather/scatter primitives

### Transformation Loop Patterns

**Blocked → Fine-Grained**:

```cpp
// From DbLowering.cpp:259-265 pattern
Value numChunks = sourceAlloc.getSizes().front();
Value blockSize = sourceAlloc.getElementSizes().front();
Value totalSize = builder.create<arith::MulIOp>(loc, numChunks, blockSize);
// Loop: for i in [0, totalSize)
//   src_chunk = i / blockSize
//   src_offset = i % blockSize
//   dest[i] = src[src_chunk][src_offset]
```

**Gather Operation** (for LULESH nodelist):

```cpp
// Generated transformation loop for gather
// Input: nodeData[numNodes], nodelist[numElems][8]
// Output: elemData[numElems][8] (contiguous, SIMD-friendly)
for (elem = 0; elem < numElems; elem++) {
  for (node = 0; node < 8; node++) {
    idx = nodelist[elem * 8 + node];  // Indirection
    elemData[elem][node] = nodeData[idx];  // Gather
  }
}
// Benefits:
// - elemData is now contiguous (cache-friendly)
// - Subsequent compute can use elemData[elem][0..7] with unit stride
// - Enables SIMD vectorization over the 8 node values
```

**Gather Lowering to EDT**:

```mlir
// arts.memory_edt gather lowers to:
%dest_alloc = arts.db_alloc sizes[%numElems] elementSizes[8]
arts.edt single intranode route(%r) (%nodelist, %nodeData) {
  scf.for %elem = 0 to %numElems {
    scf.for %node = 0 to 8 {
      %idx_ptr = memref.load %nodelist[%elem, %node]
      %idx = arith.index_cast %idx_ptr : i32 to index
      %val = memref.load %nodeData[%idx]
      memref.store %val, %dest_ptr[%elem, %node]
    }
  }
  arts.yield
}
```

---

## Comprehensive Opportunity Analysis

### Priority 1: Critical Performance Impact

| Pattern | Benchmark | Current Issue | Memory EDT Solution | Expected Gain |
|---------|-----------|---------------|---------------------|---------------|
| Indirect gather (nodelist) | LULESH | 24 scattered reads/element | Pre-gather via MemEDT | 1.5-1.8x |
| Mixed blocked+indirect | LULESH | H1.2 full-range overhead | Blocked→fine-grained MemEDT | 1.2-1.5x |
| Stencil halo fetch | Jacobi2D | Vertical neighbor ptr chase | Contiguous halo prefetch | 1.1-1.3x |

### Priority 2: Moderate Performance Impact

| Pattern | Benchmark | Current Issue | Memory EDT Solution | Expected Gain |
|---------|-----------|---------------|---------------------|---------------|
| Column-major B access | GEMM, 3MM | Strided k-loop access | Transpose B via MemEDT | 1.2-1.4x |
| ESD halo overlap | Stencils | Sequential halo + compute | Async halo prefetch | 1.1-1.2x |
| Reduction tree | Parallel reductions | Serial accumulator contention | Local→global MemEDT | 1.1-1.3x |

### Priority 3: Future Extensions

| Pattern | Use Case | Memory EDT Type |
|---------|----------|-----------------|
| Multi-node broadcast | Distributed init | `MemEDT_broadcast` |
| Compression | Large transfers | `MemEDT_compress` |
| Prefetch pipeline | Double buffering | `MemEDT_prefetch` |
| AOS↔SOA | SIMD-friendly layout | `MemEDT_recode` (extended) |

### Detection Heuristics

**H1.6.1: Indirect Access Pattern**

```
IF hasIndirectAccess(consumer) AND hasDirectAccess(producer)
   AND accessCount(consumer) > threshold
THEN insertMemoryEDT(producer→consumer, gather)
```

**H1.6.2: Layout Mismatch**

```
IF partitionMode(producer) ≠ partitionMode(consumer)
   AND isRAWDependency(producer, consumer)
   AND estimatedBenefit > transformCost
THEN insertMemoryEDT(producer→consumer, recode)
```

**H1.6.3: Stencil Halo Optimization**

```
IF hasStencilPattern(consumer)
   AND haloSize > 0
   AND canOverlapCompute(haloFetch)
THEN insertMemoryEDT(prefetch_halo, async=true)
```

---

## Research Contribution

This implementation provides:
1. **First-class Memory EDTs** in the Codelet/EDT execution model
2. **Explicit data layout transformations** as schedulable tasks
3. **Heuristics-driven insertion** with cost model
4. **Extension points** for streaming, compression, prefetching

Differentiation from prior work:
- Unlike static DbCopy/DbSync, Memory EDTs participate in task scheduling
- Unlike implicit caching, explicit programmer/compiler control
- Aligns with Memory Codelets paper vision within CARTS infrastructure

### Publication-Worthy Contributions

1. **Novel Abstraction**: Memory EDTs as first-class schedulable tasks for data layout transformation (extends Memory Codelets [arxiv 2302.00115](https://arxiv.org/abs/2302.00115))

2. **Compiler Integration**: Automatic detection and insertion via heuristics (H1.6) integrated with existing partitioning framework

3. **Cost Model**: Profitability analysis based on access patterns, reuse, and transformation overhead

4. **Benchmarks**: Demonstrated on LULESH (indirect gather), Polybench stencils, and matrix operations

5. **Extensibility**: Framework supports future memory operations (streaming, compression, prefetching)

---

## Testing Plan

### Unit Tests

**1. IR Operation Tests** (`test/arts/Ops/MemoryEdtOpTests.cpp`)
```cpp
// Test MemoryEdtOp creation and verification
TEST(MemoryEdtOp, CreateRecode) {
  // Create recode operation, verify attributes
}

TEST(MemoryEdtOp, CreateGather) {
  // Create gather operation with indirection array
  // Verify elements_per_access attribute
}

TEST(MemoryEdtOp, Verifier) {
  // Test that invalid combinations are rejected
  // e.g., gather without indirection_array should fail
}
```

**2. Pass Tests** (`test/arts/Passes/MemoryEdtPassTests.cpp`)
```cpp
// Test layout mismatch detection
TEST(MemoryEdtPass, DetectBlockedToFineGrained) {
  // Producer writes blocked, consumer reads fine-grained
  // Verify mismatch is detected
}

TEST(MemoryEdtPass, DetectGatherPattern) {
  // LULESH-style A[B[i]] pattern
  // Verify gather is detected
}

TEST(MemoryEdtPass, SkipCompatibleLayouts) {
  // Same layout producer/consumer
  // Verify no Memory EDT inserted
}
```

**3. Lowering Tests** (`test/arts/Lowering/MemoryEdtLoweringTests.cpp`)
```cpp
TEST(MemoryEdtLowering, RecodeToEdtWithLoop) {
  // Verify MemoryEdtOp lowers to EdtOp + scf.for loop
}

TEST(MemoryEdtLowering, GatherToEdtWithNestedLoop) {
  // Verify gather lowers to EDT with double-nested loop
}
```

### Integration Tests

**1. FileCheck Tests** (`test/arts/Integration/memory_edt/`)

```bash
# test/arts/Integration/memory_edt/recode_blocked_to_fine.mlir
// RUN: carts-opt %s --memory-edt | FileCheck %s

// CHECK: arts.memory_edt recode
// CHECK-SAME: source_layout = blocked
// CHECK-SAME: dest_layout = fine_grained
func.func @test_recode(%arg: memref<?x?xf64>) {
  // ... producer EDT with blocked writes ...
  // ... consumer EDT with indirect reads ...
}
```

```bash
# test/arts/Integration/memory_edt/gather_nodelist.mlir
// RUN: carts-opt %s --memory-edt | FileCheck %s

// CHECK: arts.memory_edt gather
// CHECK-SAME: indirection
// CHECK-SAME: elements_per_access(8)
func.func @test_gather(%nodelist: memref<?x8xi32>, %x: memref<?xf64>) {
  // ... LULESH-style indirect access pattern ...
}
```

**2. End-to-End Tests** (`test/arts/E2E/memory_edt/`)

```bash
# Test correctness: results must match non-transformed version
carts run lulesh.mlir --memory-edt -o lulesh_medt.ll
carts run lulesh.mlir --memory-edt-mode=never -o lulesh_baseline.ll

# Run both and compare checksums
./lulesh_medt --verify
./lulesh_baseline --verify
# Checksums must match
```

### Benchmark Tests

**1. LULESH Performance** (`benchmarks/memory_edt/`)

```bash
#!/bin/bash
# benchmark_lulesh_memory_edt.sh

echo "=== LULESH Memory EDT Benchmark ==="

# Baseline: H1.2 full-range (no Memory EDT)
echo "Running baseline (full-range)..."
carts benchmarks run lulesh --size small -- --memory-edt-mode=never 2>&1 | tee baseline.log

# With Memory EDT recode
echo "Running with Memory EDT recode..."
carts benchmarks run lulesh --size small -- --memory-edt 2>&1 | tee medt_recode.log

# With Memory EDT gather (when implemented)
echo "Running with Memory EDT gather..."
carts benchmarks run lulesh --size small -- --memory-edt --memory-edt-prefer-gather 2>&1 | tee medt_gather.log

# Compare results
echo "=== Results ==="
grep "E2E Time" baseline.log medt_recode.log medt_gather.log
grep "checksum" baseline.log medt_recode.log medt_gather.log
```

**2. Polybench Stencils**

```bash
# Test Jacobi2D
carts benchmarks run jacobi2d -- --memory-edt
carts benchmarks run jacobi2d -- --memory-edt-mode=never

# Test Seidel2D
carts benchmarks run seidel2d -- --memory-edt
carts benchmarks run seidel2d -- --memory-edt-mode=never
```

**3. Matrix Operations**

```bash
# Test GEMM with transpose optimization
carts benchmarks run gemm -- --memory-edt
carts benchmarks run gemm -- --memory-edt-mode=never

# Test 3MM chain multiplication
carts benchmarks run 3mm -- --memory-edt
carts benchmarks run 3mm -- --memory-edt-mode=never
```

### Verification Checklist

| Test | Command | Expected Result |
|------|---------|-----------------|
| IR parsing | `carts-opt --verify-diagnostics` | No errors |
| Recode lowering | `carts-opt --memory-edt-lowering` | EdtOp with loop |
| Gather lowering | `carts-opt --memory-edt-lowering` | EdtOp with nested loop |
| LULESH correctness | `carts benchmarks run lulesh --verify` | Checksum matches |
| LULESH speedup | `carts benchmarks run lulesh` | ≥1.2x vs baseline |
| Jacobi correctness | `carts benchmarks run jacobi2d --verify` | Results match |

### Debugging Commands

```bash
# Dump Memory EDT decisions
carts run input.mlir --memory-edt --memory-edt-enable-diagnostics 2>&1 | grep "H1.6"

# Visualize EDT graph with Memory EDTs
carts run input.mlir --memory-edt --dump-edt-graph > edt_graph.dot
dot -Tpng edt_graph.dot -o edt_graph.png

# Trace transformation
carts run input.mlir --memory-edt --mlir-print-ir-after-all 2>&1 | less
```

### Test Files to Create

| File | Purpose |
|------|---------|
| `test/arts/Ops/MemoryEdtOpTests.cpp` | Unit tests for MemoryEdtOp |
| `test/arts/Passes/MemoryEdtPassTests.cpp` | Unit tests for pass |
| `test/arts/Lowering/MemoryEdtLoweringTests.cpp` | Lowering tests |
| `test/arts/Integration/memory_edt/*.mlir` | FileCheck integration tests |
| `test/arts/E2E/memory_edt/*.mlir` | End-to-end correctness tests |
| `benchmarks/memory_edt/benchmark_*.sh` | Performance benchmark scripts |

---

## References

- [Memory Codelets Paper (arxiv 2302.00115)](https://arxiv.org/abs/2302.00115) - "On Memory Codelets: Prefetching, Recoding, Moving and Streaming Data"
- [Intel Memory Layout Transformations](https://www.intel.com/content/www/us/en/developer/articles/technical/memory-layout-transformations.html)
- [Region-Based Data Layout - CC 2024](https://dl.acm.org/doi/10.1145/3640537.3641571)
- [Data Layout Optimization for Portable Performance](https://link.springer.com/chapter/10.1007/978-3-662-48096-0_20)
- [AoS and SoA - Wikipedia](https://en.wikipedia.org/wiki/AoS_and_SoA)
