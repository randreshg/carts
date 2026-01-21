# Jacobi-For Example Analysis

This example demonstrates **stencil partitioning strategies** in CARTS. Unlike the `jacobi/deps` example (which uses `#pragma omp task depend`), this version uses `#pragma omp parallel for` to isolate two fundamentally different data access patterns:

1. **Loop 1 (Copy)**: Uniform access pattern - each worker writes only its assigned rows
2. **Loop 2 (Stencil)**: Neighbor-dependent access pattern - each worker reads its rows PLUS neighboring rows

This setup allows testing the partitioning heuristics described in `docs/heuristics/partitioning/stencil.md`.

## Key Differences from jacobi/deps

| Aspect | jacobi/for | jacobi/deps |
|--------|------------|-------------|
| OpenMP construct | `#pragma omp parallel for` | `#pragma omp task depend(...)` |
| Partitioning trigger | Row-chunking heuristic | User-provided dependencies |
| Data access patterns | **Isolated uniform vs stencil** | Mixed in single EDT |
| Testing focus | **Stencil partitioning** | Task dependency management |

## The Two Access Patterns

### Loop 1: Uniform Access (Copy Operation)

```c
#pragma omp parallel for private(j)
for (i = 0; i < nx; i++) {
   /// Each worker writes ONLY its assigned rows
  for (j = 0; j < ny; j++)
    u[i][j] = unew[i][j];
}
```

- **Pattern**: Simple block distribution
- **Ideal partitioning**: Chunked ownership (no communication needed)
- **Worker k**: owns rows `[k*blockSize, (k+1)*blockSize)`

### Loop 2: Stencil Access (Neighbor Computation)

```c
#pragma omp parallel for private(j)
for (i = 0; i < nx; i++) {
   /// 5-point stencil: reads u[i-1], u[i], u[i+1]
  for (j = 0; j < ny; j++)
    unew[i][j] = 0.25 * (u[i-1][j] + u[i][j+1] + u[i][j-1] + u[i+1][j] + f[i][j] * dx * dy);
}
```

- **Pattern**: Each worker needs its rows PLUS neighbor boundaries
- **Halo requirement**: Worker k needs rows `[k*blockSize-1, (k+1)*blockSize+1)`
- **Challenge**: How should CARTS partition array `u` for both patterns?

## Walkthrough Steps

### 1. Navigate to the jacobi-for example directory

```bash
cd ~/Documents/carts/tests/examples/jacobi/for
```

### 2. Build and generate the MLIR inputs

```bash
carts build
carts cgeist jacobi-for.c -O0 --print-debug-info -S --raise-scf-to-affine &> jacobi-for_seq.mlir
carts run jacobi-for_seq.mlir --collect-metadata &> jacobi-for_arts_metadata.mlir
carts cgeist jacobi-for.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> jacobi-for.mlir
```

### 3. Analyze the OpenMP to ARTS conversion

Run the pipeline up to the OpenMP conversion to see how the two loops are handled:

```bash
carts run jacobi-for.mlir --convert-openmp-to-arts &> jacobi-for_omp-to-arts.mlir
```

You should see:

- Both loops converted to `arts.for` with `parallel` mode
- Loop 1: Simple parallel loop over rows
- Loop 2: Parallel loop with stencil dependencies

### 4. Analyze Db creation and partitioning

Run the Db creation pass to see initial allocation:

```bash
carts run jacobi-for.mlir --create-dbs &> jacobi-for_create-dbs.mlir
```

This should show coarse-grained allocation (single DB for entire arrays). The key question is: how will CARTS partition this for the different access patterns?

### 5. Analyze ForLowering (partitioning decisions)

Run ForLowering to see how the parallel loops are decomposed:

```bash
carts run jacobi-for.mlir --concurrency &> jacobi-for_concurrency.mlir
```

This pass:

- Converts `arts.for` to worker-specific `scf.for` loops
- Adds `offset_hints` and `size_hints` for partitioning
- Creates epochs for synchronization between Loop 1 and Loop 2

