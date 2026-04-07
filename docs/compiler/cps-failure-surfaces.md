# CPS Failure Surfaces

Reference document for the CARTS CPS (Continuation-Passing Style) async loop
system. Covers all CPS attributes, the pack-schema reconstruction flow, known
failure modes, and how RFC Phases 2-3 will replace each recovery path.

See also: [Structured Kernel Plan RFC](structured-kernel-plan-rfc.md)

## 1. CPS Attributes Reference

All CPS attributes are defined in
`include/arts/utils/OperationAttributes.h` (lines 60-96) under
`AttrNames::Operation`.

### 1.1 Chain Identity

| Constant | IR Name | Type | Semantics |
|----------|---------|------|-----------|
| `CPSChainId` | `arts.cps_chain_id` | `StringAttr` | Unique ID linking an `EdtOp` to a CPS continuation chain. Set on kickoff and continuation EDTs. Used by `EpochLowering` to match `CPSAdvanceOp` placeholders to their target `EdtCreateOp`. |
| `CPSLoopContinuation` | `arts.cps_loop_continuation` | `UnitAttr` | Marks an EDT as a CPS loop continuation (as opposed to a kickoff). Used to distinguish continuations during dep routing and capture replacement. |

### 1.2 Outer Epoch and Iteration State

| Constant | IR Name | Type | Semantics |
|----------|---------|------|-----------|
| `CPSOuterEpoch` | `arts.cps_outer_epoch` | `UnitAttr` | Marks the outer epoch of a CPS chain (the one `main` waits on). Set during `EpochOpt` CPS driver/chain transform. |
| `CPSInitIter` | `arts.cps_init_iter` | `IntegerAttr(i64)` | Initial iteration value for the CPS loop. Attached to the outer epoch so `EpochLowering` can inject the starting counter. |
| `CPSIterCounterParamIdx` | `arts.cps_iter_counter_param_idx` | `IntegerAttr(i64)` | Index into the param pack where the iteration counter lives. Set on `EdtCreateOp` by `EpochOpt`, read by `EpochLowering` to inject and extract the counter. |
| `CPSOuterEpochParamIdx` | `arts.cps_outer_epoch_param_idx` | `IntegerAttr(i64)` | Index into the param pack where the outer epoch GUID lives. Enables the continuation to signal epoch completion on the last iteration. |

### 1.3 Parameter Permutation and Carry ABI

| Constant | IR Name | Type | Semantics |
|----------|---------|------|-----------|
| `CPSParamPerm` | `arts.cps_param_perm` | `DenseI64ArrayAttr` | Maps positional slots in a continuation's param pack to canonical carry indices. Produced by `EpochLowering` during CPS propagation (step after `materializeLoopParams`). Consumed by `rebuildCpsPackToTargetSchema` to reconstruct relaunch state. |
| `CPSNumCarry` | `arts.cps_num_carry` | `IntegerAttr(i64)` | Number of carry parameters in the `CPSAdvanceOp` next-params list (excludes the additive step/ub prefix). Set by `EpochOpt` during CPS chain/driver emission. |
| `CPSAdditiveParams` | `arts.cps_additive_params` | `UnitAttr` | When set, the first two `CPSAdvanceOp` next-params are `[step, ub]` and carry params start at index 2. Without this, carry params start at index 0. |
| `CPSPreserveCarryAbi` | `arts.cps_preserve_carry_abi` | `IntegerAttr(i64)` | Preserves the full carry-slot prefix for direct-recreate continuations even when some slots are locally dead. The value indicates the minimum carry width to preserve so the relaunch path reproduces the kickoff ABI exactly. |

### 1.4 Advance Control

| Constant | IR Name | Type | Semantics |
|----------|---------|------|-----------|
| `CPSDirectRecreate` | `arts.cps_direct_recreate` | `UnitAttr` | Marks a `CPSAdvanceOp` as using direct self-recreation (the advance EDT recreates the kickoff EDT directly, rather than going through an intermediate). Set on driver-mode advances. |
| `CPSAdvanceHasIvArg` | `arts.cps_advance_has_iv_arg` | `UnitAttr` | Indicates the `CPSAdvanceOp`'s epoch body has an induction-variable block argument. `EpochLowering` replaces this block arg with the actual dynamic iteration counter. |

### 1.5 Dependency Routing

