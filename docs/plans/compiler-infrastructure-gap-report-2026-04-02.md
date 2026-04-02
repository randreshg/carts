# CARTS Compiler Infrastructure Gap Report

Date: 2026-04-02

## Purpose

This document records what is still missing from the simplification and
migration work tracked in
`/home/raherrer/.claude/plans/concurrent-soaring-quiche.md`.

It also captures the additional lowering-contract gaps discovered while trying
to make `arts.lowering_contract` the canonical downstream authority.

## Summary

Several structural cleanups from the original plan are already in place:

- `arts.for` already implements `LoopLikeOpInterface`.
- The old monolithic concurrency cleanup has already been split into
  `post-distribution-cleanup`, `db-partitioning`, `post-db-refinement`, and
  `late-concurrency-cleanup`.
- `LoweringContractInfo` is already decomposed into `SemanticPattern`,
  `SpatialLayout`, and `AnalysisRefinement`.
- Typed `#arts.pattern<>` and `#arts.contract<>` payloads already exist.
- Partition predicates already exist and are used broadly.

The main missing work is no longer basic stage splitting or attribute shape.
The real remaining gap is behavioral authority:

1. lowering-contract semantics are still spread across ad hoc helpers and
   caller-local merge logic;
2. graph-backed partition facts and stencil bounds are still required in
   downstream paths;
3. some upstream reuse items from the original plan are only partially
   complete; and
4. a few infrastructure items, such as pass statistics and LLVM runtime
   builder cleanup, are still open.

## Status Against The Original Plan

### Phase 1: Upstream Index / Stride / View Utilities

Status: done

Already landed:

- `Codegen.cpp` already uses `affine::linearizeIndex`.
- `Codegen.cpp` already uses `memref::computeStridesIRBlock`.
- `DbUtils.cpp` already uses `computeSuffixProduct`.
- `ValueAnalysis::stripMemrefViewOps()` already uses
  `memref::skipFullyAliasingOperations`.
- `ArtsCodegen::computeTotalElements()` simplified from unnecessary suffix-
  product computation to direct fold-multiply (no upstream total-elements
  utility exists in MLIR).
- `DbUtils::hasNonPartitionableHostViewUses()` documented as correctly non-
  simplifiable — upstream utilities walk UP the def chain while this function
  walks DOWN the use chain, plus it handles CARTS-specific ops.

### Phase 2: ValueAnalysis Simplification

Status: done

Already landed:

- `ValueAnalysis.cpp` already uses `ValueBoundsConstraintSet::areEqual`.
- `ValueAnalysis.cpp` already uses `computeConstantBound()` for lower and upper
  bounds.
- `isConstantAtLeastOne()` simplified — removed redundant `ConstantOp` +
  `IntegerAttr` check already handled by `getConstantIndex()`.
- `areValuesEquivalent()` documented as correctly non-simplifiable — handles
  non-index types, commutative operand matching, and structurally identical
  expression trees that `ValueBoundsConstraintSet::areEqual` cannot handle.
- `isProvablyNonZero()` documented as correctly non-simplifiable — handles
  non-index integer constants and max propagation (`maxui`/`maxsi`) that the
  bounds infrastructure does not cover.

### Phase 3: LoopAnalysis Simplification

Status: mostly done

Already landed:

- `LoopAnalysis` now treats `LoopLikeOpInterface` as the standard loop test.
- `LoopUtils` already uses `scf::ForOp::getStaticTripCount()` for SCF loops.

Remaining note:

- `arts::ForOp` still needs custom trip-count handling, which is expected and
  not an open upstream-reuse bug by itself.

Primary files:

- `lib/arts/utils/LoopUtils.cpp`
- `lib/arts/analysis/loop/LoopAnalysis.cpp`

### Phase 4: `arts.for` LoopLikeOpInterface

Status: done

Evidence:

- `include/arts/Ops.td`
- `lib/arts/Dialect.cpp`

### Phase 5: LoweringContractInfo Decomposition

Status: structurally done, behaviorally incomplete

Already landed:

- `LoweringContractInfo` is split into `SemanticPattern`, `SpatialLayout`, and
  `AnalysisRefinement`.
- `PatternAttr` and `ContractAttr` exist as typed IR payloads.
- `mergeLoweringContractInfo` now returns `ContractChange` enum
  (`Unchanged`/`Changed`) for monotone update tracking (Section 6.6).

Still missing:

- the code still lacks one authoritative resolver / combine / projection API
  for these pieces;
- callers still merge and clamp contract state manually.

Primary files:

- `include/arts/utils/LoweringContractUtils.h`
- `lib/arts/utils/LoweringContractUtils.cpp`
- `include/arts/Attributes.td`

### Phase 6: Partition Predicates

Status: done

Evidence:

- `include/arts/utils/PartitionPredicates.h`
- broad adoption across analysis, transforms, and lowering code

### Phase 7: LLVM Builder Simplification

Status: mostly done

- `ArtsCodegen::getOrCreateRuntimeFunction()` uses `func::lookupFnDecl()` +
  `func::createFnDecl()`. Upstream `func::lookupOrCreateFnDecl()` **cannot be
  used** because it defaults null `resultType` to `i64`, breaking void-returning
  runtime functions. Documented as blocked.
- Memref-descriptor construction **audited** (14 sites): all memref descriptor
  construction is properly delegated through Polygeist's `Pointer2MemrefOp`.
  CARTS never directly constructs memref descriptors. `ArtsHintBuilder` is a
  2-field ARTS-specific struct, not a memref descriptor. **No refactoring
  needed** — correct layering: CARTS → Polygeist → MLIR MemRefDescriptor.