### 6. Analyze DbPartitioning (stencil partitioning)

This is the critical pass for stencil partitioning:

```bash
carts run jacobi-for.mlir --db-partitioning --debug-only=db_partitioning &> jacobi-for_db-partitioning.mlir
```

Analyze how CARTS decides to partition the `u` array:

- **Element-wise** (row DBs): Precise but creates many DBs
- **Chunk-wise** (chunk DBs): Few DBs but wasteful for stencils
- **Hybrid** (chunks + halo strips): Balance of both
- **ESD** (chunks + slice gets): Optimal bytes with controlled DB count

### 7. Run the complete pipeline

```bash
carts run jacobi-for.mlir --complete &> jacobi-for_complete.mlir
```

### 8. Execute and verify

```bash
carts execute jacobi-for.c -O3
./jacobi-for_arts 
```

Expected output:

```
Jacobi-For Test: 100 x 100 grid, 10 iterations
Demonstrating uniform (Loop 1) vs stencil (Loop 2) access patterns
Running parallel version with #pragma omp parallel for...
Running sequential version for verification...
RMS error: 0.000000e+00
Max error: 0.000000e+00
Test PASSED
```

---

## DbPartitioning Analysis Results

This section documents the actual DbPartitioning behavior observed when running the jacobi-for example through the CARTS pipeline (as of December 2024).

### Partitioning Decisions Per Array

The `--concurrency-opt` pass reveals the following partitioning decisions:

| Array | Allocation ID | Partition Mode | Rationale |
|-------|---------------|----------------|-----------|
| `f` | jacobi-for.c:92:16 | `chunked` | Read-only, uniform access |
| `u` | jacobi-for.c:93:16 | `element_wise` | Stencil pattern detected |
| `unew` | jacobi-for.c:94:19 | `chunked` | Write-only per iteration, uniform access |

### Stencil Detection in Loop 2

DbPartitioning correctly identifies the stencil access pattern on array `u`:

```
Access patterns: hasUniform=false, hasStencil=true
```

For the stencil loop, the acquire for `u` uses extended ranges:

```mlir
// Compute offset including left halo
%51 = arith.cmpi uge, %36, %c1 : index
%52 = arith.subi %36, %c1 : index
%53 = arith.select %51, %52, %c0 : index  /// offset = max(i-1, 0)

/// Compute size including both halos
%54 = arith.addi %41, %c2 : index         /// size = blockSize + 2

/// Acquire with extended range
%guid_12, %ptr_13 = arts.db_acquire[<in>] (%guid_4, %ptr_5)
                    offsets[%53] sizes[%54]
```

This correctly extends the acquire range to include:

- **Left halo**: row `i-1` (when `i > 0`)
- **Right halo**: row `i+blockSize` (when within bounds)

### Current Implementation: Element-Wise for Stencil

The current DbPartitioning maps `RewriterMode::Stencil` to `element_wise` partitioning:

```cpp
/// Stencil uses element-wise attribute for now
case RewriterMode::Stencil: 
  return PromotionMode::element_wise;
```

This means each row of `u` becomes its own datablock. For the stencil loop:

- Worker k acquires DBs for rows `[k*7-1, k*7+7+1)` (with bounds checking)
- The acquire fetches only the needed row DBs
- **Advantage**: Precise data movement (only halo rows are fetched)
- **Disadvantage**: Many DBs (100 DBs for 100 rows)

### ESD Implementation Status

The ESD (Ephemeral Slice Dependencies) runtime infrastructure is now complete:

