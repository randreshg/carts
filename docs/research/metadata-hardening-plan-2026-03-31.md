# Metadata Hardening Plan

Date: 2026-03-31

## Decision

Yes: the metadata subsystem needs a broader revision, not just one more local
guard.

The activations `tripCount=99` corruption is a symptom of a systemic issue:
metadata identity, provenance, attachment, and semantic validity are currently
coupled too loosely. The compiler can still produce correct IR in many cases,
but metadata-driven optimizations are not yet operating under a strong enough
contract to be trusted as the basis for distribution, partitioning, and cost
models.

This does **not** mean "rewrite everything first." It means:

1. contain the current failure mode immediately,
2. introduce a stricter metadata contract,
3. migrate loop-producing rewrites onto that contract,
4. only then expand metadata-driven optimization scope.

## Audit Summary

### What is working

- Metadata collection itself is reasonably structured:
  - `CollectMetadataPass`
  - `MetadataManager`
  - `MetadataRegistry`
  - `MetadataIO`
- Some rewrites already copy metadata intentionally:
  - `LoopNormalizer::copyLoopAttributes`
  - `copyArtsMetadataAttrs`
  - targeted loop/matmul/stencil transforms
- There is already a `VerifyMetadata` pass.

### What is not strong enough

#### 1. Identity is not a first-class rewrite contract

`arts.id` is used as if it were both:

- a stable entity identity,
- and a recoverable attachment hint.

Those are different concerns.

Right now a transformed loop may:

- preserve `arts.id`,
- preserve only location,
- preserve neither,
- or receive a new ID unrelated to semantic ancestry.

That is too weak for a metadata-driven optimizer.

#### 2. Loop rewrites rely on best-effort copying or late recovery

Memref rewrites already have several explicit transfer paths. Loop rewrites are
much more ad hoc:

- some transforms copy `arts.id` and `arts.loop`,
- some only copy selected attrs,
- some create new loops with no metadata,
- later passes recover metadata heuristically from JSON cache.

The activations bug came from exactly this model.

#### 3. Recovery is heuristic, but consumers treat the result as authoritative

`MetadataAttacher` currently tries to recover loop metadata by:

- exact key match,
- ID match,
- location match,
- near-location scoring.

That is acceptable only as a last-resort diagnostic recovery mechanism.
It is not strong enough to serve as the primary transport mechanism for
optimization-critical metadata.

#### 4. Source facts and transformed facts are conflated

`arts.loop` currently mixes:

- source-anchored information:
  - original location
  - original nesting
- semantic facts:
  - reductions
  - parallel classification
- transformed-state facts:
  - current trip count
  - current loop shape

After rewriting a loop, some of those fields should be preserved, some should
be recomputed, and some should be invalidated. Today that distinction is not
encoded explicitly.

#### 5. Validation checks coverage, not integrity

`VerifyMetadata` verifies that metadata exists.

It does **not** verify:

- trip count matches static bounds when known,
- location/provenance is coherent after rewrites,
- `arts.id` uniqueness and ancestry invariants,
- copied metadata is still semantically valid,
- consumers are using only validated fields.

Coverage is necessary. It is not sufficient.

#### 6. Rewrite-heavy passes are not all operating under one metadata API

We currently have multiple manual mechanisms:

- `copyArtsMetadataAttrs`
- `copyLoopAttributes`
- manual `setAttr(...)`
- opportunistic `ensureLoopMetadata(...)`

This is below the bar for a compiler whose optimization decisions depend on
metadata. There should be one rewrite contract layer, not scattered conventions.

## Concrete Root Cause: Activations

The activations case is useful because it shows the entire failure chain:

1. `activate_tanh` becomes an `arts.for` with no stable loop metadata attached
   after early ARTS conversion.
2. later pattern/kernel rewrites preserve structure but not enough provenance
   for exact metadata transport.
3. `MetadataAttacher` reattaches loop metadata heuristically.
4. a softmax metadata entry with `tripCount = 99` is close enough to be
   selected for a different transformed loop whose real trip count is
   `1048576`.
5. `DistributionHeuristics` consumes that value as if it were exact.
6. `DbPartitioning` propagates the bad hint across multiple allocations.

This is not "one bad comparison." It is a pipeline design gap.

## Industry-Level Target Model

The subsystem should be restructured around four separate concepts.

### 1. Entity identity

Every optimization-relevant loop and allocation should have a stable identity
that is independent of attachment heuristics.

Proposed split:

- `arts.entity_id`
  - stable identity for the current semantic entity
- `arts.origin_id`
  - source or parent identity from which the current entity was derived
- `arts.provenance_kind`
  - `exact`
  - `transferred`
  - `recomputed`
  - `recovered`

`arts.id` can remain as compatibility sugar during migration, but the internal
model should stop pretending one integer is enough to represent identity,
ancestry, and recovery all at once.

### 2. Source anchor

Source location should be treated as a source anchor, not as loop identity.

Keep:

- original source file/line/column,
- optional callsite chain,
- optional original loop nest ordinal.

Do not use source location alone as the primary mechanism for semantics
transport after loop synthesis/fusion.

### 3. Semantic facts

Split loop metadata into:

- invariant or source-family facts
  - reduction kind family
  - access/dependence family
  - original source anchor
- current IR facts
  - current static trip count
  - current loop rank
  - current bounds kind
  - current reduction status

Fields that depend on the transformed IR must either be:

- recomputed immediately after the rewrite,
- or marked invalid until recomputed.

They must not be copied blindly.

### 4. Confidence / validity

Consumers should know whether a field is:

