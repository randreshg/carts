# Phase 11 Split/Keep Decisions (>1000-line files)

> US9 (T031) follow-up. Generated 2026-06-16 on `v4` after T027–T030 land.
> Applies the constitution split-axis test: split when two independent
> responsibilities share a file; keep when a single pipeline stage owns one
> coherent transform chain.

## Already split (DONE — keep carved units)

| Former monolith | Carved units | Verdict |
|-----------------|--------------|---------|
| `RedistributionEdges.cpp` | `EdgeClassify.cpp`, `RankExpandedEdgeProject.cpp` | **SPLIT DONE** — keep thin `collectRedistributionEdges` wrapper |
| `DistributionPlanning.cpp` | `OwnerDimSelect`, `BlockGrainPlan`, `MovementTagging`, `DistributionFailClosed` | **SPLIT DONE** — delete orchestrator stub |
| `SdeToArtsBoundaryDepAnalysis.cpp` | `CoarseSu`, `RawAccessVerify`, dep-from-window helpers | **SPLIT DONE** — keep shared geometry validators inline |
| `SdeToArtsBoundaryAccessLowering.cpp` | HaloLowering, EdtBuild, AccessRewrite, StandaloneCu | **SPLIT DONE** |
| `LayoutAssignment.cpp` | `LayoutCandidateChoose`, `WriterLayoutCommit` | **SPLIT DONE** |
| `SuLoopAccessAnalysis.cpp` | PerfectNestCollect, MemrefAccessCollect, StructuredClassify, NeighborhoodAnalysis | **SPLIT DONE** (file split; no pipeline inserts) |
| `DistributedLaunchConsistency.cpp` | `WriterOwnerRoute`, `EdtSplitForMixedDeps` | **SPLIT DONE** |

## Watch-list — split deferred (single coherent responsibility today)

| File | Lines (approx) | Keep rationale | Next split trigger |
|------|----------------|----------------|-------------------|
| `OwnerDimSelect.cpp` | ~1100 | One pass: physical layout commit family (stencil/uniform/matmul/reduction) shares `DistributionLayoutUtils` and one `runOnOperation` walk | Matmul grain committer needs its own pass when 2mm/3mm grain work lands |
| `SdeToArtsBoundaryAccessLowering.cpp` | ~1600 | Single lowering pipeline: dep collect → route → halo → EDT build → access rewrite; splits T030 landed but further split would fragment ordering invariants | Extract `BoundaryDepCollect` only if a third boundary consumer appears |
| `SdeToArtsBoundaryDepAnalysis.cpp` | ~1400 | Shared window geometry + coarse-SU dep collection; further split risks duplicating `deriveMuAccessWindowGeometry` contracts | Split reduce-scatter vs window-only when US5 adds contraction-specific verifiers |
| `MemrefNormalization.cpp` | ~1200 | One normalization spine for host memref cleanup before ARTS | Split only if EDT-scoped vs module-scoped normalization diverge |
| `Tiling.cpp` | ~1100 | SDE tiling pass owns nest shaping + tile-size selection together | Split when matmul vs stencil tiling policies need independent `.td` registration |
| `ValueAnalysis.cpp` | ~1000 | Shared memref root/strip utilities consumed across dialects | **KEEP** — utility namespace, not a pass; convert to free-function header over time |
| `tools/compile/Compile.cpp` | ~1200 | Pipeline registry — inherently orchestrator | **KEEP** — extract `buildSdePlanningPipeline()` to `SdePipeline.cpp` when a second compile entrypoint appears |

## Static god-class → namespace migration (deferred)

| Symbol cluster | Current home | Disposition |
|----------------|--------------|-------------|
| `ValueAnalysis` static methods | `ValueAnalysis.cpp` | Promote to `carts::analysis` free functions incrementally after US4 boundary stabilizes |
| `DbUtils` routing helpers | `DbUtils.cpp` | **KEEP** — ARTS runtime-facing; split only `resolveBoundaryDbAlloc` consumers |

## Decision summary

- **Split complete** for all Phase 11 T027–T031 targets named in `phase11-recarve-plan.md`.
- **Keep** remaining >1000-line files until a second independent pipeline consumer or pass registration is required.
- **Next carve candidate:** matmul-specific grain in `OwnerDimSelect.cpp` when 2mm/3mm US1 matmul grain lands.
