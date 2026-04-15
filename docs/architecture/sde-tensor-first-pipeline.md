# SDE Tensor-First Pipeline Design

**Date**: 2026-04-14
**Status**: DRAFT
**Branch**: `architecture/sde-restructuring`
**Trigger**: `parallel_for/single` miscompile (sum=0 instead of 1000)

---

## 1. Problem Statement

### 1.1 The Bug

`samples/parallel_for/single/parallel_for.c` combines `omp single` and `omp for`:

```c
#pragma omp parallel {
  #pragma omp single { sum += 1000; }   // write lost
  #pragma omp for    { data[i] *= 2; }  // works
}
```

**Result**: `sum=0` (wrong), `data[99]=198` (correct).

### 1.2 Root Cause Chain

| Stage | What happens | Correct? |
|-------|-------------|----------|
| ConvertOpenMPToSde | `omp.single` -> `arts_sde.cu_region <single>`, `sum` stays as `memref.alloca` | OK but fragile |
| RaiseToLinalg | Only walks `su_iterate` bodies. `cu_region <single>` ignored entirely. | Gap |
| RaiseToTensor | Only rewrites `linalg.generic` carriers. No carriers in `<single>`. | Gap |
| RaiseMemrefToTensor | Only walks `cu_task` bodies. `cu_region` is not `cu_task`. | Gap |
| ConvertSdeToArts | `cu_region <single>` -> `arts.edt <single>`. `sum` still raw `memref.alloca`. | Propagated |
| EdtStructuralOpt | **BUG**: Sinks `sum` alloca into EDT body, creating disconnected local copy. | Broken |
| CreateDbs | Local alloca -> private DB, freed at EDT end. Value lost. | Propagated |

**The SDE pipeline has no pass that raises data inside `cu_region` to tensor form.**
The scalar `sum` is never modeled as a data dependency -- it's an invisible memref
side-effect that falls through the cracks into core ARTS, where EdtStructuralOpt
incorrectly treats it as sinkable.

### 1.3 The Three "Raise" Passes -- Current Inconsistencies