- exact,
- recomputed from current IR,
- inherited but not revalidated,
- recovered heuristically.

Safety-critical decisions should use only exact or recomputed facts.
Heuristic recovery may still be useful for diagnostics, debug output, and
non-critical hints.

## Required Architectural Changes

### A. Introduce a metadata rewrite API

Add a single shared API for loop-producing transforms.

Example responsibilities:

- create replacement loop with carried identity/provenance,
- copy only safe attrs,
- invalidate stale fields,
- recompute cheap structural facts immediately,
- record ancestry links,
- optionally request full recomputation later.

Candidate entry points:

- `MetadataManager::cloneLoopMetadataForRewrite(...)`
- `MetadataManager::replaceLoopEntity(...)`
- `MetadataManager::recomputeLoopFacts(...)`
- `MetadataManager::markRecoveredMetadata(...)`

No transform should manually decide field-by-field semantics unless the helper
explicitly asks for it.

### B. Strengthen early ARTS conversion

`ConvertOpenMPToArts` should not merely copy attrs if they happen to exist.
It should force source loop metadata materialization before rewriting, then
transfer it through the rewrite API.

That closes the current gap where many `arts.for` loops enter the pipeline with
no metadata and later depend on heuristic reattachment.

### C. Demote JSON reattachment to fallback status

JSON/cache reattachment should become:

- a recovery path,
- a diagnostic support mechanism,
- a migration bridge.

It should not be the normal transport path for rewrite-heavy optimization
stages.

If recovery remains, it should use an explicit compatibility score model and
must never silently overwrite exact/provenance-rich metadata.

### D. Add integrity verification

Create a stronger metadata integrity pass, separate from simple coverage.

It should verify at least:

- no duplicate active entity IDs,
- loop trip count matches static bounds when statically known,
- transferred metadata fields are legal for the current loop op,
- recovered metadata is flagged as recovered,
- invalid fields are not consumed by cost models,
- transformed loops without provenance are rejected in strict mode.

Suggested name:

- `verify-metadata-integrity`

### E. Harden optimization consumers

Heuristic and lowering code should not consume raw `arts.loop` as if all fields
have the same trust level.

For example:

- distribution heuristics
- partitioning heuristics
- loop hint generation
- epoch profitability checks

should prefer:

- exact current IR facts,
- recomputed facts,
- validated metadata

and only use recovered metadata for soft ranking, never hard legality.

## Execution Plan

### Phase 0: Immediate containment

Goal: stop wrong metadata from poisoning optimization decisions.

1. Keep the current trip-count compatibility guard in attachment/recovery.
2. Add a focused activations regression covering the stale-loop-metadata case.
3. Add a strict-mode integrity check for "known static bounds disagree with
   metadata trip count".
4. Gate metadata-sensitive heuristics so they fall back to direct IR facts when
   metadata confidence is insufficient.

### Phase 1: Rewrite contract layer

Goal: make loop-producing rewrites preserve provenance explicitly.

1. Add rewrite helpers in `MetadataManager`.
2. Migrate:
   - `ConvertOpenMPToArts`
   - `ElementwisePipelinePattern`
   - `LoopReordering`
   - `PerfectNestLinearizationPattern`
   - `Seidel2DWavefrontPattern`
   - `DistributedHostLoopOutlining`
   - `MatmulReductionPattern`
3. Remove handwritten attr copying where the helper can own semantics.

### Phase 2: Metadata model split

Goal: separate source anchor, current facts, and provenance state.

1. Extend loop metadata representation.
2. Mark fields as:
   - preserved,
   - recomputed,
   - invalidated,
   - recovered.
3. Update export/import format without breaking current workflows.

### Phase 3: Integrity pass

Goal: detect semantic metadata bugs immediately.

1. Implement `verify-metadata-integrity`.
2. Run it in:
   - `--diagnose`
   - selected contract tests
   - metadata-sensitive benchmark triage

### Phase 4: Consumer migration

Goal: ensure cost models use trusted facts only.

1. Audit metadata consumers.
2. Replace direct raw-attr reads with validated query helpers.
3. Prefer direct IR-derived facts where cheaper and more reliable.

### Phase 5: Retire heuristic-first attachment

Goal: make recovery exceptional rather than routine.

1. Track recovery counts in diagnostics.
2. Fail strict-mode builds when recovery is unexpectedly required in
   rewrite-owned stages.
3. Reduce near-location matching to debug-only or last-resort workflows.

## Non-Negotiable Invariants

These should become explicit policy:

1. No loop-producing rewrite may leave optimization-relevant loops without
   either:
   - transferred provenance,
   - or recomputed metadata.
2. No consumer may treat recovered metadata as hard legality.
3. No copied trip count may survive a rewrite unless it still matches current
   static bounds.
4. Source location is an anchor, not identity.
5. Metadata verification must check semantic integrity, not just presence.

## Recommended Immediate Work Order

1. Land the tactical activations containment fix.
2. Add the activations contract test.
3. Add rewrite-helper APIs for loop replacement.
4. Migrate `ConvertOpenMPToArts` first.
5. Migrate `ElementwisePipelinePattern` second.
6. Add `verify-metadata-integrity`.
7. Audit all remaining loop-synthesizing transforms.

## Recommendation

Do not attempt a one-shot rewrite of the entire metadata subsystem.

Do perform a staged hardening pass that upgrades the metadata layer from
"best-effort attachment plus heuristic recovery" to "explicit rewrite
provenance plus integrity validation."

That is the minimum standard required if benchmark optimization decisions are
going to depend on metadata and small cost models instead of hardcoded values.