Primary files: all audited, no changes needed.

### Phase 8: Pass Statistics

Status: done

Current state:

- 19 passes now declare `llvm::Statistic` counters (~94 total):
  `DbPartitioning.cpp` (10), `CreateDbs.cpp` (7), `EdtTransformsPass.cpp` (6),
  `DbTransformsPass.cpp` (4), `EpochOpt.cpp` (8, member-based),
  `ForLowering.cpp` (7, member-based), `DbModeTightening.cpp` (5),
  `DbScratchElimination.cpp` (3), `ConvertOpenMPToArts.cpp` (5),
  `CreateEpochs.cpp` (3), `EdtDistribution.cpp` (3),
  `ParallelEdtLowering.cpp` (3), `DbLowering.cpp` (4),
  `EdtLowering.cpp` (4), `EpochLowering.cpp` (5),
  `Concurrency.cpp` (5), `ForOpt.cpp` (3),
  `EdtStructuralOpt.cpp` (7), `ConvertArtsToLLVM.cpp` (5).
- All major optimization, transformation, lowering, and codegen passes covered.

Primary files: all covered.

### Phase 9: EdtInfo Dead Field Cleanup

Status: no longer needed

Reason:

- `EdtInfo::maxLoopDepth` is actively computed and consumed, so this should not
  be treated as dead work anymore.

Primary files:

- `include/arts/analysis/edt/EdtInfo.h`
- `lib/arts/analysis/edt/EdtAnalysis.cpp`
- `lib/arts/passes/opt/edt/EdtTransformsPass.cpp`

## Lowering-Contract Gaps Found During Migration

This is the highest-priority missing work.

The shared payload story is partially complete already:

- `#arts.pattern<>` exists for semantic pattern payload.
- `#arts.contract<>` exists for static contract payload.
- `arts.lowering_contract` exists as the IR carrier.

What is still missing is a thin behavioral layer that makes those payloads
authoritative and easy to compose.

### 1. Resolver Semantics Are Split Across Overlapping Helpers

Current helper surface:

- `getLoweringContract(Value)`
- `getSemanticContract(Operation *)`
- `getLoweringContract(Operation *, OpBuilder &, Location)`
- `resolveLoopDistributionContract(Operation *)`

Problem:

- these APIs do different things, but the names do not make the distinction
  obvious;
- callers still need to know when they are reading persisted pointer state,
  reading semantic attrs, or materializing dynamic stencil payloads.

Primary files:

- `include/arts/utils/LoweringContractUtils.h`
- `lib/arts/utils/LoweringContractUtils.cpp`

### 2. Merge Semantics Are Centralized, But Still Too Weak

`mergeLoweringContractInfo(...)` is better than open-coded field merging, but
it is still only a partial answer.

Current behavior:

- fills missing semantic fields;
- ORs boolean refinement flags;
- prefers higher-rank spatial payloads;
- normalizes only a subset of rank-shaped vectors.

What is missing:

- explicit ordered combine semantics;
- explicit overwrite / patch semantics;
- explicit halo union semantics;
- explicit owner-rank normalization semantics;
- optional change reporting for monotone updates.

Primary file:

- `lib/arts/utils/LoweringContractUtils.cpp`

### 3. Task-Acquire Rewrite Still Reimplements Contract Logic Locally

`applyTaskAcquireContractMetadata(...)` is still the clearest duplication hot
spot in the codebase.

It currently does all of the following in one place:

- merge pointer and operation contracts;
- clamp owner dimensions to supported partition rank;
- derive or clamp halo min/max offsets;
- fold and clamp write footprints;
- infer a center offset;
- infer or refine task block shape;
- rewrite the pointer contract back onto the acquire result.

This should become the first consumer of a single contract-state API.

Primary file:

- `lib/arts/transforms/edt/EdtRewriter.cpp`

### 4. ForLowering Still Seeds Rewrite State Manually

`ForLowering` still manually enriches `AcquireRewriteContract` from the loop's
semantic contract before task-acquire rewriting.

That means the generic rewrite path is not fully generic yet. The projection
from loop contract to acquire rewrite state should be centralized.

Primary file:

- `lib/arts/passes/transforms/ForLowering.cpp`

### 5. DbTransformsPass Still Performs Halo Union Manually

`DbTransformsPass::consolidateStencilHalos()` still manually merges raw
operation offsets with persisted contract offsets.

This is another sign that the current contract utilities do not yet expose a
first-class "combine and normalize halo window" operation.

Primary file:

- `lib/arts/passes/opt/db/DbTransformsPass.cpp`

### 6. DbAnalysis Still Needs Graph-Backed Fallbacks

`DbAnalysis` still refines contracts from graph-side facts:

- `DbAcquirePartitionFacts`
- `DbAcquireNode::getStencilBounds()`

This is the biggest blocker to calling canonical lowering contracts
behaviorally complete.

Current consequences:

- downstream code still depends on graph-derived stencil bounds in some cases;
- removing graph fallbacks before worker-acquire projection is complete is not
  safe;
- the IR contract is the documented authority, but not yet the complete
  behavioral authority in all stencil paths.

Primary files:

- `include/arts/analysis/graphs/db/DbNode.h`
- `lib/arts/analysis/db/DbAnalysis.cpp`
- `lib/arts/analysis/graphs/db/DbAcquireNode.cpp`
- `lib/arts/analysis/graphs/db/PartitionBoundsAnalyzer.cpp`
- `lib/arts/analysis/graphs/db/DbBlockInfoComputer.cpp`

### 7. Canonical Contracts Are Still Incomplete Before DB Partitioning

The intended end state is:

- worker/task pointer contracts should already carry the full halo and owner
  semantics before `db-partitioning`;
