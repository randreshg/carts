# RFC: ARTS-Native Epoch Continuations (Finish-EDT) for CARTS

Date: 2026-03-16
Status: Draft plan from code/runtime audit
Execution board: `docs/compiler/epoch-finish-continuation-execution-board.md`

## 1. Why We Are Doing This

Today CARTS lowers epoch synchronization to a blocking wait path:

- `arts.epoch` -> `arts.create_epoch` + `arts.wait_on_epoch` in `EpochLowering`.
- `arts.wait_on_epoch` -> `arts_wait_on_handle` in `ConvertArtsToLLVM`.

This is correct, but expensive in hot paths:

- `arts_wait_on_handle` repeatedly yields/schedules while waiting.
- It releases/reacquires DB frontier locks while blocked.
- It forces an upper EDT to context-switch instead of finishing quickly.

For OpenMP patterns with many barriers/taskwait-like boundaries (including `parallel for` inside timestep loops), this can become a major overhead source.

Goal: preserve correctness and bulk synchronization semantics while replacing blocking waits with ARTS-native finish-continuation scheduling when safe.

## 2. Ground Truth From Runtime and Compiler Audit

### 2.1 Runtime already supports finish EDTs for epochs

Confirmed in ARTS runtime/public API:

- `arts_initialize_and_start_epoch(finish_edt_guid, slot)` exists and is documented as signaling the finish EDT slot when epoch completes.
- `increment_finished_epoch` and `check_epoch` signal `termination_exit_guid/slot` with `arts_signal_edt_value(...)` on completion.
- Multi-node epoch protocol carries finish state through reduction rounds and still fires finish EDT at completion.

Relevant runtime files:

- `external/arts/libs/include/public/arts.h`
- `external/arts/libs/src/core/sync/termination.c`
- `external/arts/libs/src/core/remote/handler.c`

### 2.2 Current CARTS lowering does not use finish EDT plumbing

Current behavior:

- `CreateEpochOp` has no finish target operands in IR.
- `CreateEpochPattern` hardcodes finish guid to `0` (no finish EDT).
- `EpochLowering` always inserts `arts.wait_on_epoch`.

Relevant files:

- `include/arts/Ops.td`
- `lib/arts/passes/transforms/EpochLowering.cpp`
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`

### 2.3 Dependency recording path is robust and analyzable

Confirmed path:

- `EdtLowering` computes `depCount` from DB dependency element counts.
- `RecordDepOp` lowers to slot-ordered `arts_add_dependence` / `arts_add_dependence_at`.
- Runtime readiness is `depc_needed` driven; EDT runs only when all slots satisfied.

Relevant files:

- `lib/arts/passes/transforms/EdtLowering.cpp`
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`
- `external/arts/libs/src/core/sync/event.c`
- `external/arts/libs/src/core/compute/edt.c`
- `external/arts/libs/src/core/scheduler.c`

## 3. Key Design Insight

We do not need a full `arts -> async -> async.runtime -> llvm` migration to solve the main problem.

We can get the primary win with ARTS-native continuation scheduling:

1. Create epoch with finish target = continuation EDT + control slot.
2. Spawn worker EDTs into epoch.
3. Do not block with `arts_wait_on_handle`.
4. Let runtime fire continuation when epoch completes.

This keeps ARTS execution semantics explicit, preserves distributed behavior, and avoids introducing a second async IR ecosystem as a required dependency for core correctness/performance.

## 4. Important Semantic Constraint: Continuation Capture

The finish EDT/continuation must capture everything needed to run the tail correctly:

- scalar/live values (e.g., loop counters, flags, addresses)
- DB dependencies needed by tail code
- route/concurrency metadata
- contract/partition attributes required by downstream lowering

This is critical for correctness. A continuation that only carries epoch completion without proper data capture can execute with stale or missing state.

## 5. Important Semantic Constraint: Outer Loop + Parallel-For

This is the major hazard:

```c
for (t = 0; t < T; ++t) {
  #pragma omp parallel for
  for (...) body;
  post(t);
}
```

Current model (wait) enforces per-iteration order.

If we naively remove wait and keep outer loop in same EDT, we may start `t+1` before `post(t)` or before epoch `t` finishes.

So we need one of two strategies:

- Conservative phase: keep wait fallback for epochs inside loop-carried control.
- Full CPS phase: transform outer loop into continuation chain.

CPS sketch:

- `iter_upper(t)`: create epoch, spawn workers, attach finish -> `iter_lower(t)`.
- `iter_lower(t)`: run `post(t)`; if `t+1 < T`, create `iter_upper(t+1)` else continue parent.

This preserves sequential iteration semantics without blocking worker threads.

## 6. Proposed Compiler Plan (Phased)

### Phase 0: Safety and Pre-Reqs

1. Keep current wait path as default behavior.
2. Add a flag (example: `--arts-epoch-finish-continuation`) for opt-in.
3. Fix distributed metadata continuity gap before broad distributed enablement:
   - ensure `distribution_kind` and other required attrs survive DB rewrites.

### Phase 1: IR Plumbing for Finish Targets

