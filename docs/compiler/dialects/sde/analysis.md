# SDE Analysis and Utils Substrate

The Analysis and Utils layers are the shared, target-neutral query/geometry
substrate consumed by the SDE Transforms and Verify passes. The core design
discipline is **classifier/transform pairing**: a transform and its verifier
read the *identical* helper, so they cannot silently diverge. SDE analysis
facts are consumed by SDE transforms and SDE→ARTS conversion only; they must not
become hidden ARTS analysis inputs.

## Analysis (`lib/carts/dialect/sde/Analysis/`)

| Component | Computes | Notes |
| --- | --- | --- |
| `SuLoopAccessAnalysis` (1433 lines) | Per-`su_iterate` structured facts (reads/writes, affine maps, iterator types, classification) and module-wide per-array access profiles; matmul contraction-tiling and owner-strip predicates. | Pattern-free; built from affine maps + iterator types + static shapes. Strictly SDE-internal (no ARTS consumer). |
| `LayoutGraph` (538 lines) | The **live** flat `LayoutGraphFact` parser (`parseArrayLayoutFacts`), `assignStableArrayIds`, `collectCuMuHyperedgePressures`. | Also defines a typed `CuVertex`/`MuNet`/`MuPin` graph builder that is **dead** (zero external callers). |
| `RedistributionEdges` (718 lines) | Resolves each `layoutsDisagree` marker into a concrete `RedistributionEdge` or a fail-closed failure. | Reads committed facts verbatim; never recomputes owner dims/block shape/family. |
| `AccessWindowSync` (177 lines) | `classifyBarrierSync` → verdict (Justified / Redundant / Misaligned / RankMismatch / Unprovable / Malformed / OutOfScope) over raised per-CU access windows; `partitionBarrierPhases`. | Shared by `sde-mu-access-window-sync-opt` and its verifier. |
| `AffineAccessUtils` (140 lines) | Decomposes scalarized linearized index expressions back into 2-D outer/inner/stride coords. | Only consumer is the `sde-memref-normalization` conversion (input side). |
| `SdeAnalysisUtils` (header) | Small widely-used helpers, e.g. `getSuIterateComputeBlock`. | The one Analysis-layer header also read across the SDE→ARTS boundary. |

## Utils (`lib/carts/dialect/sde/Utils/`)

| Component | Provides |
| --- | --- |
| `MuLayout` (185) | `resolveMuPhysicalLayout` (committed `physicalOwnerDims/BlockShape` → canonical layout, fail-closed on dynamic/illegal grain), `buildExpandedMuType`, `recoverOwnerDims` (inverse proof obligation). |
| `MuLayoutRewriter` (815) | Applies a committed block-grid layout to one `mu_alloc`: rank-expands the type, rewrites loads/stores via a mode-specific HPF div/mod `MuAccessIndexer`, or fails closed leaving IR unmutated. Hosts shared `isBlockGridRealizable`/`recognizeExpandedBlockGridMu`/`findCommittedBlockLayoutWriter`. |
| `MuAccessWindow` (325) | `queryAccessWindows` — the single shared scope+geometry gate for the raise transform **and** its verifier. |
| `CuMuGraphPartitioning` (616) | `chooseCuMuGraphPartition` (MU block shape from a halving spine + divisor/fanout samples) and `computeCuMuHypergraphCutBytes` (weight·(λ−1) cut). |
| `SDECostModel` (138, header-only) | Pure-virtual target-agnostic cost interface (task/sync/reduction/atomic/data-access cost, scheduling overhead, logical worker capacity, locality groups, vector width, tile-byte floor). The concrete impl is supplied at the dialect boundary; SDE passes see only the interface. |
| `IterationSizingUtils` (326) | Trip-count + logical-worker sizing math (`factorWorkersAcrossDims`, `enforceOwnerBlockConcurrencyFloor`, `buildBlockAlignedLogicalWorkerSlice`, `coarsenWorkersToTileByteFloor`). Preserves storage grain while letting SDE commit a coarser CU dispatch shape. |
| `SdeCuStructure` (190, header) | The structural predicates `isCuOp`/`isSuOp`/`isSourceComputeOp`/`isSchedulePlumbing` — the definition of CU vs SU vs source work. |
| `SdeCommittedFactUtils` (373, header) | Reconciles committed CU/MU facts: writer→reader shape propagation, partial-reduction owner-dim reconciliation, `requiresNestedStencilOwnerPromotion`. |

## What is wired vs. dead (verify before relying on either)

- **Live MU rank-expansion stack** — `MuLayout` + `MuLayoutRewriter` +
  `MuAccessWindow` are exercised by `sde-rank-expand-mu` (wired at
  `Compile.cpp:1168`), `sde-coarse-avoidance`, and their verifiers.
  `physicalBlockShape` facts are realized into memref types in this tree.
- **Live partition path** — `chooseCuMuGraphPartition` is called from
  `distribution-planning` (`DistributionPlanning.cpp:1010`); the executed scorer
  is the **per-edge fanout-pressure heuristic** (`collectCuMuHyperedgePressures`
  → scored at `CuMuGraphPartitioning.cpp:479-501`).
- **Dead at runtime** — the typed weight·(λ−1) **hypergraph cut**
  (`computeCuMuHypergraphCutBytes`, `refineCuMuAssignment`) only runs when
  `memory.typedHypergraph.vertices/.nets` are populated, which the sole caller
  never does. So full hypergraph *partition refinement* is not exercised today —
  only the fanout heuristic is. This nuances the charter's "hypergraph model for
  CU grouping/partition evidence": the entry point is wired, the cut metric is
  not.
- **Dead code** — the typed `LayoutGraph` builder (`buildLayoutGraph`,
  `CuVertex`/`MuNet`/`MuPin`) has zero external callers.
- `SDECostModel` defaults `getMinDistributedTileBytes()` to 0 and
  `getWorkerLocalityGroupCount()` to 1, so the tile-byte floor and multi-locality
  task-wave logic are inert unless the boundary cost model overrides them.

## Emitted facts

Committed on `su_iterate` / `mu_alloc` and consumed downstream:
`structuredClassification`, `pattern`, owner/spatial/reduction dims,
`physicalOwnerDims`/`physicalBlockShape`/`physicalHaloShape`,
`logicalWorkerSlice`, the `arrayLayout` dictionary
(`arrayId`/`kind`/`ownerDims`/`blockShape`/`muBlockCount`),
`layoutsDisagree`, and abstract `commVolumeBytes`. These are SDE structural
outputs, not hidden ARTS inputs.