| Constant | IR Name | Type | Semantics |
|----------|---------|------|-----------|
| `CPSForwardDeps` | `arts.cps_forward_deps` | `UnitAttr` | Marks kickoff/relaunch EDTs whose explicit dependency slots form part of the loop-carried state ABI and must be forwarded on relaunch. |
| `CPSDepRouting` | `arts.cps_dep_routing` | `DenseI64ArrayAttr` | Layout: `[numTimingDbs, hasScratch]`. Tells `EpochLowering` how many dep GUIDs are in `loopBackParams` and their layout: last `(numTimingDbs + hasScratch)` carry params are dep GUIDs. Timing DBs occupy dep slots `0..T-1`; scratch occupies slot `T`. |

### 1.6 Non-CPS Epoch Attributes

| Constant | IR Name | Type | Semantics |
|----------|---------|------|-----------|
| `NoStartEpoch` | `arts.no_start_epoch` | `UnitAttr` | Epoch without caller-active count (CPS continuation-path inner epochs). Prevents `arts_epoch_start()` emission. |
| `ContinuationForEpoch` | `arts.continuation_for_epoch` | `UnitAttr` | Marks epochs cloned into continuation EDT bodies. Tells `EpochLowering` these epochs are controlled by the CPS chain, not standalone. |

### 1.7 Supporting Attributes (CPS-Adjacent)

| Constant | IR Name | Type | Semantics |
|----------|---------|------|-----------|
| `DbStaticOuterShape` | `arts.db_static_outer_shape` | `ArrayAttr(I64)` | Preserves compile-time DB outer extents when outlining breaks the `DbAllocOp` def-use chain. Used during CPS carry reconstruction to recover element sizes. |
| `DbRootAllocId` | `arts.db_root_alloc_id` | `IntegerAttr(i64)` | Preserves the original `DbAllocOp` `arts.id` on rebuilt handle scaffolding so downstream lowering can recover provenance without transporting raw handle pointers. |

## 2. Pack-Schema Reconstruction Flow

The CPS relaunch system reconstructs continuation state through a multi-step
pipeline spanning `EpochOpt` and `EpochLowering`.

### 2.1 EpochOpt: CPS Chain/Driver Construction

Source: `lib/arts/passes/opt/edt/EpochOpt.cpp`

**CPS Driver** (`tryCPSLoopTransform`, line 840): Single-epoch loops. Creates
a wrapping epoch, kickoff EDT, and advance EDT with `CPSDirectRecreate`.

**CPS Chain** (`tryCPSChainTransform`, line 1528): Multi-epoch loops. Creates
N continuation EDTs nested inside an outer epoch, with the last continuation
emitting a `CPSAdvanceOp` to restart the chain.

Key steps in CPS Chain:
1. **CPS-1**: Classify `loopBackParams` into scalar state vs DB handles.
2. **CPS-2**: Create scratch DB for data GUIDs (before outer epoch).
3. **CPS-3**: Populate scratch with GUIDs.
4. **CPS-4/5/6**: Wire deps and replace body captures after scratch creation.
5. **CPS-7**: Prepend dep GUIDs to the scalar carry state.
6. **CPS-8**: Re-analyze chain_0's final captures to rebuild carry ordering.
   If the live capture set changed, rewrite all `CPSAdvanceOp`s with reordered
   carry params.
7. **CPS-9**: Free scratch DB after the outer epoch.

### 2.2 EpochLowering: CPS Propagation and Advance Resolution

Source: `lib/arts/passes/transforms/EpochLowering.cpp`

**Phase 1: Outer epoch injection** (line 277-348): For `CPSOuterEpoch` epochs,
injects iteration counter and outer GUID parameters into all chain EDTs.

**Phase 2: CPS propagation** (`materializeLoopParams`, line 348-641): Computes
`arts.cps_param_perm` by comparing each EDT's param pack against the
canonical carry order. Propagates CPS indices from parent to child functions.
Must run BEFORE advance resolution.

**Phase 3: Advance resolution** (line 643-1941): Walks all `CPSAdvanceOp`
placeholders and resolves each one:

1. Find the target `EdtCreateOp` with matching `cps_chain_id`.
2. Read the target's `outlined_func` to determine the continuation function.
3. Build `canonicalLoopBackParams` from `CPSNumCarry` carry params.
4. Call `rebuildCpsPackToTargetSchema` to reconstruct the target's param pack.
5. Emit the continuation `EdtCreateOp` with the reconstructed pack.
6. Wire dependency routing via `CPSDepRouting` attribute.
7. Handle `CPSForwardDeps` slot forwarding.

