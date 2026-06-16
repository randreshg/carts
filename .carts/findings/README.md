# CARTS distribution effort — unified status & doc index

> Single entry point. Updated 2026-06-16 after 1n medium SC-001 validation.

## Canonical state — `v4` @ `4fcf3ec57` + E2E session 2026-06-16

Line contains: correctness base (18/21 medium + 3 fail-closed) + Phase 11 split
(T027–T031) + US8 scalar-replacement guard (`56c516207`). Latest E2E validation:
`.carts/findings/e2e-validation-20260616.md`.

Build clean; lit **103/109** (6 jacobi/poisson boundary fails).

## What is still open

- **poisson-for medium 1n runtime:** **FIXED** (SDE alternating-buffer grain reconcile per-SU).
- **activations medium 1n regression:** **PASS** (sync intranode read-only heap scratch verify carve-out).
- **End-to-end US8 large perf:** T024–T026 deferred; compiler evidence only.
- **2 nodes (SC-002 / US2 T013):** deferred/out of scope for this 1n-only pass.

## What is done

- **1n MEDIUM correctness:** 18/21 Correct=YES + 3 precise fail-closed
  (atax/batchnorm cross-owner reduction, seidel-2d in-place wavefront). lit 79/79
  (dialect scope). On the correctness base, carried into recarve.
- **Phase 11 pass-split (US9, T027–T031):** DONE + VALIDATED on recarve.
  DistributionPlanning deleted and split into DistributionFailClosed /
  BlockGrainPlan / OwnerDimSelect / MovementTagging + DistributionLayoutUtils;
  RedistributionEdges → EdgeClassify + RankExpandedEdgeProject; boundary →
  CoarseSu + RawAccessVerify; LayoutAssignment two-pass; SuLoopAccessAnalysis
  4-way; DistributedLaunchConsistency → EdtSplitForMixedDeps + WriterOwnerRoute.
- **Pre-commit review (carts-simplify + carts-review + constitution):** ran as a
  40-agent ultracode audit; 0 high / 4 med / rest low, all behavior-preserving.
  Cleanups APPLIED: dead code (recordHomeLayout, requiresUnimplementedStencilWavefront
  wrapper, LaunchPolicyUtils include), stale DistributionPlanning refs, attr
  centralization (sde.layout_choice → SdeAttrNames.h). See `phase11-review-report.md`.

## Done (2026-06-15, committed on recarve)

- **2 review test-gaps: CLOSED.** `verify-raw-access-covered` (reject + accept
  cases + registration wiring) and `writer-owner-route` reject lit. Both
  self-validated + adversarially verified via a 7-agent workflow. Commits
  `1f46568f6`, `fe39778fc`.
- **large-perf alias fix: FOLDED.** `ff5edccca` AliasScopeGen
  noalias-through-pointer-table cherry-picked onto recarve (`5e5ccca78`). Builds
  clean; lit 93/99 (== base + 4 new pass).

## US8 — RESOLVED at the compiler level (commit 07a3c27e8)

The gated IR observation was done (via carts-compile directly, bypassing the
flaky runner): the dead-scope premise was WRONG (canonicalization already folds
them); the real cause was `detectReductionPattern` only matching memref/polygeist
load-store with non-empty indices, so the canonical 0-d scalar accumulator
(`affine.load/store %acc[]`) was never promoted. Fix extends it to affine ops +
0-d accumulators. RESULT: gemm MAC loop now promotes to scf.for iter_args and
clang vectorizes it (width 4, interleave 4; 105 AVX ops vs 0). Necessary partner:
the noalias fix (already folded).

## Doc index

| File | Purpose |
|---|---|
| `goal-large-perf.md` | Remaining-work goal prompt (US8 perf + large boundary bugs) |
| `phase11-review-report.md` | Pre-commit review findings (most applied; 2 test-gaps open) |
| `phase11-recarve-plan.md` | The executed split carve plan (historical reference) |
| `distribution-audit-2n.md` | Per-benchmark 2n root causes (spec reference) |
| `.carts/spec/specs/0001-distribution-1n-2n/` | Structured spec: spec/plan/tasks/research |

## Next steps

1. After the running medium sweep: rebuild recarve, re-lit, commit the review
   cleanups (carts-commit), land the 2 test-gaps.
2. Fold `wt/large-perf` into recarve; validate gemm large vectorizes.
3. Merge recarve → `v4`.
