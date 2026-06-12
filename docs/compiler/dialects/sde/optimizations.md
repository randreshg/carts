# SDE Optimization Catalog

SDE optimizations are semantic and target-neutral: they change real MU/CU/SU
shape (never stamp a metadata promise for a downstream layer) when SDE analyses
prove legality, and otherwise **fail closed with evidence**. This catalog is the
as-built map across the six value dimensions — **State, Dependency, Effect,
Compute, Memory, Sync** — with each lever's owning pass and status. Verified
against `cgo/main`.

A pass is `real-shape` (rewrites IR structure), `metadata` (sets an attr the
boundary binds), `dormant` (code exists but is gated off / unreachable by
default), or `missing` (no implementation).

## By value dimension

### State (memory state normalization, MU realization)
| Lever | Owning pass | Kind |
| --- | --- | --- |
| Realize shared memrefs as `sde.mu_alloc` storage | `sde-memory-unit-realization` | real-shape |
| Input-normalization cleanup (scalar forwarding + dead-state) | `scalar-forwarding` + `sde-dead-state-cleanup` | real-shape (DCE) |

### Dependency (parallel work exposure, layout, redistribution edges)
| Lever | Owning pass | Kind |
| --- | --- | --- |
| Pattern/access classification | `sde-loop-pattern-facts` | fact producer (load-bearing) |
| Raise dependence-free serial nests to SU schedule | `sde-parallelize` | real-shape (write-only inits only) |
| Make layout-disagreement edges explicit `sde.redist` | `sde-redistribute` | real-shape (today: `reduce_scatter_like` only) |

### Effect (schedule/chunk/reduction selection)
| Lever | Owning pass | Kind |
| --- | --- | --- |
| Refine abstract schedule → concrete | `schedule-refinement` | metadata |
| Synthesize chunk sizes | `chunk-opt` | metadata-leaning |
| Annotate atomic-vs-tree reduction | `reduction-strategy` | metadata |
| Realize `reduction_strategy(atomic)` → `sde.cu_atomic` | `sde-atomic-reduction-realization` | real-shape |
| Realize `reduction_strategy(tree)` → partials+combine | **missing** | the tree verdict is an unrealized promise |

### Compute (loop transforms, fusion, reductions, target)
| Lever | Owning pass | Kind |
| --- | --- | --- |
| Cost-model loop reorder | `loop-interchange` | real-shape |
| Fuse consecutive elementwise SUs → pipeline | `elementwise-fusion` | real-shape |
| Split interior vs boundary regions | `iteration-space-decomposition` | real-shape |
| Expose scalar checksum reduction as block partials | `sde-scalar-block-reduction` | real-shape |
| Propagate target-cpu/features (vectorization width) | `sde-promote-target-attrs` | metadata (load-bearing) |
| SDE-level vectorization (`vector.*` in `cu_work`) | **missing/undecided** | only ARTS-RT reads `vectorizeWidth` |

### Memory (block layout, grain as structure, grain reconciliation)
| Lever | Owning pass | Kind |
| --- | --- | --- |
| HPF per-array BLOCK layout + abstract comm-volume cost | `sde-layout-assignment` | committed fact → realized later |
| Budget-grain strip-mine / owner-local coarsening | `tiling` | real-shape (cross-unit) |
| Make block grain STRUCTURE via rank-expanded MU type | `sde-rank-expand-mu` | real-shape |
| Realize committed grain to avoid coarse MU | `sde-coarse-avoidance` | real-shape |
| CU-floor grouping via tile-floor partition selection | `chooseCuMuTileFloorPlan` | **dormant** (gated `getMinDistributedTileBytes()>0`, default 0) |
| Typed `LayoutGraph` build + balance/cut report | `buildLayoutGraph` | **dead** (0 callers) |
| Storage-grain reconciliation across CUs on one MU | partial in `tiling` | **missing** as a named pass |
| Chained-matmul intermediate block-native alignment | — | **missing** (round-trips a bridge copy) |

### Sync (barriers, distribution, wavefront, hypergraph cut)
| Lever | Owning pass | Kind |
| --- | --- | --- |
| Owner/block distribution planning + spatial wavefront-skew | `distribution-planning` | real-shape |
| Erase disjoint-set barriers + commit timestep stages | `barrier-elimination` | real-shape |
| Erase barriers proven redundant by MU access windows | `sde-mu-access-window-sync-opt` | real-shape |
| Raise per-CU MU access-window facts | `raise-to-mu-access-window` | additive fact (enables sync opt + ARTS deps) |
| Lambda-1 hypergraph cut + FM/KL CU placement | `refineCuMuAssignment` | **dormant/dead** (`typedHypergraph` never populated) |
| Temporal stencil scheduling (time-band / diamond / skew) | — | **missing** (only spatial skew exists) |

## Rules (engineering standard)

- Do not emit ARTS worker counts, routes, nodes, owner maps, or runtime calls.
- Do not tile only CU/SU loops without matching MU type + access rewrites.
- Do not use benchmark-specific constants.
- Read owner dims / block shape / movement family **verbatim** from committed
  facts; never recompute a different shape downstream.
- A new committed fact must ship with a paired `verify-sde-*` gate.

## The three highest-value gaps

1. **Activate the dormant partition engine.** The tile-floor selection and the
   lambda-1 hypergraph cut both exist but never run in default `-O3` (one gated
   on a never-set flag, one on a never-populated graph). This is the activation
   half of the distribution-quality work — see
   [`proposed-passes.md`](./proposed-passes.md) `sde-default-tile-floor` (P0) and
   `sde-activate-hypergraph-cu-grouping` (P0).
2. **Realize, don't promise, the tree reduction** (`reduction_strategy(tree)`
   is a downstream promise today) — charter violation; small fix reusing the
   `sde-scalar-block-reduction` emitter.
3. **Temporal stencil scheduling** is genuinely absent; iterative timestep
   stencils (jacobi-for/poisson-for) anti-scale through per-timestep host-whole
   bridges. The largest but highest-impact missing transform.

See [`proposed-passes.md`](./proposed-passes.md) for the ranked, fully-specified
design of each.
