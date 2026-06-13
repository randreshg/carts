# ARTS Realizer for `su.all_to_all` (genuine cross-owner repartition)

> **Vocabulary note (gap audit).** This doc was written against
> `sde.redist family=all_to_all_like` + `commVolumeBytes` — both **deleted** by the
> revision (movement is the `su.all_to_all` **op**; there is no `commVolumeBytes`).
> The *realization mechanism* (reader-pull per-target-block single-writer gather,
> two-tier order-preserving/permuted, the RMA reuse) is unchanged and correct; only
> the SDE→ARTS *mapping* must read the `su.all_to_all` op (operand = source grid,
> result = target grid). Sequence after `architecture.md` step **S11** (redist op
> retired), as step **S20**.

Companion to [`design-revision.md`](./design-revision.md). Makes the
`all_to_all_like` movement family — source owner dims ≠ target owner dims, the
genuine repartition that closes the self-edge for 3mm/2mm/atax/bicg — realizable
in ARTS, lifting the single hard-error at `SdeToArtsBoundary.cpp:441-444`.
Design + adversarial review; the verdict below is the **review-corrected** one.

## Verdict

**Realizable — in two tiers:**

1. **Order-preserving block-regrain** (owner dims stay the leading prefix; the
   coarse→block case that motivates 3mm/2mm/atax/bicg) — realizable **now** as a
   boundary-pass addition + small enum extension. No new transport, no new
   owner-map math.
2. **Permuted / transpose repartition** (owner dims reorder) — needs **one new
   piece of ARTS IR**: an explicit owner-dims attribute on `DbAllocOp`. Until that
   lands the realizer **fails closed with evidence** on permutations (never
   coarsens to a host gather). This is the "extend ARTS" path, now planned rather
   than punted.

The SDE side is already complete: `SdeRedistOp` carries
`source/targetOwnerDims/BlockShape` as I64 `ArrayAttr`s and names no
collective/DB/EDT/route (`SdeOps.td:738-793`); the verifier *requires*
partitioned-both-sides + `sourceOwnerDims != targetOwnerDims`
(`SdeOps.cpp:1215-1222`). SDE stays purely geometric; ARTS does all realization.

## Why it was punted

`validateAndCollectStorageRedists` accepts only `halo_like` and
`reduce_scatter_like` — both of which *force* `source == target`
(`SdeToArtsBoundary.cpp:429-437,456-464,1547-1551`) — and rejects every other
family with "requires a real ARTS realization" (`:441-447`). `all_to_all_like`
falls into that catch-all. That hard-error is the **single** structural block.

## SDE → ARTS mapping

```
sde.redist family=all_to_all_like  mu:memref<P>  arrayId, commVolumeBytes
   sourceOwnerDims/sourceBlockShape  (producer grid)
   targetOwnerDims/targetBlockShape  (consumer grid)   target != source
        │  buildAllToAllRedistFacts  (new; peer of buildReduceScatterRedistFacts:1540)
        ▼  with the INVERSE equality gate (differing geometry REQUIRED, not forbidden)
AllToAllRedistFacts { arrayId, sourceOwnerDims/BlockShape, targetOwnerDims/BlockShape, costBytes }
        │
        ▼  ARTS structure, per target block:
   • TARGET DbAlloc on (targetOwnerDims, targetBlockShape)   [distinct alloc — NOT in-place]
   • one owner-routed internode writer EDT per target block  [single writer]
   • that EDT reads (RO) the covering SOURCE blocks from the SOURCE DbAlloc
```

Add an `all_to_all_like` arm in `validateAndCollectStorageRedists` **before** the
`:441` catch-all; keep the `source==target` re-assertions scoped to
`halo_like`/`reduce_scatter_like` (do not delete them — other families still need
them).

## The ARTS realization

- **Two distinct DBs.** A repartition is movement between two distinct distributed
  DB grids of the same logical array. The realizer **must not reuse the source DB**
  (owner routes are single-grid). Source DB keeps the producer grid; a **new**
  target `DbAlloc` carries the consumer grid. Each passes
  `evaluateDistributedDbEligibility` independently.