1. Extend `CreateEpochOp` with optional finish target operands:
   - `finishEdtGuid : i64` (optional)
   - `finishSlot : i32` (optional)
2. Update assembly/verification for backwards compatibility.
3. Update `CreateEpochPattern`:
   - if finish operands are present, call runtime with them.
   - else fallback to `(0, DEFAULT_EDT_SLOT)`.

Files:

- `include/arts/Ops.td`
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`

### Phase 2: Continuation Preparation Pass (ARTS-level)

Add new pass (name suggestion: `EpochContinuationPrepPass`) before `EdtLowering`.

Responsibilities:

1. Detect safe pattern boundaries (`epoch + wait + tail`) in same block.
2. Outline/move tail into a continuation `arts.edt` region.
3. Reacquire/map required DB dependencies for tail.
4. Tag continuation with control dependency requirement (1 slot for epoch completion).
5. Record binding from epoch to continuation target+slot (attribute or side-table op).

Why before `EdtLowering`:

- Existing EDT outlining/capture machinery can carry non-DB values safely.
- We avoid custom low-level cloning logic.

### Phase 3: Control-Slot Integration in EDT Lowering

Current `depCount` only counts DB slots. We must include control slots for continuations.

Plan:

1. Add continuation control-dep metadata to `EdtCreateOp` (or derive from op attrs).
2. Compute `depc = dbDepCount + controlDepCount`.
3. Keep `RecordDepOp` writing DB slots `[0 .. dbDepCount-1]`.
4. Reserve control slot(s) at the tail (for epoch finish signaling).

This gives runtime a real dependency slot to satisfy with `arts_signal_edt_value`.

Files:

- `lib/arts/passes/transforms/EdtLowering.cpp`
- `include/arts/Ops.td`
- optionally `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`

### Phase 4: Epoch Lowering Uses Finish Target Instead of Wait

Update `EpochLowering`:

1. If epoch has continuation binding and safety checks pass:
   - emit `arts.create_epoch` with finish target.
   - do not emit `arts.wait_on_epoch`.
2. If unsafe or unsupported:
   - emit current `create + wait` path.

Files:

- `lib/arts/passes/transforms/EpochLowering.cpp`

### Phase 5: Loop-Aware Enablement

Start conservative:

- Disable continuation replacement when epoch is inside control-flow/loop-carried regions that would alter iteration ordering.

Then add advanced loop CPS pass (optional second milestone):

- transform outer loop with internal epoch waits into explicit upper/lower iteration EDT chain.
- preserve ordered dependence between iterations by construction.

## 7. Safety Rules (Must-Have)

Continuation path enabled only when all are true:

1. Tail side effects are movable to continuation EDT without reordering hazards.
2. Required DB dependencies can be traced/reacquired from valid roots.
3. Partition and contract attrs are preserved on reacquired dependencies.
4. Route semantics are preserved (do not force route 0 accidentally in internode cases).
5. No unsupported loop-carried ordering hazard unless CPS loop transform is active.

Else fallback to current wait path.

## 8. Distributed and Multi-Node Notes

- Runtime epoch completion protocol is already distributed-aware.
- Finish EDT signal can target remote EDT via normal signal forwarding.
- Compiler must preserve distributed ownership and routing attrs through rewrites.
- For internode continuations, route mapping must follow existing worker->node semantics, not hardcoded defaults.

## 9. Async Dialect: Use Concepts, Not Mandatory IR Migration

Recommendation:

- Borrow ideas from MLIR Async passes for analysis/design patterns:
  - recursive work decomposition for parallel loops
  - continuation-style split
  - group/wait elimination concepts
- Do not make Async dialect lowering a required path for this optimization phase.

Optional later experiment (flag-gated):

- restricted intranode `arts -> async` island for benchmarking/compare-only.

## 10. Example Transformations

### 10.1 Simple Parallel-For Barrier

Before (current):

- create epoch
- spawn tasks with epoch
- wait on epoch
- tail work

After (target):

- create continuation EDT (`depc = dbSlots + 1`)
- create epoch with finish = continuation guid, slot = `dbSlots`
- spawn tasks with epoch
- upper returns
- runtime signals continuation slot on epoch completion
- continuation runs tail work

### 10.2 Outer Loop Case (Jacobi-style)

Before:

- each outer iteration does `parallel_for + wait + post`

Safe initial policy:

- keep wait fallback inside outer loop (no semantic risk)

Advanced CPS policy:

- each iteration split into upper/lower EDT chain to preserve iteration order while avoiding blocking waits.

## 11. Testing Plan

### Compile-time / IR tests

1. `CreateEpochOp` with and without finish operands.
2. `EpochLowering` emits no wait in safe continuation cases.
3. Fallback cases still emit wait.
4. Continuation `depc` includes control slots.
5. Slot mapping correctness: DB deps then control slot.

### Runtime behavior tests

1. Single-node finish EDT fire ordering.
2. Multi-node finish EDT fire ordering.
3. No deadlock when continuation has DB deps + finish control dep.

### App-level regression tests

1. `parallel_for` microbenchs.
2. Jacobi/timestep-style outer-loop examples.
3. Distributed DB ownership cases.

## 12. Recommended Execution Order

1. Land Phase 1 (IR/runtime plumbing + no behavior change).
2. Land Phase 3 control-slot support behind flag.
3. Land Phase 2/4 continuation rewrite for non-loop-safe cases only.
4. Expand eligibility gradually with metrics.
5. Implement CPS outer-loop transform only after baseline continuation path is stable.

## 13. Bottom Line

Best current direction is ARTS-native finish-continuation lowering with strict safety/fallbacks, not full Async IR migration.

This directly targets the main overhead (`arts_wait_on_handle` blocking/yield path), keeps distributed semantics in ARTS, and gives us a controlled path to loop-aware continuation scheduling.

## 14. Coverage Checklist (Make Sure We Cover Everything)

This section is the explicit “did we cover all the things?” checklist for implementation and review.

### 14.1 OpenMP semantic coverage

- `omp barrier` / `omp taskwait` equivalent bulk synchronization stays correct.
- `omp parallel for` implicit barrier behavior remains preserved.
- `nowait` behavior is preserved (no accidental extra synchronization).
- Ordered program points after a parallel region still observe completed work.

### 14.2 ARTS program execution model coverage

- Epoch scope correctness (`active/finished` accounting) preserved.
- Finish-EDT signaling used as synchronization completion source.
- No hidden imperative synchronization that breaks DAG analyzability.
- Continuation scheduling is explicit EDT creation + dep satisfaction.

### 14.3 Continuation capture coverage

- Scalar/live-out values needed by tail are captured or rematerialized.
- DB dependencies used by tail are reacquired from valid roots.
- Partition/contract metadata is preserved on reacquired DB views.
- Route/concurrency/topology attrs are preserved on continuation EDT.
- Continuation dep slots include both DB slots and control slot(s).

### 14.4 Dependency recording and EDT readiness coverage

- Slot ordering remains deterministic and stable across lowering.
- DB dependencies still recorded via `arts_add_dependence` / `_at`.
- Control slot for epoch completion maps to a real `depc` slot.
- `depc_needed` reaches zero only when DB + control deps are satisfied.
- Null/bounds-guarded deps still consume slot positions correctly.

### 14.5 DB semantics coverage

- Acquire/release mode semantics preserved (RO/EW/MEMSET/etc.).
- ESD/byte-slice dependency semantics preserved.
- No illegal movement across DB side effects.
- No dependence on wait-path-only DB frontier release/reacquire behavior.
- Tail continuation reacquires any DB state it needs explicitly.

### 14.6 Distributed / multinode coverage

- Distributed ownership markers survive rewrites.
- Distribution-kind/partition metadata is not dropped.
- Internode route mapping remains valid (no hardcoded route collapse).
- Finish EDT signal path works when source/target are on different nodes.
- Fallback exists when distributed eligibility is unclear/unsafe.

### 14.7 Loop-carried ordering coverage

- Epoch replacement inside outer loops does not violate iteration order.
- Conservative fallback for loop-carried hazards is implemented first.
- Optional CPS loop transform is required before enabling those cases.
- Jacobi/timestep patterns are explicitly tested.

### 14.8 Performance coverage

- Blocking wait frequency reduced in eligible cases.
- Context-switch/yield overhead reduced in hot synchronization paths.
- No regression from excessive continuation EDT proliferation.
- Metrics are added to compare wait-path vs continuation-path behavior.

## 15. Runtime Capability Audit Summary

Runtime has the primitives required for the primary plan:

- Epoch creation with finish target (`arts_initialize_and_start_epoch`).
- EDT creation in an epoch (`arts_edt_create_with_epoch[_dep]`).
- Slot-based dep satisfaction and readiness (`depc_needed` model).
- Distributed epoch completion protocol and remote signaling.

So the main work is compiler IR/plumbing/analysis, not a missing-runtime blocker.

## 16. Phase Exit Criteria (Definition of Done)

### Phase 1 done when

- `CreateEpochOp` finish operands exist and lower correctly.
- Default behavior remains unchanged when finish operands are absent.
- Existing tests continue to pass.

### Phase 2/3 done when

- Continuation EDT generation is enabled behind flag.
- Continuation `depc` includes control slot(s).
- Control slot is satisfied by epoch finish signal.
- Fallback-to-wait triggers for unsupported patterns.

### Phase 4 done when

- `EpochLowering` conditionally emits finish-target path and omits wait.
- Wait path remains available and is selected conservatively when needed.

### Phase 5 done when

- Loop-hazard cases are either safely CPS-transformed or explicitly rejected.
- Outer-loop + inner-parallel correctness tests pass across node counts.

## 17. Open Questions To Resolve Before Broad Enablement

- Should control slots be represented as explicit IR operands or encoded via attrs on continuation EDT/create ops?
- Do we want a dedicated continuation route policy for internode (owner-local vs worker-mapped) as a first-class helper?
- Should we gate distributed continuation support initially to intranode-only and then expand?
- How should performance telemetry be surfaced (pass stats, runtime counters, or both)?