- `DbPartitioning` should not need to rediscover stencil behavior from graph
  side channels.

That end state has not been reached yet for all stencil-family cases.

Known pressure points:

- contracts can still reach downstream stages with `ownerDims` but without the
  full min/max halo window;
- this keeps graph-side stencil bounds alive as a correctness fallback.

Known benchmark families that exposed this gap during investigation:

- `specfem3d` stress-update style stencil paths
- `sw4lite` `rhs4sg_base`
- `sw4lite` `vel4sg_base`

Known positive reference:

- `seidel-2d` already demonstrates the intended stronger contract carry path in
  simpler stencil lowering flows.

## What The Common `arts.pattern<>` / `arts.contract<>` Story Still Needs

The common attribute direction is fundamentally correct, and most of the IR
shape already exists.

What is still missing is not another wave of pattern-specific top-level attrs.
The missing pieces are:

1. a single contract-state abstraction used by lowering and analysis code;
2. an ordered combine operation for persisted facts;
3. a separate patch / overwrite operation for deliberate refinement;
4. projection helpers for acquire rewriting, owner-dim resolution, and
   rank-clamped stencil windows; and
5. one naming scheme that makes "resolve", "read semantic", and
   "materialize dynamic payload" impossible to confuse.

## Attributor-Inspired Direction

The useful idea to borrow from Attributor is not the whole framework. It is the
discipline:

- facts should live in a small explicit state object;
- combines should be monotone and ordered;
- change detection should be explicit when useful;
- IR-visible facts and transient inferred state should be separate concepts.

For CARTS, that suggests a thin contract-state layer, not a heavyweight
abstract-attribute engine.

Recommended shape:

- keep `LoweringContractInfo` as the DTO / serialization shape;
- add one explicit resolver API for reading effective contract state;
- add one explicit combine API for ordered persisted merges;
- add one explicit patch API for deliberate refinements;
- add one explicit projection API for acquire-rewrite consumers.

Avoid:

- an Attributor-sized framework;
- silent last-writer-wins behavior;
- another parallel contract language outside `arts.lowering_contract`.

## Research Tasks

These are explicit research tasks, not just implementation leftovers.

### Research Task 1: Attributor-Inspired Contract State

Goal:

- decide how far CARTS should borrow the Attributor model for ARTS contract
  state without importing the full framework.

Questions to answer:

- should the effective lowering contract be represented as one explicit
  state/value object with monotone ordered combine semantics;
- should combine operations report change, similar to
  `ChangeStatus` / `ChangeResult`, so refinement passes can express whether
  they actually strengthened the contract;
- should "fill missing", "patch", and "project" be modeled as distinct
  operations instead of one overloaded merge helper.

Desired output:

- a small design note for `LoweringContractUtils` that defines:
  - resolver semantics;
  - ordered combine semantics;
  - explicit patch semantics;
  - acquire-rewrite projection semantics;
  - optional change-reporting semantics.

Constraint:

- copy the Attributor discipline, not the Attributor framework.
- keep the result thin, local, and production-like.

### Research Task 2: MLIR Attr-Interface Boundary

Goal:

- decide whether any contract behavior belongs on a dedicated attr interface,
  or whether the current typed attrs should stay mostly declarative.

Questions to answer:

- should `#arts.pattern<>` and `#arts.contract<>` remain thin payload attrs
  with behavior living in utilities;
- is there a narrow interface worth adding for attr-local behavior only;
- where is the right boundary between typed attrs, `arts.lowering_contract`,
  and non-IR transient helper state.

Reference inspirations:

- MLIR attr interfaces
- `ConvertToLLVMAttrInterface`
- DLTI-style ordered `combineWith` behavior

Constraint:

- do not create a second semantic authority outside the canonical IR contract.

### Research Task 3: Contract Authority Completion

Goal:

- identify the exact remaining cases where graph-backed partition facts are
  still required because canonical contracts are incomplete.

Questions to answer:

- which stencil flows still reach `db-partitioning` without full halo state on
  worker/task pointer contracts;
- which remaining uses of `DbAcquirePartitionFacts` and
  `DbAcquireNode::getStencilBounds()` are true semantic dependencies versus
  temporary migration crutches;
- what is the smallest safe sequence for deleting those fallbacks.

Success condition:

- the report can name a concrete point after which graph-side stencil fallback
  logic may be deleted safely.

## Recommended Resume Order

When this work resumes, the next slice should be:

1. finish Research Task 1 and write down the thin Attributor-inspired contract
   state model;
2. finish Research Task 2 and lock the attr/interface boundary;
3. introduce a thin contract-state API in `LoweringContractUtils`;
4. convert `EdtRewriter::applyTaskAcquireContractMetadata(...)` to that API;
5. convert `ForLowering` and `DbTransformsPass` to the same API;
6. convert `DbAnalysis` seed/refinement paths to use the same combine and patch
   semantics;
7. finish Research Task 3 and verify which graph-backed fallbacks are still
   semantically required;
8. only then remove graph-backed stencil fallback logic; and
9. after contract authority is stable, finish the smaller infrastructure items
   that are still open from the original plan:
   pass statistics, LLVM runtime-function builder cleanup, and the remaining
   ValueAnalysis / DbUtils simplifications.

## Do Not Rework

The following items should not be reopened as "missing":

- `arts.for` loop interface work;
- the split of the old concurrency cleanup into smaller real pipeline stages;
- `LoweringContractInfo` structural decomposition;
- partition predicate introduction;
- `EdtInfo::maxLoopDepth` removal.

---

## Production Compiler Infrastructure Comparison