- **Reader-pull communication CU (the charter "grouped communication CU").** For
  each target block: exactly one `out`-mode writer EDT (the target-block **owner**)
  whose dependency list is the RO `DbAcquire`s of the covering source blocks. The
  dependency list *is* the many-to-many fan-in. Many RO readers of a source block
  are fine; only the per-target **write** is unique.
- **Per-target-block source enumeration (the real new logic).** A new helper (peer
  of `analyzeDepOwnerAccessIndex`/`getAccessWindowPayloadDim`) computes, for a
  target block range, the covering source block range as an affine *range*
  relation. Where extents aren't integer multiples, target blocks partially overlap
  boundary source blocks, so the acquires must carry **element-grain**
  `element_offsets/element_sizes` — this window math is the real correctness
  surface (an off-by-one silently corrupts; transport trusts the window).
- **EDT wiring — reuse.** Writer EDT via `resolveArtsLaunchPolicy` →
  `EdtConcurrency::internode` + ordinal owner route (satisfies
  `verifyCdagWriterLaunch`); `ensureDistributedWriterOwnerLocalGroups` /
  `ownerLocalWriterSplit` on the **write** only; `DbCommitDistributedDeps` stamps
  runtime DB mode automatically once the acquires exist (no change).

## Correctness (review-corrected)

- **Single-writer-per-target-block.** Holds by construction (distinct target
  alloc, one owner per target block under its grid). **But `verifyCdagSwmr`
  silently SKIPS dynamic block indices** (`VerifyArtsCdag.cpp:381-383` — `blockKey`
  → `nullopt` ⇒ waved through, *not* failed). So the realizer must (a) emit
  **fold-constant** target block indices and (b) add its **own positive
  fail-closed check** — never rely on SWMR as the backstop on the dangerous input.
- **Epoch separation is an SDE precondition, not an ARTS toggle.** The invariant
  only holds across epochs, and `CreateEpochs` builds epochs from **committed
  barriers**, not from a `distribution_pattern` attribute. So **SDE must commit a
  `su_barrier`** bracketing the repartition (producer → repartition → consumer)
  so the three-epoch chain materializes. State as an SDE precondition.
- **Deadlock-freedom requires a checked invariant: no remote writes, ever — all
  movement is target-owner RO-pull.** The acyclic chain (producer → repartition →
  consumer) is checkable (`verifyCdagHappensBefore`); the transport is pull-based
  with a Medium-AM escape at every RMA branch. This holds **only** while strictly
  reader-pull; the moment fan-out grouping introduces a remote *push* edge, a
  blocking `gex_RMA_PutBlocking` on a serving progress thread can cycle. Enforce
  "no remote write edges" on the emitted graph.
- **Cross-node remote-writer coherence — discharged, not hand-waved.** The known
  2N blocker (reverted fix 3cec95a) is the *remote-writer* direction. Reader-pull
  converts it to **owner-local write + remote RO read**: the target owner writes
  locally (`db.c` owner-local fast path, the gemm/jacobi 2N-validated path) and
  fetches source RO via the validated serve path. The hard obligation becomes the
  already-validated one.

## The two tiers, precisely

- **Tier 1 (now): order-preserving.** `targetOwnerDims` is a **leading prefix**
  (owner order unchanged; only block grain regroups). `DbAllocOp`'s
  `makeLeadingDbOwnerDims` derivation suffices — the owner map falls out of the
  target grid. Covers the coarse→block self-edge fix. **Build this first.**
- **Tier 2 (extend ARTS): permuted/transpose.** `targetOwnerDims` reorders dims.
  `DbAllocOp` **cannot** encode non-leading owner dims today
  (`DistributedDbPlacementUtils.h:240-250`). The extension: add an explicit
  **owner-dims attribute** to `DbAllocOp` and teach
  `deriveDbOwnerRouteFactsFromDbGrid` to honor it (new ARTS IR + util surface).
  Until then, **fail closed on permutations** with a located diagnostic.