### 2.3 `rebuildCpsPackToTargetSchema` (line 890)

This is the core pack-reconstruction function. It operates in two modes:

**Self-recreation mode** (same function, `outlinedFunc == parentFunc`):
- Directly maps `localUnpack` results to target slots.
- Skips slots that trace back to DB allocs (unsafe for CPS carry).
- Injects `tNext` at `CPSIterCounterParamIdx` and `outerEpochGuid` at
  `CPSOuterEpochParamIdx`.

**Cross-function mode** (continuation targets a different function):
- Resolves canonical indices through `localParamPerm` and `targetParamPerm`.
- Builds `canonicalValues[]` from loop-back params mapped through local perm.
- Falls back through multiple resolution strategies:
  1. Original pack operand matching (target EdtParamPackOp).
  2. Canonical perm-mapped values.
  3. Direct positional matching (when no perm exists).
  4. Local carry fallback (from `localUnpack` or `localParamArrayMemref`).
  5. Preserved incoming ABI slots (`CPSPreserveCarryAbi`).
- Fills unresolved slots with zero and emits a warning with slot count.

## 3. Known Failure Modes

### 3.1 Slot Mismatch

**Symptom**: `ARTS_WARN("CPS advance: rebuilt continuation pack ... with N
schema hole(s)")` at `EpochLowering.cpp:1194`.

**Root cause**: The canonical carry params and the target function's param
pack have different widths or orderings. This happens when:
- EDT fusion changes the number of captures between CPS chain creation
  (EpochOpt) and advance resolution (EpochLowering).
- An intermediate pass adds or removes DB acquires, changing the capture set.
- `CPSParamPerm` becomes stale after structural rewrites.

**Current mitigation**: Zero-fill missing slots. The zero value is often
harmless (unused parameter) but can cause silent miscomputation if the slot
carries live state.

**Risk level**: Medium. Zero-filled live slots produce wrong results without
crashing.

### 3.2 Dep Routing Miscounts

**Symptom**: `CPSDepRouting` `[numTimingDbs, hasScratch]` does not match the
actual number of dep GUIDs prepended to the carry state.

**Root cause**: CPS-8 (carry re-analysis in EpochOpt) may change the carry
order after `CPSDepRouting` was set. The attribute is updated on
`CPSAdvanceOp`s (line 2369), but if a continuation EDT's captures change
between CPS-7 and CPS-8, the routing layout can become inconsistent.

**Current mitigation**: EpochOpt logs a warning when carry re-analysis changes
arity (`EpochOpt.cpp:2392`). EpochLowering reads the routing attribute and
emits dep wiring accordingly, but does not cross-validate against the actual
carry param count.

**Risk level**: High. Incorrect dep routing causes deadlocks (missing deps) or
data races (wrong dep mode).

### 3.3 Carry ABI Violations

**Symptom**: Continuation EDT receives corrupted or stale values in carry
slots.

**Root cause**: `CPSPreserveCarryAbi` forces the relaunch path to preserve
carry slots that may be locally dead in the continuation but are needed by the
kickoff ABI. When the preservation width is wrong (too small or too large),
the relaunch pack either drops live state or includes garbage.

**Scenarios**:
- Direct-recreate continuations compact away kickoff-only captures locally
  but still need the original kickoff ABI on relaunch.
- `EdtLowering::packParams` ordering changes between passes without updating
  `CPSPreserveCarryAbi`.

**Current mitigation**: `rebuildCpsPackToTargetSchema` has a dedicated path for
`preserveIncomingCarryAbi` (line 1112-1128) that reads from the raw param array
memref. If the structural slot does not match the target type, it falls through
to the next resolution strategy.

**Risk level**: Medium. Usually caught by type mismatch checks, but silent
corruption is possible when types happen to match but values differ.

### 3.4 Forward Dep Alignment

**Symptom**: Forwarded dep slots do not line up between kickoff and
continuation EDTs.

**Root cause**: `CPSForwardDeps` is a `UnitAttr` flag that marks EDTs whose dep
slots must be forwarded on relaunch. The actual dep slot indices come from the
EDT's capture list and the dep routing attribute. When continuation EDTs have
different dep slot counts than the kickoff (due to capture differences), the
forwarding emits `arts_add_dependence` calls with wrong slot indices.