The sections below expand the gap report with findings from three focused research
investigations into production compiler infrastructure: LLVM's Attributor
framework, MLIR and IREE's analysis/pass infrastructure, and a cross-compiler
feature matrix covering LLVM, GCC, IREE, Halide, and TVM.

### 1. LLVM Attributor Architecture

The Attributor is a fixed-point interprocedural abstract analysis framework in
LLVM. Its architecture is built on four interlocking design patterns that are
worth understanding even though CARTS should not import the full framework.

#### 1.1 Core Design Patterns

**Fixed-point iteration.** The Attributor seeds AbstractAttribute instances for
all IR positions, then iteratively calls `updateImpl()` on each until no
attribute reports a state change (fixpoint). Termination is guaranteed by
monotone update functions over finite lattices, with a hard iteration cap
(default 32) as a safety net.

**AbstractState lattices.** Every attribute carries two parallel states:

- *Assumed* (optimistic): the best property that has not been disproved yet.
- *Known* (conservative): what has been proved from IR evidence.
- Invariant: `Known ≤ Assumed` always holds.

Built-in state types include `BooleanState`, `BitIntegerState`,
`IncIntegerState`, `DecIntegerState`, `IntegerRangeState`, and `SetState`. The
assumed/known pair makes optimistic reasoning safe: the framework starts
optimistic and narrows toward the conservative bound.

**Automatic dependency tracking via `getAAFor()`.** When attribute X queries
attribute Y via `A.getAAFor<AAType>(queryingAA, position, depClass)`, the
framework automatically records a dependency edge Y → X. If Y later changes
state, X is re-evaluated. Dependency classes are:

- `REQUIRED`: X is invalid if Y is invalid.
- `OPTIONAL`: X might stay valid even if Y changes.
- `NONE`: no automatic recording (caller manages manually).

**IRPosition anchoring.** Every attribute is anchored to an explicit IR
position (function, argument, return value, call-site, call-site argument, call-
site return, or floating value). Subsuming positions propagate information
upward (e.g., call-site argument → callee argument → callee function).

**InformationCache.** A one-time pre-scan of the module builds opcode maps,
read/write instruction lists, and must-be-executed context information. All
attributes share this cache, eliminating redundant IR traversals.

#### 1.2 Comparison to CARTS AnalysisManager

| Aspect | LLVM Attributor | CARTS AnalysisManager |
|--------|-----------------|----------------------|
| Type | Interprocedural fixed-point | Analysis holder / lazy factory |
| Dependency tracking | Automatic via `getAAFor()` | None (manual) |
| State management | Optimistic/pessimistic lattices | Binary (has graph or not) |
| Fixed-point iteration | Built-in (32-iteration cap) | External (in-pass loops) |
| Invalidation | Dependency-triggered | Manual `AM->invalidate()` |
| IR positions | Rich `IRPosition` (8 kinds) | Implicit (operation / value) |
| Caching | Fine-grained per attribute | Coarse per analysis type |
| Scale | 10K+ lines, ~50 abstract attrs | ~400 lines, 8 analyses |

#### 1.3 What CARTS Should Borrow

The useful idea is the *discipline*, not the framework:

- Facts should live in a small explicit state object.
- Combines should be monotone and ordered.
- Change detection should be explicit when useful.
- IR-visible facts and transient inferred state should be separate concepts.

For CARTS, this suggests a thin contract-state layer (see Section 6 below), not
a heavyweight abstract-attribute engine.

#### 1.4 What CARTS Should Avoid

- **Full Attributor port.** CARTS operates on MLIR, not LLVM IR; the framework
  is deeply tied to LLVM's `Value`, `CallBase`, and `Function` abstractions.
- **File fragmentation.** The Attributor spans 7 `AttributorAttributes*.cpp`
  files totaling ~10K lines. CARTS should keep pass implementations monolithic.
- **Deep nesting of analysis types.** CARTS has 8 analyses; 50 would be
  over-engineering.
- **Manifest-phase complexity.** CARTS already has dedicated lowering passes
  that write results to IR.

### 2. MLIR Native Analysis and Pass Infrastructure

#### 2.1 MLIR's AnalysisManager Design

MLIR provides a built-in `AnalysisManager` with the following characteristics:

- **Lazy-initialized, type-safe caching.** Analyses are computed on demand via
  `getAnalysis<T>()` and cached per IR operation using `TypeID`.
- **PreservedAnalyses tracking.** Each pass returns a `PreservedAnalyses` set
  declaring which analyses survive. The PassManager automatically invalidates
  unpreserved analyses after each pass.
- **Polymorphic AnalysisConcept.** Each analysis wraps itself in an
  `AnalysisModel<T>` providing a virtual `isInvalidated()` hook for custom
  survival logic.
- **Nested analysis maps.** Separate caches for module-level, function-level,
  and per-operation analyses, enabling thread-safe function-level parallelism.
- **Instrumentation hooks.** `PassInstrumentation` provides
  `runBeforeAnalysis()` and `runAfterAnalysis()` callbacks for timing, tracing,
  and verification.

#### 2.2 CARTS vs MLIR Convention

| Feature | MLIR | CARTS | Gap |
|---------|------|-------|-----|
| Analysis caching | Per-operation, automatic | Per-module, manual | No preservation tracking |
| Analysis queries | `getAnalysis<T>()` via PassBase | `AM->getDbAnalysis()` | Passes need AM pointer injection |
| Preservation | TypeID-based PreservedAnalyses | None | All-or-nothing invalidation |
| Pipeline structure | Linear pass sequence + nesting | Custom 20-stage descriptors | Extra complexity, enables `--pipeline` |
| Pass initialization | `initialize(MLIRContext *)` hook | No explicit init hook | Unused MLIR lifecycle point |
| Instrumentation | PassInstrumentation | None | Cannot measure analysis time |
| Thread safety | Per-operation nesting | Monolithic (NOT safe) | Blocks parallelization |

