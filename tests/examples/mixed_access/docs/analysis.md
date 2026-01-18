# Mixed Access Pattern Example Analysis

This example demonstrates **LULESH-like mixed access patterns** in CARTS:
- Loop 1: Direct chunked writes (`nodeData[i]`)
- Loop 2: Indirect reads through nodelist indirection (`nodeData[nodelist[e][j]]`)

This pattern is handled by the H1.2 heuristic using **full-range chunked acquires**
instead of DbCopy/DbSync, enabling efficient memory usage without duplication.

---

## Key Concepts

| Aspect | Loop 1 (Node-Parallel) | Loop 2 (Element-Parallel) |
|--------|------------------------|---------------------------|
| Access Pattern | Direct `nodeData[i]` | Indirect `nodeData[nodelist[e][j]]` |
| Partitioning | Chunked (worker owns chunk k) | Full-Range (worker reads ALL chunks) |
| Access Mode | Write (`<out>`) | Read (`<in>`) |
| Heuristic | Standard chunked | H1.2: Mixed access |

---

## Array Access Pattern Analysis

| Array | Loop 1 Access | Loop 2 Access | Expected Strategy |
|-------|---------------|---------------|-------------------|
| `nodeData` | Direct write `[i]` | Indirect read via `nodelist[e][j]` | **Chunked** with full-range indirect reads |
| `nodelist` | N/A | Direct read `[e*4+j]` | **Chunked** |
| `elemData` | N/A | Direct write `[e]` | **Chunked** |

---

## Step-by-Step Walkthrough

### 1. Navigate to the mixed_access example

```bash
cd ~/Documents/carts/tests/examples/mixed_access
```

### 2. Build and generate MLIR inputs

```bash
carts build
carts cgeist mixed_access.c -O0 --print-debug-info -S --raise-scf-to-affine &> mixed_access_seq.mlir
carts run mixed_access_seq.mlir --collect-metadata &> mixed_access_arts_metadata.mlir
carts cgeist mixed_access.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> mixed_access.mlir
```

### 3. Analyze OpenMP to ARTS conversion

```bash
carts run mixed_access.mlir --convert-openmp-to-arts &> mixed_access_omp-to-arts.mlir
```

This pass converts `omp.parallel` and `omp.wsloop` to ARTS primitives (`arts.parallel_for`).

### 4. Analyze Db creation

```bash
carts run mixed_access.mlir --create-dbs &> mixed_access_create-dbs.mlir
```

This pass identifies arrays that need datablock management and inserts `arts.db_alloc` operations.

### 5. Analyze ForLowering (concurrency stage)

```bash
carts run mixed_access.mlir --concurrency &> mixed_access_concurrency.mlir
```

Key observations:
- ForLowering adds `offset_hints`, `size_hints` for direct accesses
- Indirect accesses through `nodelist` won't have analyzable hints
- Both loops are converted to EDT-spawning constructs

### 6. Analyze DbPartitioning and H1.2 heuristic

```bash
carts run mixed_access.mlir --concurrency-opt --debug-only=db_partitioning,heuristics &> mixed_access_concurrency_opt.debug 2>&1
carts run mixed_access.mlir --concurrency-opt &> mixed_access_concurrency_opt.mlir
```

In the debug output, look for:
- `"H1.2 applied: Mixed access (chunked writes + full-range indirect reads)"`
- `"Marked indirect read-only acquire as full-range"`

### 7. Execute and verify

```bash
carts execute mixed_access.c -O3
./mixed_access_arts
```

Expected output:
```
Mixed Access Test: 2601 nodes, 2500 elements
Pattern: chunked node writes + full-range indirect reads
Running sequential version...
Running parallel version...
RMS error: 0.000000e+00
Max error: 0.000000e+00
Test PASSED
```

---

## H1.2 Heuristic Explanation

The H1.2 heuristic detects and handles mixed access patterns where the same array is:
1. Written directly in one loop (chunked partitioning)
2. Read indirectly in another loop (requires full-range access)

