# MLIR Infrastructure Integration

Current branch note:
- This file mixes current branch state with a broader planned SDE design.
- What is implemented today is a transient linalg/tensor carrier window inside
  SDE, not a fully destination-style tensor-native SDE surface.
- Use [docs/sde.md](/home/raherrer/projects/carts/docs/sde.md) and
  [sde-dialect.md](sde-dialect.md) as the source of truth for current
  ownership: SDE owns semantic decisions, and ARTS consumes those decisions
  later as runtime-structural contracts.

The MLIR infrastructure pieces are already active in the SDE window, but the
current branch does not yet provide type-polymorphic destination-style SDE ops
or a tensor-authoritative executable SDE form.

## Linalg: Core Structured Computation (Stages 3.6-3.9)

**New pass: `RaiseToLinalg`** -- Recognize scf.for+memref patterns ->
linalg.generic.

### What it replaces

| CARTS Current | Linalg Equivalent |
|---|---|
| `PatternDiscovery` | iterator_types classify patterns automatically |
| `AccessAnalyzer` | indexing_maps encode access patterns declaratively |
| `BlockLoopStripMining` | TilingInterface for distribution-aware tiling |
| Parts of `LoopFusion` | linalg tile-and-fuse |

### Upstream MLIR passes SDE will use

| MLIR Pass | Replaces CARTS Pass | Benefit |
|---|---|---|
| `linalg-generalize-named-ops` | -- | Normalize named -> generic |
| `linalg-fuse-elementwise-ops` | Custom LoopFusion | Proven fusion |
| `linalg-fold-unit-extent-dims` | -- | Simplify degenerate dims |
| `linalg-to-loops` | Custom ForLowering (partially) | Battle-tested |
| TilingInterface | BlockLoopStripMining | Generic tiling |

### New CARTS-specific linalg passes

| Pass | Purpose |
|---|---|
| `RaiseToLinalg` | scf.for+memref -> linalg.generic recognition |
| `ClassifyLinalgPatterns` | Read iterator_types -> stamp PatternAttr |
| `LinalgDistributionPlanning` | Use TilingInterface for ARTS worker mapping |

### Example: Matmul recognition

```mlir
// INPUT (from Polygeist):
scf.for %i = 0 to %N {
  scf.for %j = 0 to %N {
    scf.for %k = 0 to %N {
      %a = memref.load %A[%i, %k]
      %b = memref.load %B[%k, %j]
      %c = memref.load %C[%i, %j]
      %d = arith.mulf %a, %b
      %e = arith.addf %c, %d
      memref.store %e, %C[%i, %j]
    }
  }
}

// OUTPUT (after RaiseToLinalg):
linalg.generic {
  indexing_maps = [
    affine_map<(i,j,k) -> (i,k)>,  // A access
    affine_map<(i,j,k) -> (k,j)>,  // B access
    affine_map<(i,j,k) -> (i,j)>   // C access
  ],
  iterator_types = ["parallel", "parallel", "reduction"]
} ins(%A, %B) outs(%C) { ... }
```

Pattern = matmul (2 parallel + 1 reduction) -- NO PatternDiscovery needed.

### Example: Stencil in linalg

```mlir
// 1D Jacobi: C[i] = (A[i-1] + A[i] + A[i+1]) / 3
linalg.generic {
  indexing_maps = [
    affine_map<(i) -> (i - 1)>,   // A[i-1]
    affine_map<(i) -> (i)>,       // A[i]
    affine_map<(i) -> (i + 1)>,   // A[i+1]
    affine_map<(i) -> (i)>        // C[i]
  ],
  iterator_types = ["parallel"]
} ins(%A, %A, %A) outs(%C) { ... }
```

Pattern = stencil (non-identity indexing maps with offsets).

---

## Tensor + Bufferization: Core Analysis Layer (Stages 3.7-3.9)

Tensor space is the core analysis representation. SDE raises to tensor,
analyzes in tensor space (alias-free, SSA deps), then bufferizes back to
memref before ARTS.

See [pipeline-redesign.md](pipeline-redesign.md) for the full
raise/analyze/lower cycle.

| Concept | Tensor Form | CARTS Equivalent |
|---|---|---|
| `tensor.extract_slice` | Read a subregion | `arts.db_acquire(READ)` |
| `tensor.insert_slice` | Write a subregion | `arts.db_acquire(WRITE)` + release |
| `tensor.pad` | Boundary padding | Stencil boundary peeling |
| One-shot bufferization | Optimal in-place | DB alias analysis |
| SSA dataflow | Complete dep graph | EdtDataFlowAnalysis |

---

## Affine: Precise Analysis Engine (Stages 2-3)

Already partially used by CARTS -- SDE formalizes the affine analysis window.

### Currently used

| Function | Used By |
|---|---|
| `checkMemrefAccessDependence` | DependenceAnalyzer |
| `getPerfectlyNestedLoops` | LoopAnalyzer |
| `isLoopParallel` | LoopAnalyzer |
| `FlatAffineValueConstraints` | Polyhedral reasoning |

### Available but NOT currently used (SDE unlocks)

| MLIR Pass | What It Does |
|---|---|
| `affine-loop-fusion` | Producer-consumer & sibling fusion with cost model |
| `affine-loop-tile` | Cache-oblivious tiling with parametric sizes |
| `affine-loop-interchange` | Valid loop permutation with dep checking |
| `affine-parallelize` | Detect and mark parallel loops |
| `affine-scalrep` | Store-to-load forwarding |
| `sinkSequentialLoops` | Maximize parallelism |

---

## Vector: Codegen Layer (Stages 17-18)

Replaces current `LoopVectorizationHints` approach (LLVM metadata).

| Vector Op | Use Case | Replaces |
|---|---|---|
| `vector.transfer_read` | Stencil halo loads with masking | Scalar boundary code |
| `vector.transfer_write` | Masked writes for boundaries | Scalar boundary code |
| `vector.contract` | Matmul inner loops | LLVM auto-vectorization |
| `vector-to-llvm` | Native SIMD codegen | LLVM backend vectorizer |

---

## MLIR Interfaces

Current branch status:
- `LoopLikeOpInterface` is implemented on `arts_sde.su_iterate`.
- `DestinationStyleOpInterface` and `RegionBranchOpInterface` are still planned,
  not implemented on current SDE ops.

| Interface | On Which Ops | Purpose |
|---|---|---|
| `DestinationStyleOpInterface` | Planned, not current | Future ins/outs notation on SDE CU ops themselves |
| `MemoryEffectsOpInterface` | All CU/SU/MU ops | Per-op effect declarations |
| `LoopLikeOpInterface` | `sde.su_iterate` | iter_args state flow (tensor or memref values) |
| `RecursiveMemoryEffects` | `sde.cu_region`, `sde.cu_task` | Propagate body effects |
| `RegionBranchOpInterface` | Planned, not current | Future control-flow modeling for SDE regions |

---

## Dialect Availability

All 40+ MLIR dialects are registered via `registerAllDialects()` in
`tools/compile/Compile.cpp` (line 610). No additional linking needed for
linalg, tensor, bufferization, affine, or vector.
