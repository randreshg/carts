# RFC: Structured Kernel Plans, Split Launch State, and Proof-Driven Ownership

## 1. Why This Exists

CARTS already has enough information to do better on regular kernels such as:

- `jacobi2d`
- `seidel-2d`
- `convolution-2d`
- `rhs4sg`
- `vel4sg`
- `specfem3d`

The current problem is not lack of local optimizations. The problem is that the
compiler rediscovers the same facts multiple times across unrelated passes:

- loop structure and repetition in `EpochOpt`
- async loop ABI shape in `EpochLowering`
- ownership and worker-local legality in `DbAnalysis` / `DbPartitioning`
- distribution choice in `DistributionHeuristics`
- late pointer and halo cleanup in EDT/DB/codegen lowering

That fragmentation forces fallback logic, ABI recovery, and mixed heuristics.
The result is correct often enough, but too brittle and too conservative to
consistently make all structured benchmarks shine.

This RFC proposes one production-oriented architecture to fix that.

## 2. Goals

1. Make structured loop kernels lower from one verified execution plan instead
   of multiple loosely synchronized heuristics.
2. Remove CPS relaunch ABI recovery from positional pack reconstruction.
3. Make block/coarse/stencil ownership decisions proof-driven and verifiable.
4. Enable persistent structured execution for timestep and neighborhood kernels
   without benchmark-specific hardcoding.
5. Preserve safe generic fallbacks for irregular or poorly understood kernels.

## 3. Non-Goals

1. Do not add benchmark-specific thresholds or special-case constants.
2. Do not replace the whole ARTS lowering stack with an unrelated async stack.
3. Do not force persistent execution on kernels that fail the proof or cost
   gates.

## 4. Current Structural Problems

### 4.1 CPS launch ABI is inferred too late

Today `EpochLowering` reconstructs relaunch state by tracing packed values and
permutation attrs instead of consuming a first-class launch contract.

Symptoms:

- `arts.cps_param_perm`
- `arts.cps_dep_routing`
- `arts.cps_forward_deps`
- `arts.cps_preserve_carry_abi`

This is workable, but not production-clean. Relaunch correctness should not
depend on pack archaeology.

### 4.2 Ownership legality is advisory, not proven

`LoweringContractOp` is structurally validated today, but not semantically
validated against worker-local reachability.

That means the compiler can still arrive at the right answer for block/coarse
and stencil ownership, but the proof is distributed across:

- `DbAnalysis`
- `DbPartitioning`
- `ForLowering`
- late DB/EDT lowering

### 4.3 Structured kernels do not have one stable execution plan

The same kernel may be analyzed as:

- an async loop in one pass
- a stencil in another
- a block-local slice in another
- a cleanup/hoisting opportunity in another

Those are all views of the same underlying schedule, but there is no
first-class object that owns that schedule.

## 5. Proposed Architecture

The proposal has four pillars.

### 5.1 Structured Kernel Plan

Add a first-class compiler analysis/result for regular structured kernels:
`StructuredKernelPlan`.

It should be produced after `concurrency` and before distribution /
partitioning.

The plan records:

- kernel family
  - uniform
  - stencil
  - wavefront
  - reduction-mixed
  - timestep chain
- owner dims
- physical block shape
- logical worker slice shape
- halo shape
- iteration topology
  - serial
  - owner-strip
  - owner-tile
  - wavefront
  - persistent strip/tile
- repetition structure
  - repeated trip product
  - pair-step or k-step grouping
- legal async strategy
  - blocking
  - `advance_edt`
  - `cps_chain`
  - persistent structured region
- cost signals
  - scheduler overhead estimate
  - slice widening pressure
  - expected local work per launch
  - relaunch amortization benefit

This is not a runtime object. It is the compiler’s single source of truth for
structured scheduling.

### 5.2 Split Launch State Into Two Channels

Make launch state first-class instead of packing everything into one positional
array.

Two channels:

1. `state`
   - loop counters
   - bounds
   - outer epoch GUID
   - scalar captures
   - other true param-routed values
2. `deps`
   - DB families
   - forwarded dep slots
   - slice offsets/sizes
   - partition metadata
   - dependency mode/flags

New IR concepts:

- `arts.state_pack`
- `arts.state_unpack`
- `arts.dep.bind`
- `arts.dep.forward`
- `arts.edt_launch`

Stage 1 can still lower these to the current runtime ABI. The point is to make
the compiler’s internal contract explicit and deterministic.

### 5.3 Proof-Driven Ownership Contracts