#### 2.3 TextualPassPipeline and Sub-Pipeline Registration

MLIR supports registering named pass pipelines:

```cpp
registerPipeline("carts-db-optimization", [](OpPassManager &pm) {
  pm.addPass(createDbModeTighteningPass());
  pm.addPass(createDbScratchEliminationPass());
});
```

CARTS instead uses a custom `StageDescriptor` registry (`Compile.cpp:238-250`)
with explicit builder functions. Each descriptor carries:

- `captureDiagnosticsBefore` — triggers diagnostic snapshot before the stage
- `errorMessage` — custom message on stage failure
- `passes` — ordered list of pass names for `--all-pipelines` output
- `enabled` — predicate controlling whether the stage runs
- `allowStartFrom` — controls `--start-from` eligibility

The CARTS approach is more verbose but enables richer metadata (`--pipeline`,
`--start-from`, `--all-pipelines`). The trade-off is intentional but adds ~600
LOC of pipeline plumbing.

#### 2.4 Pass Statistics Integration

MLIR passes inherit `Pass::Statistic` from the LLVM `Statistic` infrastructure.
Declaration is one line per counter:

```cpp
Statistic numPartitioned{this, "num-partitioned", "DBs partitioned"};
```

CARTS has partial `Statistic` coverage: 4 of 30+ passes declare statistics
(27 total counters):

- `DbPartitioning.cpp` — 10 statistics (expanded acquires, stencil preservation, modes, failures)
- `CreateDbs.cpp` — 7 statistics (allocs created, skipped, inferred modes, groups, views)
- `EdtTransformsPass.cpp` — 6 statistics (granularity, affinity, reduction, deps, critical path)
- `DbTransformsPass.cpp` — 4 statistics (persistence, consolidation, cleanup, dead roots)

The remaining ~26 passes have no statistics.

### 3. IREE Best Practices

IREE is the most relevant production reference because it is also an MLIR-based
compiler targeting heterogeneous runtimes. Five practices stand out.

#### 3.1 Verification at Boundaries (HIGH priority)

IREE inserts lightweight verification passes between major phases. CARTS
currently has 6 verification points (`VerifyDbCreated`, `VerifyForLowered`,
`VerifyLowered`, `VerifyPreLowered`, `VerifyMetadata`, `VerifyMetadataIntegrity`).
Recommended additions:

- `VerifyEdtLowered` — after EDT lowering
- `VerifyDbLowered` — after DB lowering
- `VerifyEpochLowered` — after epoch lowering

#### 3.2 `--start-from` Pipeline Control (MEDIUM priority)

IREE supports resuming compilation from a named phase. CARTS already has both
`--pipeline` (stop-at) and `--start-from` (resume from a named stage token,
defined in `Compile.cpp:259-262`), with validation ensuring `startIndex <=
stopIndex` at lines 1159–1172. The remaining gap is not the CLI flag itself but
**AnalysisManager state persistence**: when resuming from a serialized `.mlir`
snapshot, all analysis caches (graphs, heuristic state) must be rebuilt from
scratch because there is no checkpoint/restore for AM state.

#### 3.3 `beforePhase` / `afterPhase` Hooks (MEDIUM priority)

IREE uses a `PipelineHooks` struct to inject diagnostics and validation at phase
boundaries without hardcoding them into pass code. CARTS currently hardcodes a
diagnostics check in the `PreLowering` phase.

#### 3.4 Function-Level Parallelism (HIGH priority, HIGH effort)

IREE enables multi-threading by default with a 12-thread pool and uses
pre-nested AnalysisManager instances for thread safety. CARTS has
`context.disableMultithreading(true)` at `Compile.cpp:334`. The primary blocker
is AnalysisManager thread safety (13 identified issues).

#### 3.5 Fixed-Point Canonicalize/CSE (LOW priority)

IREE replaces repeated canonicalize + CSE runs with a convergence loop. CARTS
currently runs these passes multiple times at fixed positions in the pipeline.

### 4. Feature Gap Matrix

Comparison of CARTS against five production compilers across key infrastructure
capabilities:

| Feature | CARTS | LLVM | GCC | IREE | Halide | TVM |
|---------|-------|------|-----|------|--------|-----|
| Heuristics framework | Static rules (H1, H2) | Cost-based | Heuristics + FDO | Cost-based | Multi-objective search | Learned predictor |
| Cost models | Hard-coded thresholds | Extensive | Partial | Partial | Explicit | XGBoost/RF |
| Pass statistics | **Done (19 passes, ~94 counters)** | Yes | Yes | Yes | N/A | Partial |
| Analysis dependency decls | **Partial (10 of 23 passes annotated)** | Yes | Yes | Yes | N/A | Partial |
| Profiling / feedback loop | **None** | PGO | FDO | No | Offline | Yes |
| Autotuning | **None** | No | No | No | Yes | Yes |
| Phase ordering semantics | **Partial (dependency DAG + validation)** | Strong | Strong | Strong | N/A | N/A |
| Pass parallelization | **Partially unblocked** (G-1 Phase 1a/1b/1d done) | Yes | Partial | Yes | N/A | N/A |
| Verification passes | **11 (creation + lowering boundaries)** | Continuous | Continuous | Continuous | N/A | Partial |
| IR-embedded decisions | Yes | Limited | Limited | Yes | Yes | Yes |
| Pipeline checkpoint/resume | `--pipeline` + `--start-from` (no AM persistence) | Full | Full | `--start-from` | N/A | N/A |
| Analysis invalidation | All-or-nothing | Selective | Selective | Selective | N/A | N/A |
| Compiler instrumentation | **`--diagnose` JSON + `--pass-timing` + `--mlir-timing`** | `--time-passes` | `-ftime-report` | `--mlir-timing` | Trace files | Logging |