| Component | Status | Location |
|-----------|--------|----------|
| `artsRecordDepAt()` | Implemented | `external/arts/core/src/runtime/memory/DbFunctions.c` |
| `artsSignalEdtPtrWithGuid()` | Implemented | `external/arts/core/src/runtime/compute/EdtFunctions.c` |
| `artsDependent.byteOffset/size` | Extended | `external/arts/core/inc/arts/runtime/RT.h` |
| Distributed protocol | Implemented | `RemoteProtocol.h`, `RemoteFunctions.c`, `Server.c` |
| `RecordDepOp` with byte_offsets | Extended | `include/arts/ArtsOps.td` |
| EdtLowering halo extraction | Implemented | `lib/arts/Passes/Transformations/EdtLowering.cpp` |
| RecordDepPattern lowering | Updated | `lib/arts/Passes/Transformations/ConvertArtsToLLVM.cpp` |

**Missing Link**: DbPartitioning needs to be updated to:

1. Use chunked partitioning for stencil patterns (instead of element-wise)
2. Set `halo_left`, `halo_right`, `element_bytes` attributes on `DbAcquireOp`
3. This enables EdtLowering to create `RecordDepOp` with `byte_offsets`/`byte_sizes`

### ESD Data Flow (When Fully Connected)

```
1. DbPartitioning detects stencil pattern
   → Sets halo_left, halo_right, element_bytes on DbAcquireOp
   → Uses chunked partitioning (not element-wise)

2. EdtLowering extracts halo info from DbAcquireOp
   → Computes byte_offset = halo_left * element_bytes
   → Creates RecordDepOp with byte_offsets and byte_sizes

3. ConvertArtsToLLVM lowers to artsRecordDepAt(...)
   → Stores byteOffset/size in artsDependent struct

4. Runtime: When DB is ready, artsPersistentEventSatisfy
   → Checks for ESD deps (byteOffset != 0)
   → Computes slicePtr = dbData + byteOffset
   → Calls artsSignalEdtPtrWithGuid(edt, slot, dbGuid, slicePtr, size)

5. EDT receives:
   → depv[slot].guid = original DB GUID
   → depv[slot].ptr = pointer to slice
   → depv[slot].mode = ARTS_PTR
```

### Verification Results

The jacobi-for test passes with zero error:

```
$ carts execute jacobi-for.c -O3
$ ./jacobi-for_arts

Jacobi-For Test: 100 x 100 grid, 10 iterations
Demonstrating uniform (Loop 1) vs stencil (Loop 2) access patterns
Running sequential version for verification...
Running parallel version with #pragma omp parallel for...
RMS error: 0.000000e+00
Max error: 0.000000e+00
[CARTS] jacobi-for.c: PASS (0.0031s)
```

### Future Work: Connect ESD to DbPartitioning

To fully enable ESD for stencil patterns:

1. **Modify `toPromotionMode()`** in DbPartitioning.cpp:

   ```cpp
   /// Use chunked, not element_wise
   case RewriterMode::Stencil:
     return PromotionMode::chunked;  
   ```

2. **Set halo attributes** on DbAcquireOp during stencil rewriting:

   ```cpp
   acquireOp.setHaloLeftAttr(builder.getIndexAttr(haloLeft));
   acquireOp.setHaloRightAttr(builder.getIndexAttr(haloRight));
   acquireOp.setElementBytesAttr(builder.getIndexAttr(elementBytes));
   ```

3. **EdtLowering** will then automatically:
   - Extract these attributes
   - Compute byte offsets
   - Generate `artsRecordDepAt()` calls with ESD parameters

4. **Runtime** will deliver slices as `ARTS_PTR` dependencies instead of fetching whole DBs

### Trade-off Summary (Current vs ESD)

| Metric | Current (Element-Wise) | ESD (When Connected) |
|--------|------------------------|---------------------|
| DB count | O(Nrows) = 100 | O(Nchunks) = ~16 |
| Bytes transferred | Optimal (halo rows) | Optimal (halo bytes) |
| Dependency slots | ~blockSize + 2 | 3 (chunk + 2 halos) |
| Runtime overhead | Higher (many DBs) | Lower (few DBs) |
| Implementation | Complete | Runtime complete, DbPartitioning connection pending |