Upgrade `LoweringContractOp` from “metadata + hints” to a proof-carrying
contract for structured ownership.

The contract should answer:

1. Which physical dims are owned?
2. Which worker-local slice is legal?
3. Which halo widening is legal?
4. Whether a read-only input may stay block-local or must widen to full-range.
5. Whether dependency-local narrowing is sound for the exact acquire.

New proof surface:

- owner-dim reachability
- partition-to-access mapping
- halo legality
- dependence slice soundness
- relaunch-state soundness for persistent regions

`ContractValidation` should fail or warn on proof inconsistencies, not only on
missing attrs.

### 5.4 Persistent Structured Regions

For proven regular timestep or neighborhood kernels, allow the compiler to
lower a region into a long-lived owner-strip or owner-tile execution shape.

This is the high-upside path for:

- `jacobi2d`
- `seidel-2d`
- some convolution variants

The persistent region model:

- one launch owns a stable local slice
- halo/dataflow is refreshed explicitly
- multiple logical steps run before teardown when legal
- relaunch uses structured state/deps, not generic pack recovery

This is optional and gated by proof + cost model.

## 6. Expected Benchmark Impact

### 6.1 `jacobi2d`

Main wins:

- less epoch / relaunch overhead
- stable owner-local strip plan
- explicit halo transport
- removal of generic per-access buffer/dep reconstruction

### 6.2 `seidel-2d`

Main wins:

- wavefront plan becomes first-class
- `advance_edt` / persistent region can be selected from one plan
- split-phase halo transport can be emitted from the contract, not rediscovered

### 6.3 `convolution-2d`

Main wins:

- neighborhood-window execution becomes explicit
- block-local read-only input legality can be decided from proof, not by
  emergent heuristic interaction
- avoids both over-widening and over-conservative fallback

### 6.4 `rhs4sg`, `vel4sg`, `specfem3d`

Main wins:

- block/stencil ownership stays stable across passes
- dep routing and halo legality are explicit
- fewer late pointer/neighbor reconstruction passes in hot regions

## 7. Proposed Phases

### Phase 0: Documentation and Contract Audit

1. Document the current CPS and ownership failure surfaces.
2. Add verifier-level visibility for ownership proof gaps and pack-schema gaps.
3. Freeze naming and attribute conventions for the new plan/contract surface.

### Phase 1: Structured Kernel Plan Analysis

1. Add `StructuredKernelPlanAnalysis`.
2. Produce plans only for regular affine / structured kernels.
3. Thread the plan into:
   - `DistributionHeuristics`
   - `EdtDistribution`
   - `DbPartitioning`
   - `ForLowering`

Fallback:

- if no plan is proven, use the current generic path unchanged.

### Phase 2: Split Launch State IR

1. Introduce `arts.state_pack` / `arts.state_unpack`.
2. Introduce `arts.dep.bind` / `arts.dep.forward`.
3. Lower existing `EdtCreateOp` construction through the split contract.
4. Keep runtime ABI compatible at first.

### Phase 3: CPS Schema Cleanup

1. Stop reconstructing carry ABI from positional pack traces.
2. Replace attr-driven relaunch inference with explicit state/deps schema.
3. Shrink or remove:
   - `arts.cps_param_perm`
   - `arts.cps_dep_routing`
   - `arts.cps_forward_deps`
   - `arts.cps_preserve_carry_abi`

### Phase 4: Proof-Driven Ownership

1. Extend `LoweringContractOp` with proof-derived ownership fields.
2. Make `ContractValidation` check semantic soundness:
   - owner-dim reachability
   - slice legality
   - halo legality
3. Make DB/EDT lowering consume the proof directly.

### Phase 5: Persistent Structured Regions

1. Add persistent structured lowering for proven timestep / neighborhood
   kernels.
2. Gate it conservatively using the plan’s cost model.
3. Start with:
   - `jacobi2d`
   - `seidel-2d`
   - selected structured convolution paths

## 8. Cost Model Direction

This design should not use benchmark-specific constants. The cost model should
be based on normalized properties:

- launch count per unit of useful work
- repeated trip product
- worker-local arithmetic intensity
- dependency slice widening ratio
- expected halo traffic
- expected DB family count
- persistence benefit vs launch overhead

Decision examples:

- prefer persistent structured regions when launch overhead dominates and state
  is stable
- prefer block-local reads when proof says owner-local reachability is exact
- prefer coarse when proof says narrowing would force full-range behavior anyway

## 9. Validation Plan

### Compiler Validation