### 5. Priority-Ranked Infrastructure Gaps

#### Tier 1 — Critical (correctness and parallelization blockers)

| ID | Gap | Impact | Effort | Files |
|----|-----|--------|--------|-------|
| G-1 | AnalysisManager thread safety | Blocks parallelization (5–15% speedup available). **Phase 1a done** (std::call_once for 8 lazy getters), **Phase 1b done** (shared_mutex on graph maps in DbAnalysis/EdtAnalysis), **Phase 1d done** (atomic flags/counters in DbGraph/EdtGraph/EdtAnalysis). Remaining: Phase 1c (clear-then-read), Phase 2 (eager init). | 1 week remaining | `AnalysisManager.h`, all 8 analysis classes |
| G-2 | Lowering contract authority completion | Graph-backed fallbacks still required; ~500 LOC cleanup blocked. **Phase 1 done**: thin contract-state API added (resolveEffectiveContract, combineContracts, patchContract, projectHaloWindow, hasCompleteHaloState). | 2 weeks remaining | `DbAnalysis.cpp`, `DbAcquireNode.cpp`, `PartitionBoundsAnalyzer.cpp`, `DbBlockInfoComputer.cpp` |

#### Tier 2 — High (optimization visibility and caching)

| ID | Gap | Impact | Effort | Files |
|----|-----|--------|--------|-------|
| G-3 | Pass statistics coverage | **Done**: 19 passes now have statistics (~94 counters). All major passes covered. | 0 | All covered |
| G-4 | Analysis dependency declarations | **Phase 1 done**: AnalysisDependencies.h header with AnalysisKind enum + AnalysisDependencyInfo struct. 10 passes annotated with declarative read/invalidate arrays (DbModeTightening, DbPartitioning, EdtStructuralOpt, EpochOpt, Concurrency, ConvertOpenMPToArts, CreateDbs, EdtDistribution, ForLowering, ForOpt). Remaining: annotate ~13 more passes, wire into AnalysisManager for selective invalidation. | 1 week remaining | All passes that use AM |
| G-5 | Phase ordering semantics | **Phase 1 done**: StageDescriptor now has `dependsOn` field with dependency arrays for all 20 stages. `validatePipelineDAG()` runs at pipeline construction time and asserts no forward dependencies. `--pipeline --json` now exports dependency info. Remaining: `--start-from` validation against DAG, extended verification barriers. | 2–3 days remaining | `Compile.cpp`, `StageDescriptor` |

#### Tier 3 — Medium (optimization quality and code health)

| ID | Gap | Impact | Effort | Files |
|----|-----|--------|--------|-------|
| G-6 | Heuristic parameterization | **Done**: `kMaxOuterDBs`, `kMaxDepsPerEDT`, `kMinInnerBytes` exposed as `--max-outer-dbs`, `--max-deps-per-edt`, `--min-inner-bytes` CLI options on `db-partitioning` pass | 0 | `DbHeuristics.h`, `Passes.td`, `DbPartitioning.cpp` |
| G-7 | Cost models for partitioning | Partitioning decisions based on access patterns only, no actual cost estimation | 2–3 weeks | `DbHeuristics.cpp`, `PartitioningHeuristics.h`, `DistributionHeuristics.h` |
| G-8 | Runtime function builder cleanup | `getOrCreateRuntimeFunction()` uses `func::lookupFnDecl()` + `func::createFnDecl()`. Upstream `func::lookupOrCreateFnDecl()` (`Utils.h:81`) **cannot be used**: it defaults null `resultType` to `i64` (`Utils.cpp:307`), breaking void-returning runtime functions. Current code correctly handles void returns via `isa<NoneType>(ReturnType) ? ArrayRef<Type>{} : ArrayRef<Type>{ReturnType}`. **Blocked** until upstream adds void-return support or a `FunctionType` overload. | N/A | `Codegen.cpp`, `Codegen.h`, `ConvertArtsToLLVM.cpp` |

#### Tier 4 — Low (instrumentation and future foundations)

| ID | Gap | Impact | Effort | Files |
|----|-----|--------|--------|-------|
| G-9 | `--mlir-timing` integration | **Resolved**: already functional via `applyDefaultTimingPassManagerCLOptions(pm)` in `configurePassManager()` (`Compile.cpp:576`) | 0 | `Compile.cpp` |
| G-10 | Pass instrumentation hooks | **Phase 1 done**: CartsPassInstrumentation class implemented with per-pass wall-clock timing. `--pass-timing` prints sorted report to stderr. `--pass-timing-output` exports JSON. Shared PassTimingData accumulates across pipeline stages. Remaining: analysis timing, IR change detection, op count tracking. | 1 week remaining | `Compile.cpp`, `PassInstrumentation.h/.cpp` |
| G-11 | Autotuning foundation | No parameter search space, no empirical search loop | 2–3 months | New infrastructure |
| G-12 | Verification at every boundary | **Done**: 11 verification passes now cover all creation and lowering boundaries. Added `VerifyEdtCreated` (after OpenMP conversion), `VerifyEpochCreated` (after epoch creation), plus existing `VerifyEdtLowered`, `VerifyDbLowered`, `VerifyEpochLowered`. | 0 | `Compile.cpp`, verification pass sources |

### 6. Attributor-Inspired Recommendations