## Cousins (degenerate cases of the same realizer)

- `su.broadcast` (1→N): source replicated → every target writer reads the one
  source DB.
- `su.gather` (N→whole): target replicated → one writer reads all source blocks
  (structurally `reduce_scatter`'s read-all without the collapse).
- `su.scatter` (whole→N): source replicated, target partitioned → each target
  writer slices the single source. Correctness primitives, not scaling wins;
  shared realizer shape, kept as separate SDE-gated roadmap ops.

## New code vs pure reuse (corrected)

**New code** (the bulk is the EDT-graph emitter — *not* a `reduce_scatter`
parameterization):

| Item | Location |
| --- | --- |
| `AllToAllRedistFacts` + `buildAllToAllRedistFacts` (inverse equality gate) | `MovementLoweringUtils.h` / `MovementLoweringUtils.cpp` |
| `convertAllToAllMovement` + `realizeAllToAllMovements` | `MovementLoweringUtils.cpp` |
| `staticCoordsFromLinearIndex` (inverse of `staticLinearIndex`) | `DistributedDbPlacementUtils.h` |
| `createUnitBlockDbAcquireAtCoords` | `DbUtils.h` / `DbUtils.cpp` |
| `resolveBoundaryDbAlloc` | `DbBackedMemrefUtils.h` / `DbBackedMemrefUtils.cpp` |
| `buildWholeDbAcquireWindow` | `DbUtils.h` / `DbUtils.cpp` |
| `all_to_all_like` arm replacing the hard error | `SdeToArtsBoundary.cpp` before :441 |
| **per-target-block writer-EDT graph emitter** (enumerate target blocks → N writers → RO acquires of covering source blocks on the distinct source DB) + positive fold-constant fail-closed check | `SdeToArtsBoundary.cpp` (new) |
| **source sub-block range enumeration** (affine range + element-grain windows) | `SdeToArtsBoundary.cpp` peer of :1341 |
| owner-dim **permutation** dep-result-dim encoding **+ new ARTS-RT consumer** | `:742-789` extend + ARTS-RT (net-new both sides) |
| `repartition` case in `EdtDistributionPattern`/`ArtsDepPattern` + `RealizeEdtDistribution` stamp | `ArtsAttrs.td` + `RealizeEdtDistribution.cpp` |
| `perBlockSingleWriter` DbAlloc attr + `DbDistributedEligibility` arm | `ArtsOps.td` + `DbDistributedEligibility.cpp` |
| **Tier 2 only:** explicit owner-dims attr on `DbAllocOp` + route honoring | `ArtsOps.td` + `DistributedDbPlacementUtils.h` |
| **SDE side:** commit a `su_barrier` bracketing the repartition | SDE AccessWindowSync / redistribute |

**Pure reuse (no change):** RMA DB-move transport (OFFER/READY/DONE, segment
arena, SEND/FULL_SEND), `DbDistributedOwnershipRealization`,
`DbDistributedRuntimeInit`, `ensureDistributedWriterOwnerLocalGroups`,
`resolveArtsLaunchPolicy`, `DbCommitDistributedDeps`, the CDAG gates, and the
EW-promotion + `arts_serve_ro_readers` remote-RO path.

## Status / dependencies

- **No existing test coverage** — `all_to_all` appears only in the SDE op def +
  verifier (`grep -rln all_to_all` → 0 test files). The realizer is **lit-tested
  against hand-authored `sde.redist family=all_to_all_like`** with distinct
  source/target attrs.
- **End-to-end is blocked on the SDE producer:** `RedistributionEdges.cpp:502-503`
  hardcodes `targetOwnerDims = sourceOwnerDims`; until `design-revision.md` Step 5
  (layout-assignment commits the consumer's required read layout as the target
  type) lands, no real pipeline emits an `all_to_all` edge. The realizer makes the
  ARTS half ready; Step 5 makes it fire.
