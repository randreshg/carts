# Op-Level Verification — Eliminating the SDE Verify Passes

Companion to [`design-revision.md`](./design-revision.md). Goal: **move SDE
verification to the op level, following the ARTS/EDT model**, eliminating the 8
standalone `ModuleOp` verify passes. Produced by a per-pass investigation +
adversarial review, grounded against the live tree. The honest achievable end
state is **8 passes → 2 (or 3 if one premise fails)** — not literally zero,
because two checks have no host op.

## The EDT model — three op-level homes

> **Correction (gap audit).** An earlier draft claimed "ARTS is the existence proof:
> zero standalone verify passes." That is **false** — it grepped only `include/` and
> missed `lib/carts/dialect/arts/Transforms/Verify/`: ARTS has `VerifyArtsCdag` +
> `VerifyArtsObjectsOnly` (two ModuleOp verify passes), and ARTS-RT has four
> (`verify-{pre,edt,db,epoch}-lowered`). So the model below is a **shared goal**, not
> a proof ARTS already achieves it. ARTS *is* heavily op-verified at the ODS level;
> its one genuinely irreducible ModuleOp verifier is `VerifyArtsCdag`'s whole-module
> physical-layout walk, which can only fold to op verifiers once `db_alloc`'s stamped
> layout becomes the type (the ARTS no-contract audit, `architecture.md` PHASE 7).

ARTS's ops carry their invariants in ODS op verifiers (the model SDE should match).
Every invariant lives in one of three homes, chosen by the **reach** the check needs:

- **(A) Op verifier** — `hasVerifier=1` (own attrs/operands/results) or
  `hasRegionVerifier=1` (region/nesting, runs after nested ops verify). A verifier
  may legally reach: its own attrs/operands/results/regions; **one hop** via
  `operand.getDefiningOp<T>()` (e.g. `EdtOp` dep-is-`DbAcquire`,
  `Dialect.cpp:201-207`); a ≤2-hop def-chain (`DbRefOp`, `:1120-1162`); a
  **self-region walk that skips nested same-kind ops** (`EdtOp` capture discipline,
  `:248-250`); the parent; and uses. **Never a forward whole-module walk.** That
  line — operands' producers + parent + uses, bounded by local fan, not
  O(module/N²) — decides op-local vs must-stay.
- **(B) Traits / ODS constraints** — declarative structure: `ParentOneOf`,
  `Terminator`, `SingleBlockImplicitTerminator`, `SizedRegion`,
  `AttrSizedOperandSegments`, child-whitelist. ARTS `YieldOp` nesting is pure ODS
  (`ArtsOps.td:749-751`), no C++.
- **(C) OpInterfaces** — typed cross-op attr-family queries so no pass exists
  merely to *locate* attr-bearing ops.

**The tautology-avoidance template (load-bearing).** `DbAllocOp::verify`
(`Dialect.cpp:581-617`) does NOT trust the stamped owner dims — it **re-derives an
independent extent** from the op's own grid and fails closed unless they agree.
SDE's `findOwnerIterationExtents` checks are the same shape: they fold the
**writer `su_iterate`** constant bounds and demand `gridCount == ceilDiv(extent,
block)`. These port into the SU/MU op verifier — they must **not** collapse into a
no-op trait asserting an attribute equals itself.

## Per-pass verdict (review-corrected)

| Pass | Verdict | Where its surviving checks go |
| --- | --- | --- |
| `verify-sde-lowered` | **keep (O(N) barrier)** | "No `sde.*` op survives" is a dialect-absence predicate with no host op. Canonical home is `ConversionTarget.addIllegalDialect`, but the boundary is hand-written `module.walk` with no conversion driver — keep the cheap O(N) pass (`Compile.cpp:1194`). |
| `verify-sde` | **reduce to 1 arm (maybe 2)** | "Source compute outside any CU in an SDE-bearing func" (`VerifySde.cpp:193-202`) is irreducible — the trigger op is *foreign* (non-SDE), so no SDE op verifier can host it. Other arms → new `SdeMuDepOp`/`SdeControlTokenOp`/`SdeSuBarrierOp` verifiers + `SdeCuWorkOp::verify`. Raw-compute-direct-SU-child arm deleted (redundant with the su_iterate/su_distribute child whitelists). |
| `verify-sde-physical-consistency` | **eliminate** | All 7 arms fold into `SdeSuIterateOp::verify` (own body StoreOps + `getDefiningOp`, O(body)); presence/(a)–(d) vanish once `physical*`/`arrayLayout` become the type. The **store→IV worker-slice** arm re-homes to the CU-grouping op verifier (transitionally stays in `su_iterate`). |
| `verify-sde-mu-layout` | **eliminate (→ op verifiers)** | R1a/R1b deleted (redundant with core `memref` load/store verifiers). R1c + **R2b** (`findOwnerIterationExtents`, **true-writer path**) → `SdeMuAllocOp::verify`. R2a/R2c are type tautologies, vanish. |
| `verify-sde-mu-access-window` | **eliminate** | R1 / spec-identity / rank-0 / realize-gate → `SdeMuAllocOp::verify`; dup-window + **R2 ceilDiv (true-writer)** → `SdeMuAccessWindowOp::verify`; partial-reduction subset relocates to `su.reduce_scatter`. Attr-presence arms vanish. |
| `verify-sde-mu-access-window-sync` | **eliminate (→ op verifier)** | Misaligned/Redundant → new `SdeSuBarrierOp::verify` (bounded sibling scan of the CU run before/after the barrier, calling `classifyBarrierSync` **verbatim**). Per-CU window coverage → `SdeCuRegionOp` region verifier. |
| `verify-sde-coarse-avoidance` | **delete** | Nothing migrates — it is the verify-twin of an *optimization* pass; a flat-but-realizable `mu_alloc` is well-formed IR, and a verifier must not assert "an optional rewrite fired." Vanishes with `sde.redist` + type-borne grid. |
| `verify-sde-redistribute` | **eliminate, conditionally** | `verifyRedistGeometry` deleted (redundant with `SdeRedistOp::verify`); `verifyRedistAnchoredInConsumer` (sibling-bounded) folds into the op verifier then the `su_distribute` child-whitelist trait. **`redistGroundedInCommittedLayout` does three whole-module walks** (`RedistributionEdges.cpp:606,610,630`) keyed by `arrayId` to find the global home writer — it only vanishes **if** the movement op's operand is provably the SSA def of the home writer (premise gated, below). |