These are concrete patterns to adopt from the Attributor's *discipline* without
importing its framework.

#### 6.1 State Lattices for DbAnalysis — DONE

**Implemented**: `DbPartitionState` header at
`include/arts/analysis/db/DbPartitionState.h` with:

- `FactProvenance` enum: `IRContract`, `GraphInferred`, `HeuristicAssumption`
- `ProvenancedFact<T>` template with `isKnown()`, `isAssumed()`, `strengthen()`
- `DbPartitionState` struct with provenanced fields for mode, ownerDims,
  partitionDims, haloWindow, blockShape, replicationFactor
- Monotone `meet()` operation: higher-provenance facts dominate lower ones
- `isComplete()` check for all required fields

This formalizes the Known/Assumed lattice for partition decisions. The
`PartitioningContext` can now track provenance of each fact explicitly.

Primary file: `include/arts/analysis/db/DbPartitionState.h`.

#### 6.2 Dependency-Aware Analysis Queries

Adopt the `getAAFor()` pattern at the AnalysisManager level:

```cpp
template <typename AnalysisT>
AnalysisT &getAnalysisFor(Pass *queryingPass,
                          AnalysisDependency dep = OPTIONAL);
```

When a pass queries an analysis, the AnalysisManager records the dependency
edge. On invalidation, only dependent analyses are cleared. This replaces the
current all-or-nothing `AM->invalidate()`.

Primary file: `include/arts/analysis/AnalysisManager.h`.

#### 6.3 InformationCache for Module-Level Facts — DONE

**Implemented**: `ArtsInformationCache` class at
`include/arts/analysis/InformationCache.h` +
`lib/arts/analysis/InformationCache.cpp` with:

- Single walk using `dyn_cast` dispatch for EdtOp, ForOp, DbAllocOp,
  DbAcquireOp
- Per-function caches: `edtsPerFunction`, `loopsPerFunction`,
  `allocsPerFunction`, `acquiresPerFunction`
- `ArrayRef<T>` getters for zero-copy access
- `invalidate()` method to clear all caches on module mutation
- Built from `ModuleOp` in constructor

Primary files: `include/arts/analysis/InformationCache.h`,
`lib/arts/analysis/InformationCache.cpp`.

#### 6.4 Graph Versioning — DONE

**Implemented**: `uint64_t version` counter added to both `DbGraph` and
`EdtGraph`. Incremented on `build()`, `buildNodesOnly()`, and `invalidate()`.
Public `getVersion()` getter available for staleness detection.

Primary files: `include/arts/analysis/graphs/db/DbGraph.h`,
`include/arts/analysis/graphs/edt/EdtGraph.h`.

#### 6.5 PreservedAnalyses Integration

Move from all-or-nothing invalidation to selective preservation. Each pass
declares which analyses it preserves:

```cpp
void DbModeTighteningPass::runOnOperation() {
  // ... pass logic ...
  markAnalysesPreserved<LoopAnalysis>();  // Loop structure unchanged
}
```

The AnalysisManager checks the preservation set and only destroys unpreserved
caches. This is the MLIR-native pattern and complements the dependency tracking
from Section 6.2.

Primary files: `include/arts/analysis/AnalysisManager.h`, all passes in
`lib/arts/passes/`.

#### 6.6 Change Reporting for Monotone Updates — DONE

**Implemented**: `enum class ContractChange { Unchanged, Changed }` added.
`mergeLoweringContractInfo` now returns `ContractChange` instead of `void`.
Each conditional assignment branch sets `changed = true` when the value
actually changes. Source-compatible (callers that discard the return are
unaffected).

Primary file: `lib/arts/utils/LoweringContractUtils.cpp`.

### 7. Recommended Implementation Sequence

When infrastructure work resumes, the recommended order integrates these new
findings with the existing contract-authority work from the first half of this
report:

1. **Contract authority** (Tier 1, G-2): finish the thin contract-state API
   and eliminate graph-backed fallbacks. This is already the top priority from
   the existing gap analysis.

2. **Pass statistics coverage** (Tier 2, G-3): extend `Statistic` coverage
   from the existing 4 passes (27 counters) to the remaining ~26 uncovered
   passes. Low risk, immediate visibility into optimization impact.

3. **Analysis dependency declarations** (Tier 2, G-4): implement dependency-
   aware queries in AnalysisManager. Prerequisite for selective invalidation.

4. **AnalysisManager thread safety** (Tier 1, G-1): harden for multi-threaded
   execution. Requires dependency declarations (step 3) to be in place first
   for safe selective invalidation.

5. **Graph versioning** (Section 6.4): add modification counters to DbGraph
   and EdtGraph. Quick win that eliminates stale-data bugs.

6. **InformationCache** (Section 6.3): pre-scan module to eliminate redundant
   walks. Medium effort, measurable compile-time improvement.

7. **Runtime function builder cleanup** (Tier 3, G-8): **blocked** — upstream
   `func::lookupOrCreateFnDecl()` defaults null `resultType` to `i64`, breaking
   void-returning ARTS runtime functions. Current `lookupFnDecl()` +
   `createFnDecl()` pattern is correct. Revisit if upstream adds a
   `FunctionType` overload or void-return support.

8. **Heuristic parameterization** (Tier 3, G-6): expose existing named
   constants (`kMaxOuterDBs`, `kMaxDepsPerEDT`, `kMinInnerBytes`) as CLI-tunable
   pass options. Foundation for future autotuning.

9. **Verification passes** (Tier 4, G-12): add `VerifyEdtLowered`,
   `VerifyDbLowered`, `VerifyEpochLowered` at lowering boundaries.

