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

Status: partial

Already landed:

- `Codegen.cpp` already uses `affine::linearizeIndex`.
- `Codegen.cpp` already uses `memref::computeStridesIRBlock`.
- `DbUtils.cpp` already uses `computeSuffixProduct`.
- `ValueAnalysis::stripMemrefViewOps()` already uses
  `memref::skipFullyAliasingOperations`.

Still missing:

- `ArtsCodegen::computeTotalElements()` still reconstructs total size manually
  from `computeStridesFromSizes()` instead of collapsing onto one upstream
  helper path.
- `DbUtils::hasNonPartitionableHostViewUses()` still carries a custom host-use
  traversal and has not been reduced to a thinner upstream-based view-chain
  walk.

Primary files:

- `lib/arts/codegen/Codegen.cpp`
- `lib/arts/utils/DbUtils.cpp`
- `lib/arts/analysis/value/ValueAnalysis.cpp`

### Phase 2: ValueAnalysis Simplification

Status: partial

Already landed:

- `ValueAnalysis.cpp` already uses `ValueBoundsConstraintSet::areEqual`.
- `ValueAnalysis.cpp` already uses `computeConstantBound()` for lower and upper
  bounds.

Still missing:

- `areValuesEquivalent()` still has custom recursive SSA reasoning on top of
  the bounds path.
- `isConstantAtLeastOne()` and `isProvablyNonZero()` still carry bespoke proof
  logic instead of delegating more aggressively to the bounds layer.

Primary files:

- `lib/arts/analysis/value/ValueAnalysis.cpp`
- `include/arts/analysis/value/ValueAnalysis.h`

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

Status: mostly missing

Still missing:

- `ArtsCodegen::getOrCreateRuntimeFunction()` is still a custom cache instead
  of using `LLVM::lookupOrCreateFn()`.
- memref-descriptor construction still needs a focused audit against upstream
  builders before it can be called simplified.

Primary files:

- `lib/arts/codegen/Codegen.cpp`
- `include/arts/codegen/Codegen.h`
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`

### Phase 8: Pass Statistics

Status: missing

Current state:

- there are no `Statistic<>` or `STATISTIC(...)` declarations in the compiler
  pass tree.

Primary files:

- `include/arts/passes/Passes.td`
- `lib/arts/passes/opt/db/DbPartitioning.cpp`
- `lib/arts/passes/opt/db/DbTransformsPass.cpp`
- `lib/arts/passes/opt/edt/EdtTransformsPass.cpp`
- other high-value optimization passes

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