1. new contract tests for:
   - state/deps schema correctness
   - ownership proof correctness
   - persistent-region legality
2. verifier tests for invalid proof states
3. pass-boundary tests for plan preservation

### Performance Validation

Required benchmark sets:

- large / 64-thread full suite
- targeted stencil/timestep kernels
- regression-sensitive non-stencil kernels

Acceptance criteria:

1. no benchmark-specific hardcoded decisions
2. no correctness regression
3. no broad suite slowdown
4. clear wins on structured timestep / neighborhood kernels

## 10. Initial Workstreams

### Workstream A: Structured Kernel Plan

Files likely involved:

- `lib/arts/dialect/core/Analysis/heuristics/DistributionHeuristics.cpp`
- `lib/arts/passes/transforms/EdtDistribution.cpp`
- `lib/arts/passes/transforms/ForLowering.cpp`
- `tools/compile/Compile.cpp`

### Workstream B: Split Launch State IR

Files likely involved:

- `include/arts/Ops.td`
- `lib/arts/passes/transforms/EdtLowering.cpp`
- `lib/arts/passes/transforms/EpochLowering.cpp`
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`

### Workstream C: Ownership Proof

Files likely involved:

- `lib/arts/dialect/core/Analysis/db/DbAnalysis.cpp`
- `lib/arts/passes/opt/db/DbPartitioning.cpp`
- `lib/arts/passes/opt/db/ContractValidation.cpp`
- `lib/arts/utils/DbUtils.cpp`

### Workstream D: Persistent Structured Regions

Files likely involved:

- `lib/arts/passes/opt/edt/EpochOpt.cpp`
- `lib/arts/dialect/core/Analysis/heuristics/EpochHeuristics.cpp`
- `lib/arts/passes/transforms/EpochLowering.cpp`

### Workstream E: Benchmark / Contract Gate

Files likely involved:

- `tests/contracts/`
- `docs/compiler/`
- benchmark runner policy / regression docs

Primary deliverables:

1. a dedicated structured-kernel contract matrix
2. benchmark experiment subsets for structured winners and non-structured
   regressions
3. perf-gate policies that distinguish required gates from advisory tracking

#### E.1 Contract gate layout

The contract gate should be split into required classes so each new
architecture surface has a direct regression owner.

1. `structured_plan_*`
   - plan creation on regular kernels
   - plan rejection on irregular kernels
   - pass-boundary preservation from plan creation through pre-lowering
2. `state_dep_schema_*`
   - state/deps split remains stable through CPS and non-CPS lowering
   - launch-state ordering does not depend on positional pack archaeology
   - forwarded dep slots remain aligned with relaunch ABI
3. `ownership_proof_*`
   - owner dims, worker-local slices, halo legality, and widening decisions
   - verifier rejects inconsistent proof states
   - proof-preserving passes cannot silently drop required facts
4. `persistent_region_*`
   - persistent structured region legality
   - explicit fallback when proof or cost gates fail
   - no persistence for nested / irregular / unsafe loop shapes
5. `structured_fallback_*`
   - same input shape compiles through the generic path when the structured
     path is disabled or rejected
   - ensures new fast paths never become correctness-critical

Initial benchmark-shaped contract coverage should include:

- `jacobi2d`-like timestep strip stencil
- `seidel-2d`-like wavefront / split-phase halo case
- `convolution-2d`-like neighborhood window
- `rhs4sg` / `vel4sg`-like blocked neighborhood ownership
- one deliberately irregular negative case that must not produce a structured
  plan

#### E.2 New contract tests to add

Minimum new required tests:

1. plan synthesis / rejection
   - structured plan emitted for regular stencil/timestep kernel
   - structured plan rejected for mixed-indirect or non-affine kernel
2. plan preservation
   - plan survives `edt-distribution`
   - plan survives `db-partitioning`
   - plan survives `post-db-refinement`
3. split launch schema
   - CPS relaunch consumes explicit state/deps schema
   - non-CPS structured launch uses the same schema surface
   - verifier rejects state/deps width mismatch
4. ownership proof
   - verifier rejects impossible owner-dim / halo combination
   - verifier rejects block-local read when proof requires full-range
   - verifier accepts exact owner-local reachability without widening
5. persistent-region legality
   - legal positive case
   - proof failure fallback case
   - cost-gate fallback case
6. regression guards
   - disabling structured-kernel mode preserves old generic lowering
   - structured path does not change non-structured kernels' key contracts

The rule for new fast paths should be: every positive structured-path test must
have a paired fallback or reject test using the same family of IR shape.

#### E.3 Benchmark gate tiers

Use existing benchmark-runner experiments and perf-gate policy machinery rather
than a separate gating stack.

Tier 0: required correctness / compile gate

- run focused `tests/contracts/` lit coverage for all new plan/schema/proof
  tests
- no performance assertions here, only semantic and verifier stability

Tier 1: required structured-kernel performance gate

- add one experiment config for a structured stable subset
- target kernels:
  - `polybench/jacobi2d`
  - `polybench/seidel-2d`
  - `polybench/convolution-2d`
  - `sw4lite/rhs4sg-base`
  - `sw4lite/vel4sg-base`
  - `specfem3d/velocity` and/or `specfem3d/stress` as advisory until stable
- default size: `large`
- default single-node thread profile: local full-thread point plus a short
  sweep (`1, full`)

Tier 2: required non-structured regression gate

- add one experiment config for benchmarks that should not depend on the new
  architecture but are sensitive to runtime overhead changes
- target kernels:
  - `polybench/gemm`
  - `polybench/2mm`
  - `polybench/3mm`
  - `polybench/atax`
  - `polybench/bicg`
  - `ml-kernels/layernorm`
  - `ml-kernels/activations`
  - `stream`
- purpose:
  - catch scheduler / launch overhead regressions
  - catch accidental structured-path activation where it should not occur

Tier 3: advisory broad-suite gate

- full single-node large suite
- multinode structured subset once structured ownership is stable
- this should inform rollout decisions but should not block early architectural
  landing until the stable subsets are trustworthy

#### E.4 Perf-gate policy split

Add separate policy files instead of one monolithic threshold list:

1. `structured-kernel-required.json`
   - required entries for the Tier 1 subset
   - benchmark-specific baselines are allowed only as measurement baselines,
     not as compile-time decision thresholds
2. `nonstructured-regression-required.json`
   - required entries for Tier 2
   - wider tolerances, focused on "do not get worse"
3. `structured-kernel-advisory.json`
   - larger and/or noisier cases tracked for trend visibility

Policy semantics should stay aligned with the current perf-gate model:

- require success
- require correctness
- cap startup outliers
- use `baseline_speedup` plus tolerance or direct `min_speedup`
- allow `required: false` only in advisory policies

#### E.5 Counter-based regression checks

Time-only gates are not enough for this architecture. Structured-kernel
improvements should also be validated against runtime-shape counters already
reported by the benchmark tooling.

Required tracked metrics:

- `num_edts_created`
- `num_edts_finished`
- `num_dbs_created`
- task throughput / average task time derived by the report tooling

Add advisory counter expectations for structured winners:

1. no increase in `num_edts_created` unless time improves materially
2. no increase in `num_dbs_created` from proof-driven ownership unless time or
   locality benefit justifies it
3. persistent-region mode should show a clear drop in EDT creation rate versus
   the generic relaunch path

These should start as report-only assertions in policy documentation, then move
to hard gates once variance is understood.

#### E.6 Regression-prevention rules

1. Every structured optimization remains proof-gated and cost-gated.
2. Generic fallback must remain compilable and tested for every kernel family.
3. No benchmark may require a benchmark-specific compile-time special case to
   pass the gate.
4. New structured-path defaults should roll out in stages:
   - verifier + contracts first
   - advisory benchmark gate second
   - required structured subset third
   - broad default enablement last
5. If a kernel wins only by inflating DB or EDT counts dramatically, treat that
   as a suspicious optimization until the cost model explains it.

#### E.7 Suggested initial commands and CI shape

Contract gate:

```bash
carts lit tests/contracts/
```

Structured subset benchmark gate:

```bash
carts benchmarks run --experiment <structured-subset-config>
carts benchmarks perf-gate <results-dir-or-json> --policy <structured-policy>
```

Non-structured regression gate:

```bash
carts benchmarks run --experiment <nonstructured-regression-config>
carts benchmarks perf-gate <results-dir-or-json> --policy <regression-policy>
```

Recommended rollout order for Workstream E:

1. land contract-test matrix names and paired fallback tests
2. land structured and non-structured subset experiment configs
3. land advisory perf-gate policies
4. promote stable entries to required gates only after variance is measured

## 11. Recommended Execution Order

1. land the RFC and verifier expansion
2. land `StructuredKernelPlanAnalysis`
3. land split launch-state IR without changing runtime ABI
4. convert CPS relaunch to the explicit schema
5. land proof-driven ownership validation
6. enable persistent structured regions for proven kernels

That sequence keeps each phase production-safe and testable in isolation.
