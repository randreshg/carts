# parallel_for/chunk Example Analysis

This example demonstrates **element-wise partitioning** for a 1D vector addition with `#pragma omp parallel for schedule(static, 4)`. The H2 heuristic determines that for small block sizes (4 bytes/element x 4 elements = 16 bytes), element-wise allocation is optimal.

## Source Code

```c
#pragma omp parallel for schedule(static, 4)
for (int i = 0; i < N; i++)
  c[i] = a[i] + b[i];
```

## Key Characteristics

| Aspect | Value |
|--------|-------|
| OpenMP construct | `#pragma omp parallel for schedule(static, 4)` |
| Arrays | 1D integer arrays (a, b, c) |
| Block size | 4 iterations (from schedule clause) |
| Element size | 4 bytes (int) |
| Inner bytes | 16 bytes (4 x 4) |

## Expected Partitioning

| Array | Mode | Access Pattern | Partitioning |
|-------|------|----------------|--------------|
| `a` | `<in>` | Read-only | **Coarse** (H1: read-only on single-node) |
| `b` | `<in>` | Read-only | **Coarse** (H1: read-only on single-node) |
| `c` | `<out>` | Write | **Element-wise** (H2: innerBytes=16 < minInnerBytes) |

## Pipeline Flow

### 1. Build and generate MLIR

```bash
cd ~/Documents/carts/tests/examples/parallel_for/chunk
carts build
carts cgeist parallel_for.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> parallel_for.mlir
```

### 2. Run concurrency pass (ForLowering)

```bash
carts run parallel_for.mlir --concurrency &> parallel_for_concurrency.mlir
```

**Expected**: All allocations are coarse (`sizes[%c1]`), with `offset_hints` and `size_hints` on acquires.

### 3. Run concurrency-opt (second Db pass)

```bash
carts run parallel_for.mlir --concurrency-opt &> parallel_for_concurrency_opt.mlir
```

**Expected**:
- Arrays a, b (read-only): `sizes[%c1]` (coarse, H1 applied)
- Array c (write): `sizes[%N] elementSizes[%c1]` (element-wise, H2 applied)

### 4. Execute

```bash
carts execute parallel_for.c
./parallel_for_arts 12
```

**Expected output**:
```
Parallel region:
Parallel region completed
[CARTS] parallel_for.c: PASS (X.XXXXs)
```

## Heuristic Decisions

### H1: Read-only Coarse Allocation
- **Condition**: `nodeCount=1` AND `accessMode=<in>`
- **Result**: Arrays a, b stay coarse
- **Rationale**: No write contention to relieve

### H2: Cost Model for Write Arrays
- **Input**: innerBytes = 4 bytes x 4 elements = 16 bytes
- **Threshold**: minInnerBytes = 64 bytes (IntraNode mode)
- **Decision**: 16 < 64 -> **Reject chunking** -> Element-wise

## Verification Checklist

| Stage | What to Check |
|-------|---------------|
| `parallel_for_concurrency.mlir` | `offset_hints`, `size_hints` on acquires |
| `parallel_for_concurrency_opt.mlir` | a,b: `sizes[%c1]`; c: `sizes[%N] elementSizes[%c1]` |
| EDT body | db_ref uses redistributed indices (not localized) |
| Execution | No SIGSEGV, `PASS` result |

## Index Transformation (Element-wise)

For element-wise allocation with 1D arrays:

**Before promotion** (coarse allocation):
```mlir
%ref = arts.db_ref %arg5[%c0] : memref<?xmemref<?xi32>> -> memref<?xi32>
memref.store %val, %ref[%globalIdx] : memref<?xi32>
```

**After promotion** (element-wise):
```mlir
// outer = global element index (DB index for element-wise)
// inner = 0 (single element per DB)
%ref = arts.db_ref %arg5[%globalIdx] : memref<?xmemref<?xi32>> -> memref<?xi32>
memref.store %val, %ref[%c0] : memref<?xi32>
```

## Technical Details

### Why Element-wise (not Chunked)?

The H2 heuristic rejects chunking when `innerBytes < minInnerBytes`:
- `innerBytes = sizeof(int) * blockSize = 4 * 4 = 16 bytes`
- `minInnerBytes = 64 bytes` (cache-line aligned threshold)
- Since 16 < 64, chunking is rejected -> element-wise allocation

### Allocation Transformation

| Stage | Array c Allocation |
|-------|-------------------|
| Before | `sizes=[1], elementSizes=[N]` (coarse) |
| After | `sizes=[N], elementSizes=[1]` (element-wise) |

### Index Redistribution

Element-wise promotion redistributes indices:
1. Original: `db_ref[0]` then `load/store[i]`
2. Promoted: `db_ref[i]` then `load/store[0]`

This is different from chunked mode which uses coordinate localization:
- Chunked: `outer = (global - offset) / blockSize`, `inner = global % blockSize`
- Element-wise: `outer = global element index`, `inner = 0`

## Troubleshooting

**Problem**: SIGSEGV at runtime
- **Check**: Ensure `isChunked_` detection correctly identifies element-wise (innerSize=1)
- **Symptom**: Negative index in db_ref from chunked-style localization

**Problem**: Array c stays coarse
- **Check**: Are `offset_hints`/`size_hints` present on acquires?
- **Check**: Does H2 evaluate the allocation?

**Problem**: Wrong partitioning strategy
- **Check**: `arts.cfg` has `nodeCount=1` for single-node behavior
- **Check**: Element type size is correctly computed
