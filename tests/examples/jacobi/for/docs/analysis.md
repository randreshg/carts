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
  for (j = 0; j < ny; j++) {
    u[i][j] = unew[i][j];  // Each worker writes ONLY its assigned rows
  }
}
```

- **Pattern**: Simple block distribution
- **Ideal partitioning**: Chunked ownership (no communication needed)
- **Worker k**: owns rows `[k*chunkSize, (k+1)*chunkSize)`

### Loop 2: Stencil Access (Neighbor Computation)

```c
#pragma omp parallel for private(j)
for (i = 0; i < nx; i++) {
  for (j = 0; j < ny; j++) {
    // 5-point stencil: reads u[i-1], u[i], u[i+1]
    unew[i][j] = 0.25 * (u[i-1][j] + u[i][j+1] + u[i][j-1] + u[i+1][j] + f[i][j] * dx * dy);
  }
}
```

- **Pattern**: Each worker needs its rows PLUS neighbor boundaries
- **Halo requirement**: Worker k needs rows `[k*chunkSize-1, (k+1)*chunkSize+1)`
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
carts run jacobi-for.mlir --for-lowering &> jacobi-for_for-lowering.mlir
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
carts execute jacobi-for.c
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

## Testing Different Partitioning Strategies

To test different partitioning approaches, you can modify the CARTS partitioning heuristics or use debug flags:

### Force Element-Wise Partitioning

```bash
carts run jacobi-for.mlir --debug-only=create_dbs:element_wise --complete
```

### Force Chunk-Wise Partitioning

```bash
carts run jacobi-for.mlir --debug-only=create_dbs:chunk_wise --complete
```

### Analyze Halo Exchange

```bash
carts run jacobi-for.mlir --debug-only=db_partitioning:halo --complete
```

## Expected Partitioning Behavior

For this example, CARTS should ideally choose **EHP (Enhanced Hybrid Partitioning)**:

1. **Loop 1 (Copy)**: Uses chunked ownership (efficient bulk writes)
2. **Loop 2 (Stencil)**: Uses hybrid halo strips or ESD slice gets (optimal communication)

This demonstrates the key insight from the stencil partitioning analysis: different loops on the same data may require different partitioning strategies.

## Performance Analysis

Compare the different strategies by measuring:
- **DB count**: Fewer DBs = lower metadata overhead
- **Network bytes**: Optimal halo exchange vs wasteful chunk transfers
- **Dependency edges**: Fewer slots per EDT = faster readiness

Use the counters from `arts.cfg` to measure runtime behavior:

```bash
# After execution, check counters
cat ./counters/*
```

## Related Documentation

- `docs/heuristics/partitioning/stencil.md`: Comprehensive analysis of stencil partitioning strategies
- `tests/examples/jacobi/deps/docs/analysis.md`: Task-dependency version comparison
- `tests/examples/stencil/`: Pure stencil example
- `tests/examples/rowchunk/`: Row-chunking without stencils