10. **`--mlir-timing`** (Tier 4, G-9): **Resolved** — already functional via
    `applyDefaultTimingPassManagerCLOptions(pm)` in `configurePassManager()`
    (`Compile.cpp:576`). No further work needed.

## Investigation Reports (2026-04-02)

Comprehensive investigation completed for all remaining infrastructure gaps
using 10-agent parallel investigation. Reports are located at:

### Implementation Deliverables (merged to main)

- **G-3 Pass statistics**: 19 passes now have ~94 total counters. All major
  passes covered including `EdtStructuralOpt.cpp` (7) and
  `ConvertArtsToLLVM.cpp` (5).
- **Phase 1 upstream reuse**: `computeTotalElements` simplified to direct
  fold-multiply. `hasNonPartitionableHostViewUses` documented as non-simplifiable.
- **Phase 2 ValueAnalysis**: `isConstantAtLeastOne` simplified (removed
  redundant check). `areValuesEquivalent` and `isProvablyNonZero` documented
  as correctly non-simplifiable.

### Investigation Deliverables (design documents)

- **G-1 AnalysisManager thread safety**:
  `docs/audits/2026-04-02-am-thread-safety-audit.md`,
  `docs/audits/2026-04-02-am-implementation-roadmap.md`
  — 10 race conditions (4 critical, 4 high, 2 medium), 5-phase fix roadmap.
  Phase 0 can parallelize 6 passes immediately for 3-5x speedup.

- **G-2 Contract-state API**:
  `.investigation/CONTRACT_STATE_API_DESIGN.md`
  — 5-function API (resolve, combine, patch, projectAcquireRewrite,
  projectHaloWindow). Only 1 graph fallback safely removable (refineContract
  WithStencilBounds). Research Tasks 1-3 all answered.

- **G-4 + 6.2 + 6.5 Analysis dependencies**:
  `docs/audits/2026-04-02-analysis-dependency-investigation.md`
  — 54-pass dependency matrix. 23 passes use analyses, 13 call blanket
  invalidate. 7-phase migration strategy (25-35 days total). 6 passes have
  unused AM parameters (code smell).

- **G-5 Phase ordering**:
  `docs/compiler/phase-ordering-semantics.md`,
  `docs/plans/phase-ordering-design.md`
  — All 20 stages mapped with pre/postconditions. 4 hard preconditions, 8
  analysis-dependency constraints, 8 verification barriers. 4 missing
  verification passes identified. Recommended: dependency DAG + extended
  verification barriers.

- **G-7 + 6.1 + 6.3 Cost models and caching**:
  `docs/analysis/g7-investigation-report.md`
  — 218 redundant module walks identified; InformationCache could reduce 36%
  (15-25% compile-time improvement). Cost model formula designed. State
  lattice exists implicitly in DbAnalysis but needs explicit Known/Assumed
  labeling.

- **G-10 Pass instrumentation**:
  `docs/compiler/pass-instrumentation-investigation.md`
  — CartsPassInstrumentation design using MLIR's native PassInstrumentation
  API. 6 features (per-pass timing, IR change detection, statistic
  aggregation, analysis timing, op count tracking, JSON export). 4-7 days
  estimated implementation.

- **Phase 7 Memref-descriptor audit**:
  No refactoring needed. 14 descriptor construction sites audited. All memref
  construction properly delegates through Polygeist's Pointer2MemrefOp.
  ArtsHintBuilder is a 2-field custom struct, not a memref descriptor.

### Implementation Deliverables — Round 2 (2026-04-02 PM)

10-agent parallel implementation of investigation recommendations:

- **G-1 Phase 1a** (committed `e261b3b9`→ merged into AM changes): `std::call_once`
  for all 8 AnalysisManager lazy getters.
- **G-1 Phase 1b** (committed `e50c7dbc`): `std::shared_mutex` added to
  DbAnalysis::functionGraphMap and EdtAnalysis::edtGraphs. getOrCreateGraph/
  invalidateGraph wrapped with unique_lock.
- **G-1 Phase 1d** (committed `d849cd12`): `std::atomic<bool>` for built/needsRebuild/
  analyzed flags; `std::atomic<uint64_t>` for version counters. Memory ordering:
  acquire/release for bools, relaxed for counters.
- **G-2 Phase 1** (committed `2df7de11`): 5 contract-state API functions in
  LoweringContractUtils: resolveEffectiveContract, combineContracts, patchContract,
  projectHaloWindow, hasCompleteHaloState.
- **G-4 Phase 1** (committed `a2ab162a`): AnalysisDependencies.h with AnalysisKind
  enum + AnalysisDependencyInfo struct. 10 passes annotated with reads/invalidates
  arrays.
- **G-5 Phase 1** (committed `7d2bddb3`): StageDescriptor::dependsOn field with
  dependency arrays for all 20 stages. validatePipelineDAG() assertion.
  JSON pipeline manifest now exports dependency info.
- **G-10 Phase 1** (committed `7d2bddb3`): CartsPassInstrumentation class with
  per-pass wall-clock timing. --pass-timing and --pass-timing-output CLI flags.
  PassTimingData shared across pipeline stages.
- **G-12 additions** (committed `a2ab162a`): VerifyEdtCreated (after openmp-to-arts)
  and VerifyEpochCreated (after create-epochs) verification passes.
- **Section 6.1** (committed `2c243cd1`): DbPartitionState with FactProvenance enum
  and ProvenancedFact<T> template for Known/Assumed provenance tracking.
- **Section 6.3** (committed `3ffcb4f0`): ArtsInformationCache with single-walk
  module pre-scan caching EdtOp/ForOp/DbAllocOp/DbAcquireOp per function.