| Pass | Target scope | Input | Output | Gap |
|------|-------------|-------|--------|-----|
| RaiseToLinalg (#6) | `su_iterate` bodies only | memref load/store | memref-backed `linalg.generic` carrier | Ignores `cu_region`, `cu_task` |
| RaiseToTensor (#7) | `linalg.generic` only | memref-backed linalg carrier | tensor-backed linalg carrier | Only rewrites what #6 created |
| RaiseMemrefToTensor (#12) | `cu_task` bodies only | rank-1 alloca K<=16 | `mu_data`/`mu_token`/`cu_codelet` SSA graph | Ignores `cu_region`, `su_iterate` |

**Problems**:
1. **Naming confusion**: "RaiseToTensor" vs "RaiseMemrefToTensor" sound identical but do different things.
2. **Scope fragmentation**: Each pass targets a narrow slice of SDE ops. No pass covers all compute units.
3. **Wrong ordering**: ScopeSelection/ScheduleRefinement/ChunkOpt/ReductionStrategy run BEFORE
   any tensor raising, making analysis decisions on raw memref ops.
4. **`cu_region` blind spot**: The most basic SDE compute unit has ZERO tensor coverage.

---

## 2. Design: Tensor-First SDE Pipeline

### 2.1 Core Principle

**All SDE-level computation should operate on tensor values with explicit SSA data flow.**
No memref side-effects should survive inside SDE compute units. The memref-to-tensor
boundary is the FIRST transformation after ConvertOpenMPToSde, not a late afterthought.

### 2.2 New Pipeline Order

```
ConvertOpenMPToSde
  |
  v
RaiseToTensor          (first pass, raises memrefs to tensor SSA)
  |
  v
RaiseToLinalg          (pattern-matches tensor ops into linalg.generic)
  |
  v
LoopInterchange
TensorOpt
StructuredSummaries
ElementwiseFusion
ScopeSelection         (now sees post-optimization tensor-level IR)
ScheduleRefinement     (now sees post-optimization tensor-level IR)
ChunkOpt               (now sees post-optimization tensor-level IR)
ReductionStrategy      (now sees post-optimization tensor-level IR)
DistributionPlanning
IterationSpaceDecomposition
BarrierElimination
LowerToMemref
ConvertToCodelet
ConvertSdeToArts
VerifySdeLowered
DCE
CSE
VerifyEdtCreated
```

**Key changes**:
- `RaiseToTensor` moves from position #7 to position #1 (right after OMP conversion)
- `RaiseToLinalg` moves from position #6 to position #2
- Old `RaiseToTensor` (carrier converter) is subsumed -- no longer needed
- `RaiseMemrefToTensor` is subsumed -- no longer needed
- Analysis passes (ScopeSelection etc.) now see tensor-level IR

### 2.3 New RaiseToTensor -- Scope and Algorithm

**Scope**: Every `memref.alloca` whose uses include ops inside any SDE region
(`cu_region`, `cu_task`, `su_iterate`, `su_distribute`).

**Algorithm**:

```
1. COLLECT: Walk the function, find memref.alloca ops used inside SDE regions.
   For each alloca, record:
   - All SDE compute units that read/write it
   - All uses outside SDE regions (boundary readers)
   - Element type and shape

2. ANCHOR: For each collected alloca, create an sde.mu_data handle at the
   appropriate scope level (before the outermost SDE region containing uses).
   Initialize from the alloca's initial value if present.

   %sum_data = sde.mu_data {shared} init(%c0_i32) : tensor<i32>

3. REWRITE COMPUTE UNITS: Walk SDE regions in program order. For each compute
   unit that accesses a collected memref:

   a) Create sde.mu_token for each accessed memref:
      %tok = sde.mu_token <readwrite> %current_sum : tensor<i32>

   b) Create sde.cu_codelet with tokens as operands:
      %new_sum = sde.cu_codelet(%tok) {
        ^bb0(%sum_arg: tensor<i32>):
          %val = tensor.extract %sum_arg[] : tensor<i32>
          %added = arith.addi %val, %c1000 : i32
          %result = tensor.insert %added into %sum_arg[] : tensor<i32>
          sde.yield %result : tensor<i32>
      }

   c) Advance current tensor SSA value:
      current_tensor[sum] = %new_sum

4. REWRITE su_iterate: For iterate loops, the codelet body contains
   tensor.extract/tensor.insert indexed by the induction variable:

   %new_data = sde.cu_codelet(%data_tok) {
     ^bb0(%data_arg: tensor<100xi32>):
       // inner loop over chunk:
       scf.for %iv = %lo to %hi step %c1
         iter_args(%acc = %data_arg) -> tensor<100xi32> {
         %val = tensor.extract %acc[%iv] : tensor<100xi32>
         %doubled = arith.muli %val, %c2 : i32
         %next = tensor.insert %doubled into %acc[%iv] : tensor<100xi32>
         scf.yield %next : tensor<100xi32>
       }
       sde.yield %loop_result : tensor<100xi32>
   }

5. BOUNDARY MATERIALIZE: At the end of the SDE scope, for each tensor with
   readers outside the scope, emit:

   sde.su_barrier   // ensure all tasks complete
   // extract final values back to original alloca:
   %final_val = tensor.extract %current_tensor[sum][] : tensor<i32>
   memref.store %final_val, %original_alloca[] : memref<i32>
```

### 2.4 Revised RaiseToLinalg -- Tensor Input

**Input**: tensor.extract/tensor.insert patterns inside `cu_codelet` bodies or
`su_iterate` bodies (already in tensor form from step 2.3).

**Output**: `linalg.generic` ops that replace the scalar tensor.extract/insert loop bodies.

```
// Before RaiseToLinalg:
sde.cu_codelet(%data_tok) {
  ^bb0(%data: tensor<100xi32>):
    %result = scf.for %iv = 0 to 100 step 1
      iter_args(%acc = %data) -> tensor<100xi32> {
      %val = tensor.extract %acc[%iv] : tensor<100xi32>
      %doubled = arith.muli %val, %c2 : i32
      %next = tensor.insert %doubled into %acc[%iv] : tensor<100xi32>
      scf.yield %next : tensor<100xi32>
    }
    sde.yield %result : tensor<100xi32>
}

// After RaiseToLinalg:
sde.cu_codelet(%data_tok) {
  ^bb0(%data: tensor<100xi32>):
    %result = linalg.generic {
      indexing_maps = [affine_map<(d0) -> (d0)>, affine_map<(d0) -> (d0)>],
      iterator_types = ["parallel"]
    } ins(%data : tensor<100xi32>) outs(%data : tensor<100xi32>) {
      ^bb0(%in: i32, %out: i32):
        %doubled = arith.muli %in, %c2 : i32
        linalg.yield %doubled : i32
    } -> tensor<100xi32>
    sde.yield %result : tensor<100xi32>
}
```

Since the input is already tensor-backed, there is no need for a separate
"carrier conversion" step. The old `RaiseToTensor` pass (memref carrier -> tensor
carrier) is eliminated entirely.

### 2.5 The `parallel_for/single` Example Under New Pipeline

**After ConvertOpenMPToSde** (same as today):
```mlir
arts_sde.cu_region <parallel> {
  arts_sde.cu_region <single> scope(<local>) {
    %18 = memref.load %alloca_0[] : memref<i32>      // sum
    %19 = arith.addi %18, %c1000_i32 : i32
    memref.store %19, %alloca_0[] : memref<i32>
  }
  arts_sde.su_iterate(%c0) to(%c100) step(%c1) {
    ^bb0(%iv: index):
      %val = memref.load %alloca[%iv] : memref<100xi32>
      %doubled = arith.muli %val, %c2_i32 : i32
      memref.store %doubled, %alloca[%iv] : memref<100xi32>
  }
}
```

**After NEW RaiseToTensor**:
```mlir
%sum_data = sde.mu_data {shared} : tensor<i32>     // anchored before parallel
%data_data = sde.mu_data {shared} : tensor<100xi32>

arts_sde.cu_region <parallel> {
  // --- single region: sum += 1000 ---
  %sum_tok = sde.mu_token <readwrite> %sum_data : tensor<i32>
  %new_sum = sde.cu_codelet(%sum_tok) -> tensor<i32> {
    ^bb0(%sum: tensor<i32>):
      %val = tensor.extract %sum[] : tensor<i32>
      %added = arith.addi %val, %c1000_i32 : i32
      %result = tensor.insert %added into %sum[] : tensor<i32>
      sde.yield %result : tensor<i32>
  }

  // --- for loop: data[i] *= 2 ---
  %data_tok = sde.mu_token <readwrite> %new_sum /*uses advanced sum*/, %data_data
  // (data_tok carries the data tensor)
  ...su_iterate with tensor ops...
}
// boundary materialize:
%final_sum = tensor.extract %current_sum[] : tensor<i32>
memref.store %final_sum, %alloca_0[] : memref<i32>
```

**Key**: `sum` is now an SSA tensor value (`%new_sum`) that flows through the graph.
EdtStructuralOpt can NEVER sink it because it's an SSA value, not a memref alloca.
The `omp single` semantics are preserved because the tensor result is explicitly
threaded to subsequent consumers.

---

## 3. Implementation Plan

### Phase 0: Preparation (no functional change) -- **DONE**

- [x] **P0.1**: Audit `cu_region` / `su_iterate` / `cu_task` TableGen definitions to
  understand what operand changes are needed.
- [x] **P0.2**: Ensure `sde.mu_data`, `sde.mu_token`, `sde.cu_codelet` ops and their
  ConvertSdeToArts lowering patterns work for rank-0 tensors (`tensor<i32>` for scalars).
  Add lit tests if missing.
- [x] **P0.3**: Verify that ConvertSdeToArts can lower `cu_codelet` + `mu_token` for
  `cu_region <single>` semantics (single-execution EDT with shared data access).

### Phase 1: New RaiseToTensor (tensor-first) -- **DONE** (different mechanism: iter_args on cu_region instead of mu_data/mu_token)

- [ ] **P1.1**: Create new `RaiseToTensor.cpp` (replaces existing). Algorithm:
  1. Collect memref.alloca ops used inside SDE regions
  2. Classify access modes (read/write/readwrite) per compute unit
  3. Emit `sde.mu_data` anchors
  4. Walk compute units in program order, create `cu_codelet` + `mu_token`
  5. Thread tensor SSA values through the graph
  6. Boundary-materialize back to memref at scope exit
- [ ] **P1.2**: Handle `cu_region <single>` -- the motivating case. The single body
  becomes a `cu_codelet` with readwrite token for `sum`.
- [ ] **P1.3**: Handle `su_iterate` -- convert memref.load/store in loop body to
  tensor.extract/insert. Thread loop data via codelet tokens.
- [ ] **P1.4**: Handle `cu_region <parallel>` -- the outer region. Its captured
  memrefs become `mu_data` handles; inner regions consume via tokens.
- [ ] **P1.5**: Handle nested regions -- `cu_region <single>` inside `cu_region <parallel>`.
  Token SSA threading must respect nesting.
- [ ] **P1.6**: Lit tests for each construct (scalar, array, nested, single+for combo).

### Phase 2: Revise RaiseToLinalg (tensor input)

- [ ] **P2.1**: Modify RaiseToLinalg to accept tensor-backed bodies (from Phase 1)
  instead of memref-backed bodies. Pattern-match `tensor.extract`/`tensor.insert`
  indexed by induction variable into `linalg.generic`.
- [ ] **P2.2**: Remove memref-backed linalg carrier creation (the old path).
- [ ] **P2.3**: Since input is already tensor-backed, the classification step
  (`StructuredOpAnalysis`) should work on tensor indexing maps directly.
- [ ] **P2.4**: Update lit tests.

### Phase 3: Pipeline Reorder -- **DONE** (commit `17556d53`)

- [x] **P3.1**: Move `RaiseToTensor` to position #1 (right after ConvertOpenMPToSde).
- [x] **P3.2**: Move `RaiseToLinalg` to position #2.
- [x] **P3.3**: Remove old `RaiseToTensor` (carrier converter) -- subsumed.
- [x] **P3.4**: Remove `RaiseMemrefToTensor` -- subsumed by new `RaiseToTensor`.
- [x] **P3.5**: Verify ScopeSelection/ScheduleRefinement/etc. work on tensor-level IR.
  These passes may need updates to read tensor types instead of memref types.
- [x] **P3.6**: Full test suite pass (`dekk carts test --suite all`).

### Phase 4: ConvertSdeToArts Updates

- [ ] **P4.1**: Verify `cu_codelet` lowering handles the new patterns from Phase 1
  (especially rank-0 tensors for scalars, and nested codelets from single+for).
- [ ] **P4.2**: Verify `mu_data` lowering creates correct DBs for shared scalars.
- [ ] **P4.3**: Ensure the `<single>` EDT gets proper DB acquire/release for
  the shared `sum` tensor (not a local alloc).
- [ ] **P4.4**: Verify boundary materialization pattern is correct for main-function
  readers that consume `sum` after the parallel region.

### Phase 5: Cleanup -- **partial**

- [ ] **P5.1**: Remove dead code from old raise passes.
- [ ] **P5.2**: Rename files for clarity: `state/RaiseToTensor.cpp` is the canonical
  tensor-raising pass. No more "RaiseMemrefToTensor".
- [ ] **P5.3**: Update `docs/architecture/linalg-in-sde.md` to reflect new pipeline.
- [ ] **P5.4**: Update MEMORY.md entries.

---

## 4. Risks and Mitigations

### R1: StructuredOpAnalysis currently expects memref indexing
**Mitigation**: The analysis already handles `linalg.generic` indexing maps. With
tensor-backed linalg, the maps are identical -- only the operand types change from
memref to tensor. Minimal changes expected.

### R2: ScopeSelection/ScheduleRefinement may inspect memref types
**Mitigation**: These passes primarily inspect SDE op attributes and cost model.
If they check operand types, update to handle tensor types. Audit needed in Phase 3.

### R3: ConvertSdeToArts lowering for nested codelets
**Mitigation**: The existing `lowerCuCodelet` handles single-level codelets. For
nested single-inside-parallel, the outer parallel region creates the `mu_data` DBs,
and the inner single codelet acquires from those DBs. This matches the existing
multi-level EDT/DB pattern in ARTS.

### R4: Boundary materialization race conditions
**Mitigation**: Reuse the barrier + store pattern from RaiseMemrefToTensor v1 (Gap D).
The barrier ensures all async EDTs complete before final tensor values are extracted
back to memref.

### R5: Performance regression in the raise step
**Mitigation**: The new RaiseToTensor is a single-pass walk + rewrite. The old pipeline
had three overlapping passes. Net pass count decreases by 1.

---

## 5. Success Criteria

1. `parallel_for/single` produces `sum=1000` (the original bug is fixed)
2. All 218 pass tests continue to pass
3. All 26 e2e tests maintain their current status
4. The SDE pipeline has exactly TWO raise passes: RaiseToTensor then RaiseToLinalg
5. No memref.load/store survives inside SDE compute units after RaiseToTensor
6. Analysis passes (ScopeSelection etc.) operate on tensor-level IR

---

## 6. Files Affected

### New/Rewritten
- `lib/arts/dialect/sde/Transforms/state/raising/RaiseToTensor.cpp` -- complete rewrite

### Modified
- `lib/arts/dialect/sde/Transforms/state/raising/RaiseToLinalg.cpp` -- accept tensor input
- `lib/arts/dialect/sde/Transforms/state/raising/LowerToMemref.cpp` -- tensor-to-memref lowering (inverse of RaiseToTensor)
- `tools/compile/Compile.cpp` -- pipeline reorder (lines 658-669)
- `lib/arts/dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp` -- verify codelet lowering
- `include/arts/dialect/sde/Transforms/Passes.td` -- remove RaiseMemrefToTensor pass def

### Deleted
- `lib/arts/dialect/sde/Transforms/state/raising/RaiseMemrefToTensor.cpp` -- subsumed (Phase 5)

### Test Files
- `lib/arts/dialect/sde/test/raise_to_tensor_*.mlir` -- new tensor-first tests
- `lib/arts/dialect/sde/test/raise_to_linalg_*.mlir` -- updated for tensor input

---

## 7. Prior Art: Two Converging Design Threads

This design does not come from nowhere. Two prior design threads independently
arrived at the same conclusion: SDE data must be tensor-valued. This section
documents them and shows how this proposal unifies both.

### 7.1 Thread A: RFC `sde.cu_codelet` + `RaiseMemrefToTensor` (commit `e9d28bfb`)

**Document**: `docs/compiler/raise-memref-to-tensor-rfc.md` (714 lines).

The RFC introduced three new SDE ops and a transform pass:

| Op | Role |
|----|------|
| `sde.cu_codelet` | `IsolatedFromAbove` compute unit — all I/O via tokens + yield, no captures |
| `sde.mu_token` | Tensor-path access handle with mode (`read`/`write`/`readwrite`) — analogue of `mu_dep` |
| `sde.mu_data` | Declarative data anchor — replaces inherited `memref.alloca` |

Key RFC insights that carry forward:

1. **"Tensor-typed Mem2Reg across an SDE region"** (RFC §2) — the transformation
   is a generalization of LLVM's `mem2reg` to tensor types over MLIR regions.
2. **Dep-drop elimination is structural** (RFC §4.4, rules V1-V13) — once data is
   SSA-visible via tokens, dropping a dependency requires dropping a use-def edge,
   which the MLIR verifier refuses. The entire dep-drop bug class is eliminated.
3. **RFC §7.1 audit**: ALL 13 SDE analysis/transform passes walk `su_iterate` only.
   None traverse `cu_task`/`cu_region` bodies. The raise is what puts task data
   "into the same regime" as loop data.
4. **"Neutral on loops, transformative on tasks"** (RFC §7.4) — the raise is a
   no-op on loop-centric programs and unlocks task-centric programs.

**What the RFC got right**: The codelet/token/data op surface, the SSA invariants,
the boundary materialization pattern, the per-pass adaptation analysis.

**What the RFC got wrong**: Placement. The RFC placed `RaiseMemrefToTensor` AFTER
analysis passes (`ScopeSelection → ScheduleRefinement → ChunkOpt → ReductionStrategy
→ RaiseMemrefToTensor`), arguing that analysis passes need the memref form. But the
per-pass dump analysis (§8 below) proves those passes are no-ops on cu_region bodies.
There is no cost to running the raise first.

**What the RFC missed**: It only targeted `cu_task` bodies, not `cu_region`. The
`parallel_for/single` bug is in a `cu_region <single>` — exactly the blind spot.
This design extends the raise to cover ALL SDE compute units.

### 7.2 Thread B: DestinationStyleOpInterface on SDE Ops (commits `558183d2`, `1c452da5`)

**Document**: `docs/architecture/sde-dialect.md` §175-185 ("Planned But Not Implemented").

This thread proposed making SDE ops natively tensor-aware:

- First-class tensor `ins`/`outs` on `cu_region`, `su_iterate`, `cu_task`
- Tensor-native dependency modeling as the primary SDE surface
- Replacing the memref-authoritative loop body with a tensor-authoritative form
- `DestinationStyleOpInterface` so SDE ops carry destination-passing semantics

**What Thread B got right**: The end-state vision — SDE ops should be tensor-native
with explicit ins/outs, not memref-reliant with invisible side-effects.

**What Thread B deferred**: Everything. These are listed as "Planned But Not
Implemented" in the current dialect docs. No code was written.

### 7.3 Unification: This Proposal

This design unifies both threads:

| Aspect | Thread A (RFC) | Thread B (DestStyle) | This Proposal |
|--------|---------------|---------------------|---------------|
| Scope | `cu_task` only | All SDE ops | All SDE ops |
| Mechanism | `cu_codelet` + `mu_token` | `ins`/`outs` on existing ops | `cu_codelet` + `mu_token` for tasks; tensor threading for regions |
| Ordering | After analysis passes | Not specified | BEFORE analysis passes (tensor-first) |
| Carrier model | Independent of linalg carriers | Replaces carrier model | Subsumes carrier model — linalg operates on tensor input directly |
| Implementation | v1 shipped (cu_task only, gated off) | Unimplemented | Extends v1 to all compute units, reorders pipeline |

The key insight this design adds: **raise first, analyze second**. The RFC's
conservative placement was driven by the assumption that analysis passes need
memref form. The per-pass dump evidence (§8) disproves this. By raising to
tensor immediately after `ConvertOpenMPToSde`, we get:

1. **Complete coverage**: `cu_region <single>` (the `parallel_for/single` bug)
   is covered by the same mechanism that covers `cu_task` and `su_iterate`.
2. **Unified input**: Analysis passes see tensor-level IR everywhere, eliminating
   the mixed memref/tensor form that complicates every downstream pass.
3. **Carrier simplification**: `RaiseToLinalg` receives tensor input directly,
   eliminating the memref→tensor carrier conversion step entirely.

### 7.4 Existing Infrastructure Reuse

The RFC's ops are already implemented and have ConvertSdeToArts lowering:

| Component | Status | Commits |
|-----------|--------|---------|
| `sde.cu_codelet` op definition | Implemented | `e0abbad8` |
| `sde.mu_token` op definition | Implemented | `e0abbad8` |
| `sde.mu_data` op definition | Implemented | `e0abbad8` |
| ConvertSdeToArts codelet lowering | Implemented | `e0abbad8` |
| ConvertSdeToArts tensor-path lowering | Implemented | `e0abbad8` |
| RaiseMemrefToTensor v1 (cu_task only) | Implemented, gated off | `af0ecf5f` |
| Boundary-write race fix (barrier + sink guard) | Implemented | `bab33916` |
| Verifier rules V1-V13 | Partially implemented | `e0abbad8` |

This means Phase 0 (preparation) is largely done. The main work is extending
the raise to cover `cu_region` and reordering the pipeline.

---

## 8. Per-Pass Dump Evidence: Analysis Passes are No-Ops on `cu_region`

Examined the `parallel_for/single` pipeline dumps at every SDE pass.

### 8.1 Pass-by-pass behavior on the `<single>` body

| Pass | File | Effect on `cu_region <single>` body |
|------|------|-------------------------------------|
| ConvertOpenMPToSde (001) | `passes/03_openmp-to-arts/001_*.mlir` | Creates the `cu_region <single>` with memref body |
| ScopeSelection (002) | `passes/03_openmp-to-arts/002_*.mlir` | Adds `scope(<local>)` to `cu_region` — attribute only, body untouched |
| ScheduleRefinement (003) | `passes/03_openmp-to-arts/003_*.mlir` | **NO-OP** — only targets `su_iterate` schedule attrs |
| ChunkOpt (004) | `passes/03_openmp-to-arts/004_*.mlir` | **NO-OP** — only targets `su_iterate` chunk strategy |
| ReductionStrategy (005) | `passes/03_openmp-to-arts/005_*.mlir` | **NO-OP** — only targets `cu_reduce` / `su_iterate` reductions |
| RaiseToLinalg (006) | `passes/03_openmp-to-arts/006_*.mlir` | **NO-OP** on `<single>` — only walks `su_iterate` bodies |
| RaiseToTensor (007) | `passes/03_openmp-to-arts/007_*.mlir` | **NO-OP** on `<single>` — only rewrites linalg carriers |
| LoopInterchange (008) | `passes/03_openmp-to-arts/008_*.mlir` | **NO-OP** — only targets `su_iterate` |
| TensorOpt (009) | `passes/03_openmp-to-arts/009_*.mlir` | **NO-OP** — only targets `su_iterate` carriers |
| StructuredSummaries (010) | `passes/03_openmp-to-arts/010_*.mlir` | **NO-OP** — only targets `su_iterate` |
| ElementwiseFusion (011) | `passes/03_openmp-to-arts/011_*.mlir` | **NO-OP** — only targets `su_iterate` carriers |
| RaiseMemrefToTensor (012) | `passes/03_openmp-to-arts/012_*.mlir` | **NO-OP** — only targets `cu_task` bodies |
| DistributionPlanning (013) | `passes/03_openmp-to-arts/013_*.mlir` | **NO-OP** — only targets `su_iterate` |
| IterationSpaceDecomposition (014) | `passes/03_openmp-to-arts/014_*.mlir` | **NO-OP** — only targets `su_iterate` |
| BarrierElimination (015) | `passes/03_openmp-to-arts/015_*.mlir` | **NO-OP** — only reads `mu_dep` side-effect scope |
| ConvertSdeToArts (016) | `passes/03_openmp-to-arts/016_*.mlir` | Converts `cu_region <single>` to `arts.edt <single>`. Body unchanged. |

**Result**: 14 of 16 SDE passes are NO-OPs on the `<single>` body.
Only ConvertOpenMPToSde (creates it) and ConvertSdeToArts (lowers it)
touch the `cu_region <single>`. The memref `sum` passes through the entire
SDE pipeline without any tensor raising, analysis, or transformation.

### 8.2 The `su_iterate` path works correctly

The `data[i] *= 2` loop inside `su_iterate` is correctly handled:
- RaiseToLinalg (006) creates a `linalg.generic` carrier with `elementwise` classification
- RaiseToTensor (007) converts the carrier from memref-backed to tensor-backed
- StructuredSummaries (010) stamps `dep_distances`, `vectorize_width`, `unroll_factor`
- ConvertSdeToArts (016) stamps ARTS contracts from the carrier

This confirms: **the loop path works; only the task/region path is broken**.
The tensor-first pipeline fixes the region path by making it go through the
same tensor-level machinery.

### 8.3 Implication for pipeline ordering

Since ScopeSelection only sets an attribute on the `cu_region` op (not its body),
and all other analysis passes are no-ops on `cu_region` bodies, there is
**zero cost to running RaiseToTensor before them**. The raise does not remove
any information the analysis passes need — there is no information in the
`cu_region` body that they read.

For `su_iterate`, the analysis passes (ScopeSelection, ScheduleRefinement,
ChunkOpt) read attributes on the `su_iterate` op itself, not the loop body's
memref form. A tensor-backed loop body provides the same attributes. The only
passes that read the body are RaiseToLinalg (which would read tensor ops
instead of memref ops) and StructuredSummaries (which reads linalg carriers).

**Conclusion**: Tensor-first ordering is safe for all current passes.

### 1.4 Implementation Status (as of 2026-04-15)

Problems #3 (wrong ordering) and #4 (cu_region blind spot) from Section 1.3 are now RESOLVED:
- Wrong ordering: Fixed by commit `17556d53` (SDE pipeline reorder: dep passes before effect passes)
- cu_region blind spot: Fixed by commit `332bb7f9` (tensor-first step 0: cu_region iter_args + barrier after single/master)

The current pipeline uses iter_args on cu_region ops instead of the mu_data/mu_token mechanism proposed in Section 2.3. This is a simpler mechanism that achieves the same SSA data flow goals.
