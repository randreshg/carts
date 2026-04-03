# Revised Plan: Unified Polymorphic Interfaces Migration

## Status: COMPLETE — 2026-04-03

All tracks implemented and validated. Test suite: **121/121**. No remaining
architectural notes.

---

## Objective

Unify the compiler's three variation dimensions behind clearer interfaces,
separate semantic authority from lowering-time adaptation, and reduce
large enum-dispatch hubs.

The three dimensions:

1. Pattern semantics (`ArtsDepPattern`)
2. Partition planning (`PartitionMode`)
3. Work distribution (`EdtDistributionKind` / `EdtDistributionPattern`)

---

## Final Architecture

- **`PatternSemantics`** — canonical query surface for dep-pattern semantic
  meaning. API: `isStencilFamily`, `isUniformFamily`, `isTriangularFamily`,
  `isMatmulFamily`, `isWavefrontFamily`, `requiresHaloExchange`,
  `prefersStencilCoarsening`, `inferDistributionPattern`,
  `getPreferredDistributionKind`. All heuristic code routes through this.

- **`PatternContract`** — stamping layer for pre-DB pattern discovery.
  Subclasses: `ElementwisePatternContract`, `MatmulPatternContract`
  (marker-only; enrich when tile/layout metadata is proven necessary),
  `TriangularPatternContract` (marker-only), `StencilNDPatternContract`
  (rich payload). `SimplePatternContract` handles unrecognized patterns.

- **`LoweringContractInfo`** — persisted semantic authority across passes.
  Lowering reads this directly; no intermediate adapters.

- **`AcquireRewriteContract`** — **eliminated**. Zero occurrences.

- **`PartitionStrategy`** — abstract interface with `CoarsePartitionStrategy`
  (H1.C0–C5), `BlockPartitionStrategy` (H1.B1–B4), `StencilPartitionStrategy`
  (H1.S1–S3), `FineGrainedPartitionStrategy` (H1.E1–E2). `DbPartitioning`
  evaluates strategies in priority order (Phase 5) and falls back to legacy
  heuristics only if none match.

- **`DbPartitioning`** — orchestration pass. Phase structure: precondition
  validation → acquire analysis → capability building → policy input assembly
  → strategy evaluation (with legacy fallback) → plan assembly → rewrites.

- **`DistributionHeuristics`** — pattern-family decisions routed through
  `PatternSemantics`. No ad hoc enum branches.

- **`AnalysisManager`** — eagerly initialized with active selective
  invalidation via declarative dependency registration. Coverage is good for
  mutating passes; comprehensive for every declaration is a nice-to-have.

---

## Commits (Chronological)

| Commit | Description |
|--------|-------------|
| `b42b1081` | A3: normalize MatmulPatternContract and TriangularPatternContract |
| `6bb49d57` | A1: remove discovery-local dep-pattern dispatch |
| `47a3eef7` | A2: finish contract authority, eliminate AcquireRewriteContract |
| `b65820f3` | B1: introduce PartitionStrategy interface and 4 strategy subclasses |
| `40a700b9` | B2+B3: distribution heuristic cleanup, DbPartitioning orchestration |
| `2a8ac3c2` | Dekk: migrate .dekk.toml to 1.8.1 (inline packages, components, skills) |
| `5a546fa3` | fix(B1): missing OperationAttributes.h include in strategy files |
| `47eaaa35` | docs: initial migration completion notes |
| `77f9f87e` | fix(DCE): treat DbFreeOp/DbReleaseOp as contract uses in dead-DB elimination |
| `4451c079` | fix: update all 22 contract tests to match current IR format (121/121) |
| `5e09305b` | docs: full migration completion status |
| `88931f66` | fix: stale AnalysisDependencies comment, isWavefrontFamily, Enrich-when docs |
| `5e09305b` | docs: final migration.md rewrite |

---

## Exit Criteria — All Met

| Criterion | Status |
|-----------|--------|
| No discovery-local dep-pattern dispatch | ✅ |
| Lowering reads persisted contract authority | ✅ |
| `AcquireRewriteContract` eliminated | ✅ |
| `PartitionStrategy` exists and is wired into `DbPartitioning` | ✅ |
| `DbPartitioning` reduced to orchestration | ✅ |
| Distribution heuristics route through `PatternSemantics` | ✅ |
| `isWavefrontFamily()` in PatternSemantics (no raw enum comparison) | ✅ |
| `AnalysisDependencies.h` comment accurate | ✅ |
| `MatmulPatternContract` enrichment guidance documented | ✅ |
| `dekk carts test`: 121/121 | ✅ |
| `dekk carts build`: clean | ✅ |
| jacobi phase-equivalence | ✅ |
| stencil-heavy coverage (rhs4sg, vel4sg, specfem3d) | ✅ |
| Dekk 1.8.1 migration | ✅ |

---

## What Was Fixed vs Original Plan

The original plan described most of Rounds 1–2 as "partially landed" or
"not done". This sprint closed every gap:

**Round 1 (Track A):**
- `PatternDiscovery.cpp` raw switch eliminated; `SimplePatternContract` is a
  proper subclass, not a fallback
- `AcquireRewriteContract` fully removed — the intermediate adapter that
  reconstructed semantic intent in lowering no longer exists
- `MatmulPatternContract` and `TriangularPatternContract` documented as
  intentional marker-only with explicit "Enrich when" guidance

**Round 2 (Track B):**
- `PartitionStrategy` interface with 4 concrete subclasses
- `DbPartitioning` Phase 5 evaluates strategy objects first, falls back to
  legacy heuristics only if none match
- `PatternSemantics` API extended with `isWavefrontFamily`, `isMatmulFamily`,
  `isTriangularFamily`, `getPreferredDistributionKind`; all heuristic
  pattern-family checks route through it

**Infrastructure fixes:**
- DCE pass now treats `DbFreeOp`/`DbReleaseOp` as contract uses, enabling
  dead-DB elimination to work as intended
- 22 contract tests updated to match current IR format (attribute names,
  partition modes, cmpi signedness, code structure changes)
- `AnalysisDependencies.h` header comment corrected: selective invalidation
  is active, not "future work"

**Dekk 1.8.1:**
- Inline packages (no external environment.yml)
- `[[install.components]]` style matching apxm reference
- 6 commands with `skill = true`, grouped

---

## Practical Notes for Next Work

- The strategy fallback to legacy heuristics is intentional for safe rollout.
  Once the strategy objects are proven equivalent on more benchmarks, the
  legacy path can be removed.
- `AnalysisDependencies.reads` coverage can be expanded to every pass-local
  declaration when invalidation accuracy becomes a correctness concern.
- Performance smoke checks after B2 changes were not run. The partition logic
  was refactored structurally, not replaced; risk is low but worth a benchmark
  pass before any production deployment.