**Scenarios**:
- Multi-epoch CPS chains where different continuations capture different DB
  sets.
- Scratch DB routing where `hasScratch=1` in `CPSDepRouting` but the scratch
  DB is only used by some continuations.

**Current mitigation**: `EpochLowering` emits forwarded deps from the parent
function's `depv` argument using `DepGepOp` (line 1240-1249). Slot alignment
is assumed to match the kickoff's dep layout.

**Risk level**: Medium-High. Misaligned deps cause runtime crashes (null dep
GUID) or deadlocks (missing signal).

### 3.5 Cross-Function Perm Mismatch

**Symptom**: Values appear in wrong carry slots after CPS chain relaunch.

**Root cause**: `localParamPerm` and `targetParamPerm` are computed
independently during CPS propagation. If the local and target functions have
different capture orders (e.g., due to EDT fusion or inlining between chain
creation and propagation), the perm-to-canonical mapping produces incorrect
slot assignments.

**Current mitigation**: The `resolveCanonicalIndex` lambda (line 971) maps
through perms. When both local and target perms exist, the resolution goes
through canonical space. When only one exists, identity mapping is assumed for
the missing perm.

**Risk level**: Medium. Usually masked by the multiple fallback strategies in
`rebuildCpsPackToTargetSchema`.

## 4. Phase 2-3 Replacement Plan

RFC Phases 2-3 (see `docs/compiler/structured-kernel-plan-rfc.md` sections 5.2,
7.2, 7.3) will replace each CPS recovery path with explicit IR contracts.

### 4.1 Phase 2: Split Launch State IR

**New IR concepts**:
- `arts.state_pack` / `arts.state_unpack`: First-class state channel carrying
  loop counters, bounds, outer epoch GUID, scalar captures.
- `arts.dep.bind` / `arts.dep.forward`: First-class dep channel carrying DB
  families, forwarded dep slots, slice offsets/sizes, partition metadata.
- `arts.edt_launch`: Unified launch op consuming both channels.

**What it replaces**:
- `CPSParamPerm` positional permutation recovery.
- `rebuildCpsPackToTargetSchema` pack archaeology.
- `materializeLoopBackParam` memref-to-i64 conversion.
- Zero-fill slot mismatch recovery (Section 3.1).

**Migration path**: `EdtCreateOp` construction will go through the split
contract. Existing `EdtParamPackOp` becomes a lowering detail, not a semantic
contract.

### 4.2 Phase 3: CPS Schema Cleanup

**Attributes to shrink or remove**:

| Attribute | Replacement |
|-----------|-------------|
| `arts.cps_param_perm` | Eliminated. State/dep channels have explicit schemas; no positional recovery needed. |
| `arts.cps_dep_routing` | Replaced by `arts.dep.bind` / `arts.dep.forward` ops. Dep layout is explicit in IR, not encoded as a metadata attribute. |
| `arts.cps_forward_deps` | Replaced by `arts.dep.forward` ops. Each forwarded dep is a named, typed IR value. |
| `arts.cps_preserve_carry_abi` | Eliminated. The state schema is always explicit; there is no ABI to preserve across compacted carry slots. |

**Attributes to keep** (possibly simplified):

| Attribute | Status |
|-----------|--------|
| `arts.cps_chain_id` | Kept. Chain identity remains useful for structural matching. |
| `arts.cps_outer_epoch` | Kept. Outer epoch marking is orthogonal to launch state. |
| `arts.cps_init_iter` | Kept. Initial iteration value moves into the `arts.state_pack` schema. |
| `arts.cps_num_carry` | Simplified. Carry count becomes implicit in the state schema width. |
| `arts.cps_direct_recreate` | Kept. Self-recreation is a valid advance strategy independent of state encoding. |

### 4.3 Failure Surface Elimination

| Failure Mode | Phase 2-3 Resolution |
|-------------|---------------------|
| Slot mismatch (3.1) | State schema is explicit and verified. Width mismatches are caught by the IR verifier, not recovered at lowering time. |
| Dep routing miscounts (3.2) | `arts.dep.bind` ops carry typed dep references. Routing is IR-visible and verifiable. |
| Carry ABI violations (3.3) | No ABI to violate. State/dep channels are the ABI. |
| Forward dep alignment (3.4) | `arts.dep.forward` ops name specific dep slots. Alignment is structural, not positional. |
| Cross-function perm mismatch (3.5) | No permutations. State schema travels with the launch op. |
