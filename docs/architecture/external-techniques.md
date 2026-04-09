# Techniques Adopted from External Projects

Survey of 15+ compiler/runtime systems. Organized by what SDE adopts NOW
(Phase 2) vs what informs architecture decisions.

## Core Adoptions (Phase 2)

### 1. Algorithm-Schedule Separation (Halide)

Separate WHAT to compute from HOW to compute it. The algorithm is fixed;
the schedule (tiling, parallelism, vectorization, storage) is composable.

**SDE adoption**: `sde.cu_*` = algorithm (WHAT), `sde.su_*` = schedule (HOW).
SDE attributes cleanly separate:
```
#sde.algorithm { pattern = "stencil", deps = [...], access = [...] }
#sde.schedule  { tile = [32,32], parallel = [0,1], vectorize = 2 }
```

### 2. MLIR Transform Dialect for Schedule Composition

Encode scheduling decisions AS IR operations, not pass parameters. Schedules
become composable, serializable, and debuggable.

**SDE adoption**: The `patterns/` layer could emit transform dialect sequences
as "recommended schedules" that SDE applies.

### 3. Open Earth Compiler's Stencil Hierarchy (ETH Zurich)

Two-level stencil representation: high-level data-flow graphs with explicit
producer-consumer and boundary conditions, low-level parallel execution.

**SDE adoption**: SDE's three-layer analysis (linalg + SCF + indirect)
mirrors this. Explicit boundary modeling via `tensor.pad` instead of
separate stencil boundary peeling.

### 4. Flang HLFIR's Deferred Buffering

`hlfir.elemental` represents array operations without committing to buffer
allocation. Decision of inline/fuse/materialize is deferred.

**SDE adoption**: SDE should NOT immediately lower `sde.cu_region` to
concrete allocations. Keep computation abstract; let SDE->ARTS conversion
decide DB allocation strategy.

### 5. Legion's Logical Regions (Stanford)

Hierarchically partitionable data abstractions with privilege tracking
(read/write/reduce) per subregion per task.

**SDE adoption**: `SdeMemAccessAttr` encodes which dimensions are
partitionable and what partition types are valid. SDE analysis determines
partitionability; ARTS core's `DbPartitioning` applies it.

### 6. Chapel's Domain Map Standard Interface (HPE)

Distribution is an EXTENSIBLE INTERFACE, not hardcoded strategies.

**SDE adoption**: Interface-based distribution hints:
```
#sde.distribution<strategy = @block_stencil, params = {halo = 1}>
```

### 7. PaRSEC's Parameterized Task Graphs (UTK ICL)

Express task graphs as parameterized functions, not enumerated instances.
Symbolic dependencies, lazy instantiation.

**SDE adoption**: `sde.su_iterate` = parameterized task family.
Dependency expressions are SYMBOLIC: "task(i,j) depends on task(i-1,j)".

## Architecture Informers

### Lift/RISE Functional Patterns (Edinburgh)

Functional combinators like `slide(size, stride)` for stencil access.
CARTS achieves similar declarative access via linalg's `indexing_maps`.

### TPP-MLIR Tensor Processing Primitives (Intel)

Micro-kernel abstractions (BRGEMM, fused GEMM+bias+ReLU) on top of linalg.
Validates linalg as the right substrate. CARTS's `KernelTransforms` could
adopt TPP's approach of fused kernel named ops.

### CIRCT's Dialect Progression (MLIR for hardware)

`hw -> comb -> seq -> sv` progression validates sde -> arts -> arts_rt
and confirms verification barriers between levels are essential.

### StarPU's Data Handle Model (INRIA)

Data handles with automatic coherency. Validates that SDE should describe
data access intent, with runtime binding (DB creation, mode selection)
at conversion time.

### Devito's Temporal Blocking (Imperial College)

Automatic temporal blocking (diamond/parallelogram tiling). SDE could
express temporal reuse opportunities as metadata:
```
#sde.temporal_reuse { depth = 2, wavefront_valid = true }
```

### Charm++ SDAG When-Clauses (UIUC)

`when` clauses declare data dependencies declaratively; runtime handles
message ordering. Parallels how SDE declares dependencies and ARTS core
materializes them as EDT dep slots.

## Integration Map

| Technique | Source | SDE Component | Phase |
|---|---|---|---|
| Algorithm-schedule separation | Halide | SDE attributes split | Phase 2 |
| Schedule as IR | MLIR Transform | patterns/ schedule emission | Phase 2+ |
| Two-level stencil model | Open Earth | Three-layer analysis | Phase 2 |
| Deferred buffering | Flang HLFIR | SDE->ARTS late materialization | Phase 2 |
| Hierarchical data regions | Legion | MemrefMetadataAttr partitioning | Phase 2 |
| Extensible distribution | Chapel DSI | SDE distribution hints | Phase 2 |
| Parameterized task graphs | PaRSEC PTG | Symbolic dependencies in SDE | Phase 2 |
| Functional stencil patterns | Lift/RISE | linalg indexing_maps | Phase 2 |
| Micro-kernel abstractions | TPP-MLIR | KernelTransforms on linalg | Phase 2+ |
| Dialect progression | CIRCT | sde->arts->arts_rt verified | Phase 1-3 |
| Data handle coherency | StarPU | SDE access intent -> DB modes | Phase 2 |
| Temporal blocking metadata | Devito | SDE temporal reuse attrs | Phase 2+ |
| Declarative dependencies | Charm++ SDAG | SDE dependency attributes | Phase 2 |