### Detection Criteria

```cpp
canChunked && hasIndirectRead && !hasIndirectWrite && hasDirectAccess
```

Where:
- `canChunked`: The array can be partitioned into chunks
- `hasIndirectRead`: At least one acquire reads via indirection
- `!hasIndirectWrite`: No indirect writes (would require more complex handling)
- `hasDirectAccess`: At least one acquire uses direct indexing

### Action Taken

When H1.2 triggers, it sets `needsFullRange=true` for indirect read-only acquires.

### Result

Indirect reads use `offsets[0] sizes[numChunks]` (full allocation range) instead of per-chunk offsets.

---

## IR Transformation Examples

### Before DbPartitioning (Coarse)

```mlir
%guid, %ptr = arts.db_alloc sizes[%c1] elementSizes[%numNodes]
    {arts.partition = #arts.promotion_mode<coarse>}
```

All data is in a single allocation; not suitable for parallel chunked writes.

### After DbPartitioning (Chunked with Full-Range)

```mlir
// Allocation: chunked into numChunks pieces
%guid, %ptr = arts.db_alloc sizes[%numChunks] elementSizes[%chunkSize]
    {arts.partition = #arts.promotion_mode<chunked>}

// Loop 1: Single chunk acquire (worker k -> chunk k)
// Worker writes only its assigned chunk
%acq1 = arts.db_acquire[<out>] offsets[%workerIdx] sizes[%c1]

// Loop 2: Full-range acquire (worker k -> ALL chunks)
// Worker reads potentially any node via indirect access
%acq2 = arts.db_acquire[<in>] offsets[%c0] sizes[%numChunks]
```

---

## Memory Efficiency Comparison

| Approach | Memory | Sync Ops | Description |
|----------|--------|----------|-------------|
| Old (Versioning) | 2x | DbCopy + DbSync | Creates fine-grained copy for indirect reads |
| New (Full-Range) | 1x | None | Same allocation, different acquire patterns |

The full-range approach:
- Avoids memory duplication
- Eliminates copy/sync overhead
- Maintains correctness through ARTS coherence

---

## Verification Checklist

After running the analysis, verify:

- [ ] H1.2 heuristic is triggered (check debug output for "H1.2 applied")
- [ ] `nodeData` allocation has `promotion_mode<chunked>`
- [ ] Loop 1 acquires use worker-specific offsets/sizes (`offsets[%workerIdx]`)
- [ ] Loop 2 acquires use full-range (`offsets[%c0] sizes[%numChunks]`)
- [ ] No `db_copy` or `db_sync` operations in final IR
- [ ] Test passes with correct numerical results (RMS error < 1e-10)

---

## Source Code Reference

The source file `mixed_access.c` contains:
- `compute_parallel()`: Two OpenMP parallel loops demonstrating the pattern
- `compute_seq()`: Sequential version for verification
- `build_mesh()`: Creates quad mesh connectivity (nodelist indirection array)

Key code pattern:
```c
/// Loop 1: Direct chunked write
#pragma omp parallel for
for (int i = 0; i < numNodes; i++)
    nodeData[i] = (double)(i * i) * 0.01;  /// Direct access

/// Loop 2: Indirect read
#pragma omp parallel for
for (int e = 0; e < numElems; e++) {
    double sum = 0.0;
    for (int j = 0; j < NODES_PER_ELEM; j++) {
        int nodeIdx = nodelist[e][j];      /// Indirection
        sum += nodeData[nodeIdx];          /// Indirect access
    }
    elemData[e] = sum / NODES_PER_ELEM;
}
```

---

## Related Documentation

- [Partitioning Heuristics](../../../../docs/heuristics/partitioning/partitioning.md) - Full documentation of H1.x heuristics
- [LULESH Task](../../../../docs/tasks/lulesh.md) - Real-world application using this pattern