## New op verifiers to add

Today these ops have **no** verifier: `SdeControlTokenOp`, `SdeSuBarrierOp`,
`SdeMuDepOp`. Add:

- **`SdeSuBarrierOp::verify`** (`hasVerifier`) — no `$tokens` operands; the
  Misaligned/Redundant sync verdicts via a bounded sibling scan, sharing
  `classifyBarrierSync`/`partitionBarrierPhases` with the existing
  `sde-mu-access-window-sync-opt` transform. **Do not relocate `phaseFullyWindowed`
  out of `classifyBarrierSync`** (the Redundant verdict depends on it internally).
- **`SdeControlTokenOp::verify`** — all `getToken().getUsers()` are `su_barrier`.
- **`SdeMuDepOp::verify`** — result must remain unconsumed (leaf declaration).

Augment existing verifiers: `SdeMuAllocOp` (R1c, R2b true-writer extent, window
presence/identity), `SdeMuAccessWindowOp` (dup-window, R2 ceilDiv true-writer),
`SdeCuRegionOp` (region verifier: per-CU window coverage), `SdeCuWorkOp`
(conflicting-sibling `cu_work`), `SdeSuIterateOp` (the 7 physical-consistency
arms). Add a `su_distribute` **child-whitelist trait** so movement ops appear only
under `su_distribute` (makes `verify-sde`'s no-dataflow-graph rule hold by
construction).

## The genuinely cross-op residuals (honest accounting)

1. **"Source work outside any CU in an SDE-bearing func"** — foreign trigger op +
   function-scope walk; no SDE op host. Stays as the single arm of a shrunken
   `verify-sde`.
2. **"No `sde.*` op survives lowering"** — dialect-absence; no host op. Stays as the
   O(N) `verify-sde-lowered` barrier (becomes conversion legality only if the
   boundary is ever rewritten as dialect-conversion patterns — a separate, larger
   refactor).
3. **(conditional) `redistGroundedInCommittedLayout`** — whole-module home-writer
   lookup by `arrayId`. Vanishes **only if** a Phase-E gate proves the movement
   op's `getMu()` operand `sameMemrefRoot`-equals its home writer's result with no
   intervening `arrayId`-only link. If that premise fails, it remains a residual
   `verify-sde` arm. **End state is therefore 8 → 2, or 8 → 3.**

## Two corrections the review made load-bearing

- **R2b/R2 must resolve the extent via the true-writer path**
  (`findCommittedBlockLayoutWriter`) and **fail closed** when only the
  reader-witness fallback exists. Folding the *reader's* bounds would make the
  ceilDiv check compare the grid against the same layout fact — a tautology. The
  independent-writer-extent guarantee is the whole point; preserve it.
- **`verify-sde-mu-access-window-sync` deletion is coupled.** The barrier verifier
  must call `classifyBarrierSync` verbatim; `cu_region` gets only the *standalone*
  coverage check. Relocating `phaseFullyWindowed` would either double-host or break
  the Redundant verdict (its only caller is `AccessWindowSync.cpp:158`).

## Ordered migration (convert-then-delete, gated vs the RED baseline)

- **Phase A** — pin the SDE+ARTS lit baseline (RED at HEAD); all gates =
  no-new-failures vs it.
- **Phase B — pure-redundancy deletions** (lit delta == 0, no new verifiers):
  `verifyRedistGeometry` arm; mu-layout R1a/R1b; verify-sde raw-compute-direct-SU
  arm.
- **Phase C — add the 3 new op verifiers** (`SdeMuDepOp`, `SdeControlTokenOp`,
  `SdeSuBarrierOp` incl. sync verdicts), move per-CU coverage to `SdeCuRegionOp`,
  then **delete `verify-sde-mu-access-window-sync`**; fold conflicting-sibling
  `cu_work`.
- **Phase D — fold the tautology-avoidance arms** into `SdeMuAllocOp`/
  `SdeMuAccessWindowOp` (true-writer path, fail-closed on reader fallback), then
  **delete `verify-sde-mu-layout` and `verify-sde-mu-access-window`**.
- **Phase E — transitional fold of the attribute-soup passes**, gated on the
  type/op revision (design-revision Steps 4–9): fold physical-consistency into
  `SdeSuIterateOp` then delete; delete `verify-sde-coarse-avoidance`; delete
  `verify-sde-redistribute` (after the SSA-grounding gate decides residual #3).
- **Phase F — irreducibles remain**: `verify-sde` (1 arm, maybe 2) +
  `verify-sde-lowered`; the `su_distribute` child-whitelist trait.

Gate at every step: byte-diff stability sweep on the 21 kernels + per-kernel
`Correct=YES` + lit delta == 0 vs the RED baseline.
