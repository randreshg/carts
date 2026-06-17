# CARTS distribution effort - unified status and doc index

> Single entry point. Updated 2026-06-17 after the v4 worker-grain and compact
> halo fixes.

## Canonical state - current `v4`

Line contains the correctness base, Phase 11 split, US8 scalar-replacement
guard, worker-grain layout realization, clean SDE halo payload consumption,
compact halo graph pressure reduction, used-side compact halo packing, and the
2026-06-17 distribution-grain guidance cleanup. Latest medium E2E validation:
`.carts/findings/e2e-validation-20260616.md`.

Build and full-suite status should be refreshed before publication. The focused
guard for promoted owner dimensions in ARTS DB payloads passes:
`dekk carts lit lib/carts/dialect/arts/test/conversion/sde-to-arts-drops-promoted-owner-payload-dims.mlir -v`.
Fresh large GEMM `sde-to-arts` output on this compiler emits C as DB grid
`sizes[11,8]` with payload `elementSizes[437,600]`, not
`elementSizes[1,1,437,600]`.

## What is still open

- **poisson-for medium 1n runtime:** **FIXED** (SDE alternating-buffer grain reconcile per-SU).
- **activations medium 1n regression:** **PASS** (sync intranode read-only heap scratch verify carve-out).
- **End-to-end large perf:** still open. The promoted-owner DB payload shape is
  fixed, but large benchmark timing and cross-benchmark ARTS overhead remain to
  be remeasured on the current binary.
- **2 nodes (SC-002 / US2 T013):** deferred/out of scope for this 1n-only pass.

## What is done

- **1n MEDIUM correctness:** 21/21 registered benchmarks Correct=YES on
  2026-06-16 after tracked-WIP cleanup. No 2n, cluster, large, extralarge, or
  megalarge rows were run in this scoped pass.
- **Phase 11 pass-split (US9, T027-T031):** DONE + VALIDATED on recarve.
  DistributionPlanning deleted and split into DistributionFailClosed /
  BlockGrainPlan / OwnerDimSelect / MovementTagging + DistributionLayoutUtils;
  RedistributionEdges → EdgeClassify + RankExpandedEdgeProject; boundary →
  implementation units such as CoarseSu and RawAccessVerify while the production
  `sde-to-arts` pipeline stays three passes:
  `SdeStorageToArtsDb`, `SdeAccessesToArtsDeps`, `FinalizeSdeToArts`.
  LayoutAssignment two-pass; SuLoopAccessAnalysis 4-way;
  DistributedLaunchConsistency → EdtSplitForMixedDeps + WriterOwnerRoute.
- **Pre-commit review (carts-simplify + carts-review + constitution):** ran as a
  40-agent ultracode audit; 0 high / 4 med / rest low, all behavior-preserving.
  Cleanups APPLIED: dead code (recordHomeLayout, requiresUnimplementedStencilWavefront
  wrapper, LaunchPolicyUtils include), stale DistributionPlanning refs, attr
  centralization (sde.layout_choice → SdeAttrNames.h). The standalone
  pre-commit review report was superseded by the applied fixes and this index.

## Done (2026-06-15, committed on recarve)

- **2 review test-gaps: CLOSED.** `verify-raw-access-covered` (reject + accept
  cases + registration wiring) and `writer-owner-route` reject lit. Both
  self-validated + adversarially verified via a 7-agent workflow. Commits
  `1f46568f6`, `fe39778fc`.
- **large-perf alias fix: FOLDED.** `ff5edccca` AliasScopeGen
  noalias-through-pointer-table cherry-picked onto recarve (`5e5ccca78`). Builds
  clean; lit 93/99 (== base + 4 new pass).

## US8 - resolved at the compiler level

The gated IR observation was done (via carts-compile directly, bypassing the
flaky runner): the dead-scope premise was WRONG (canonicalization already folds
them); the real cause was `detectReductionPattern` only matching memref/polygeist
load-store with non-empty indices, so the canonical 0-d scalar accumulator
(`affine.load/store %acc[]`) was never promoted. Fix extends it to affine ops +
0-d accumulators. RESULT: gemm MAC loop now promotes to scf.for iter_args and
clang vectorizes it (width 4, interleave 4; 105 AVX ops vs 0). Necessary partner:
the noalias fix (already folded). The later guard keeps multi-read accumulators
from being promoted incorrectly.

## Doc index

| File | Purpose |
|---|---|
| `phase11-recarve-plan.md` | The executed split carve plan (historical reference) |
| `phase11-split-keep-decisions.md` | Final split/keep decisions after the recarve |
| `compiler-code-health-backlog.md` | Source-verified backlog; historical rows must be rechecked against current `v4` before reuse |
| `e2e-validation-20260616.md` | Latest committed 1-node medium validation evidence |

## Next steps

1. Re-run large representative benchmarks on the current binary and inspect
   `sde-planning`, `sde-to-arts`, `post-db-refinement`, and `pre-lowering`
   shape before changing runtime knobs.
2. Keep the SDE-to-ARTS production pipeline mechanical. Split implementation
   files only by responsibility; do not add boundary repair passes that
   rediscover SDE-owned partitioning, movement families, DB grain, or owner
   shape.
3. Run 2-node validation only when explicitly in scope and cluster access is
   available.
