# Structured Kernel Contract Test Matrix

This document defines the contract test plan for the structured kernel
architecture described in the
[Structured Kernel Plan RFC](structured-kernel-plan-rfc.md). Every new
architecture surface (plan, schema, proof, persistent region) gets dedicated
regression coverage so that each can be validated and evolved independently.

## 1. Test Classes

Five test classes cover the structured kernel architecture. Each class owns a
distinct correctness surface.

### 1.1 `structured_plan_*` -- Plan Creation, Rejection, and Preservation

Tests that a `StructuredKernelPlan` is produced for regular kernels, rejected
for irregular kernels, and preserved across pass boundaries.

| Concern | What to verify |
|---------|----------------|
| Plan synthesis | Plan emitted for regular stencil, timestep, uniform, and neighborhood kernels |
| Plan rejection | Plan NOT emitted for mixed-indirect, non-affine, or irregular kernels |
| Pass-boundary preservation | Plan survives `edt-distribution`, `db-partitioning`, and `post-db-refinement` without silent mutation |
| Kernel family classification | Correct family tag (uniform, stencil, wavefront, reduction-mixed, timestep chain) |

### 1.2 `state_dep_schema_*` -- Split Launch State Stability

Tests that the state/deps split is correct and stable through both CPS and
non-CPS lowering paths.

| Concern | What to verify |
|---------|----------------|
| State/deps split | `arts.state_pack` and `arts.dep.bind` carry the correct values |
| Launch-state ordering | State ordering is deterministic and does not depend on positional pack archaeology |
| Forwarded dep slots | Forwarded dep slot indices remain aligned with the relaunch ABI |
| CPS consumption | CPS relaunch consumes the explicit state/deps schema |
| Non-CPS consumption | Non-CPS structured launch uses the same schema surface |
| Width mismatch rejection | Verifier rejects state/deps width mismatch |

### 1.3 `ownership_proof_*` -- Proof-Driven Ownership Contracts

Tests that ownership proofs are correct, verifiable, and consumed by downstream
passes.

| Concern | What to verify |
|---------|----------------|
| Owner dims | Correct owner-dim assignment for block, stencil, and neighborhood patterns |
| Worker-local slices | Legal worker-local slice shape matches the plan |
| Halo legality | Halo widening is accepted when legal, rejected when unsound |
| Widening decisions | Block-local read stays block-local when proof says owner-local reachability is exact |
| Full-range fallback | Verifier rejects block-local read when proof requires full-range |
| Proof preservation | Proof-preserving passes cannot silently drop required ownership facts |
| Inconsistent proof rejection | Verifier rejects impossible owner-dim / halo combinations |

### 1.4 `persistent_region_*` -- Persistent Structured Region Legality

Tests that persistent structured regions are emitted only when both the proof
and cost gates pass, and that fallback is explicit otherwise.

| Concern | What to verify |
|---------|----------------|
| Legal positive | Persistent region emitted for proven timestep/neighborhood kernel |
| Proof failure fallback | Explicit fallback to non-persistent lowering when proof fails |
| Cost-gate fallback | Explicit fallback when proof passes but cost model rejects persistence |
| Irregular rejection | No persistent region for nested, irregular, or unsafe loop shapes |
| Stable slice ownership | Persistent launch owns a stable local slice across logical steps |
| Halo refresh | Halo/dataflow refresh is explicit within the persistent region |

### 1.5 `structured_fallback_*` -- Generic Path Regression Guards

Tests that the generic lowering path remains compilable and correct when the
structured path is disabled or rejected. New fast paths must never become
correctness-critical.

| Concern | What to verify |
|---------|----------------|
| Disabled mode | Same input compiles through generic path when structured mode is disabled |
| Rejection fallback | Same input compiles through generic path when structured plan is rejected |
| Non-structured stability | Structured path does not change non-structured kernels' key contracts |
| No silent activation | Structured path does not activate for kernels that should remain generic |

## 2. Required Coverage Per Class

These are the minimum required tests from RFC section E.2, mapped to the five
classes.

### 2.1 Plan Synthesis / Rejection (`structured_plan_*`)

- Structured plan emitted for regular stencil/timestep kernel.
- Structured plan rejected for mixed-indirect or non-affine kernel.

### 2.2 Plan Preservation (`structured_plan_*`)

- Plan survives `edt-distribution`.
- Plan survives `db-partitioning`.
- Plan survives `post-db-refinement`.

### 2.3 Split Launch Schema (`state_dep_schema_*`)

- CPS relaunch consumes explicit state/deps schema.
- Non-CPS structured launch uses the same schema surface.
- Verifier rejects state/deps width mismatch.

### 2.4 Ownership Proof (`ownership_proof_*`)

- Verifier rejects impossible owner-dim / halo combination.
- Verifier rejects block-local read when proof requires full-range.
- Verifier accepts exact owner-local reachability without widening.

### 2.5 Persistent-Region Legality (`persistent_region_*`)

- Legal positive case.
- Proof failure fallback case.
- Cost-gate fallback case.

### 2.6 Regression Guards (`structured_fallback_*`)

- Disabling structured-kernel mode preserves old generic lowering.
- Structured path does not change non-structured kernels' key contracts.

## 3. Naming Conventions

### File Naming

All test files live in `tests/contracts/` and follow the pattern:

```
<test_class>_<descriptive_behavior>.mlir
```

Examples:

```
structured_plan_stencil_timestep_synthesis.mlir
structured_plan_irregular_indirect_rejection.mlir
structured_plan_survives_edt_distribution.mlir
state_dep_schema_cps_explicit_consumption.mlir
state_dep_schema_width_mismatch_verifier_reject.mlir
ownership_proof_owner_dim_halo_reject.mlir
ownership_proof_exact_owner_local_no_widening.mlir
persistent_region_timestep_legal_positive.mlir
persistent_region_proof_failure_fallback.mlir
structured_fallback_disabled_mode_generic_path.mlir
structured_fallback_nonstructured_stability.mlir
```

### CHECK Pattern Conventions

- Use `CHECK:` for required output lines.
- Use `CHECK-NOT:` to assert absence (e.g., no structured plan for irregular
  kernels, no persistent region on fallback).
- Use `CHECK-LABEL:` to anchor multi-function tests.
- Negative tests (rejection / fallback) must use `CHECK-NOT:` for the
  structured artifact that should be absent AND `CHECK:` for the fallback
  artifact that should be present.

### RUN Line Convention

```mlir
// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline <stage> | %FileCheck %s
```

Where `<stage>` is the pipeline stage whose output the test validates.

## 4. Benchmark Family Mapping

Each benchmark family maps to one or more test classes. This ensures that the
contract test suite covers the same kernel shapes that the benchmark suite
exercises at runtime.

| Benchmark | Kernel Family | Primary Test Classes | Pattern |
|-----------|--------------|---------------------|---------|
| `polybench/jacobi2d` | stencil / timestep chain | `structured_plan_*`, `persistent_region_*` | Owner-strip stencil with timestep repetition |
| `polybench/seidel-2d` | wavefront | `structured_plan_*` | Split-phase halo, wavefront iteration |
| `polybench/convolution-2d` | neighborhood | `structured_plan_*` | Neighborhood window, block-local read-only input |
| `polybench/gemm` | uniform / matmul | `structured_plan_*`, `structured_fallback_*` | Uniform block distribution, no stencil features |
| `polybench/2mm` | uniform / matmul | `structured_plan_*`, `structured_fallback_*` | Multi-kernel uniform, reduction chain |
| `polybench/3mm` | uniform / matmul | `structured_plan_*`, `structured_fallback_*` | Multi-kernel uniform, reduction chain |
| `sw4lite/rhs4sg-base` | stencil / neighborhood | `structured_plan_*`, `ownership_proof_*` | Blocked neighborhood ownership, loop-local weights |
| `sw4lite/vel4sg-base` | stencil / neighborhood | `structured_plan_*`, `ownership_proof_*` | Blocked neighborhood ownership |
| `specfem3d/velocity` | stencil / neighborhood | `structured_plan_*`, `ownership_proof_*` | Block/stencil ownership, dep routing |
| `specfem3d/stress` | stencil / neighborhood | `structured_plan_*`, `ownership_proof_*` | Block/stencil ownership, dep routing |
| (irregular / indirect) | irregular | `structured_plan_*` (negative) | Must NOT produce a structured plan |

### Negative Test Families

Deliberately irregular or non-affine kernels that must be rejected by plan
synthesis:

- Indirect-access patterns (e.g., `A[B[i]]` style indirection)
- Non-affine loop bounds
- Mixed-indirect data access patterns
- Kernels with data-dependent control flow in the hot loop

## 5. Positive/Negative Pairing Rule

**Every positive structured-path test must have a paired fallback or reject test
using the same family of IR shape.**

This rule ensures that:

1. The structured path is never the only compilation route for any kernel shape.
2. Disabling or rejecting the structured path does not break compilation.
3. Regression testing covers both directions: "structured works" and "generic
   still works."

### Pairing Examples

| Positive Test | Paired Negative/Fallback Test |
|---------------|-------------------------------|
| `structured_plan_stencil_timestep_synthesis.mlir` | `structured_fallback_stencil_timestep_generic.mlir` |
| `structured_plan_wavefront_synthesis.mlir` | `structured_plan_wavefront_irregular_rejection.mlir` |
| `ownership_proof_exact_owner_local_no_widening.mlir` | `ownership_proof_block_local_requires_full_range_reject.mlir` |
| `persistent_region_timestep_legal_positive.mlir` | `persistent_region_proof_failure_fallback.mlir` |
| `persistent_region_timestep_legal_positive.mlir` | `persistent_region_cost_gate_fallback.mlir` |
| `state_dep_schema_cps_explicit_consumption.mlir` | `state_dep_schema_width_mismatch_verifier_reject.mlir` |

## 6. Phase-by-Phase Rollout Plan

Tests are introduced incrementally, aligned with the RFC's implementation
phases. Each phase adds tests for the architecture surface it introduces.

### Phase 0: Baseline and Gap Documentation (Current)

**Goal**: Document the test matrix, freeze naming conventions, identify gaps in
existing coverage.

Tests landed:
- This document (contract matrix specification).
- Gap analysis against existing `tests/contracts/` coverage.
- No new `.mlir` test files yet -- Phase 0 is documentation only.

Existing tests that partially cover structured kernel concerns (pre-RFC):
- `lowering_contract_verifier_requires_owner_dims_for_offsets.mlir`
- `lowering_contract_verifier_rejects_owner_dims_duplicates.mlir`
- `lowering_contract_contract_flags_roundtrip.mlir`
- `jacobi_stencil_normalization.mlir`
- `jacobi_stencil_intranode_worker_cap.mlir`
- `seidel_wavefront_keeps_frontier_parallelism.mlir`
- `seidel_wavefront_emit_ro_halo_slices.mlir`
- `narrowable_dep_readonly_worker_slice.mlir`

### Phase 1: Structured Kernel Plan Analysis

**Goal**: Validate `StructuredKernelPlanAnalysis` synthesis, rejection, and
preservation.

New tests:
- `structured_plan_stencil_timestep_synthesis.mlir` -- plan emitted for jacobi2d-like kernel
- `structured_plan_wavefront_synthesis.mlir` -- plan emitted for seidel-2d-like kernel
- `structured_plan_neighborhood_synthesis.mlir` -- plan emitted for convolution-2d-like kernel
- `structured_plan_uniform_synthesis.mlir` -- plan emitted for gemm-like kernel
- `structured_plan_irregular_indirect_rejection.mlir` -- plan rejected for indirect-access kernel
- `structured_plan_nonaffine_rejection.mlir` -- plan rejected for non-affine bounds
- `structured_plan_survives_edt_distribution.mlir` -- plan preserved through edt-distribution
- `structured_plan_survives_db_partitioning.mlir` -- plan preserved through db-partitioning
- `structured_plan_survives_post_db_refinement.mlir` -- plan preserved through post-db-refinement
- `structured_fallback_stencil_timestep_generic.mlir` -- generic path works for stencil timestep shape
- `structured_fallback_uniform_generic.mlir` -- generic path works for uniform matmul shape

### Phase 2: Split Launch State IR

**Goal**: Validate state/deps split correctness and stability.

New tests:
- `state_dep_schema_state_pack_roundtrip.mlir` -- state_pack/unpack carry correct values
- `state_dep_schema_dep_bind_roundtrip.mlir` -- dep.bind/forward carry correct values
- `state_dep_schema_cps_explicit_consumption.mlir` -- CPS relaunch uses explicit schema
- `state_dep_schema_noncps_explicit_consumption.mlir` -- non-CPS structured launch uses same schema
- `state_dep_schema_ordering_deterministic.mlir` -- launch-state ordering is deterministic
- `state_dep_schema_forwarded_dep_alignment.mlir` -- forwarded dep slots aligned with relaunch ABI
- `state_dep_schema_width_mismatch_verifier_reject.mlir` -- verifier rejects width mismatch

### Phase 3: CPS Schema Cleanup

**Goal**: Validate that CPS relaunch no longer depends on positional pack
archaeology.

New tests:
- `state_dep_schema_cps_no_pack_archaeology.mlir` -- no `arts.cps_param_perm` in output
- `state_dep_schema_cps_no_dep_routing_attr.mlir` -- no `arts.cps_dep_routing` in output
- `state_dep_schema_cps_chain_explicit_schema.mlir` -- multi-step CPS chain uses explicit schema throughout

### Phase 4: Proof-Driven Ownership

**Goal**: Validate ownership proof correctness and verifier enforcement.

New tests:
- `ownership_proof_owner_dim_stencil_assignment.mlir` -- correct owner-dim for stencil pattern
- `ownership_proof_owner_dim_block_assignment.mlir` -- correct owner-dim for block pattern
- `ownership_proof_worker_local_slice_legal.mlir` -- legal worker-local slice shape
- `ownership_proof_halo_widening_accepted.mlir` -- halo widening legal and accepted
- `ownership_proof_halo_widening_rejected.mlir` -- halo widening rejected when unsound
- `ownership_proof_block_local_exact_reachability.mlir` -- block-local read with exact owner-local reachability
- `ownership_proof_block_local_requires_full_range_reject.mlir` -- block-local read rejected, proof requires full-range
- `ownership_proof_owner_dim_halo_reject.mlir` -- impossible owner-dim / halo combination rejected
- `ownership_proof_preservation_across_passes.mlir` -- proof facts not silently dropped

### Phase 5: Persistent Structured Regions

**Goal**: Validate persistent region legality and fallback behavior.

New tests:
- `persistent_region_timestep_legal_positive.mlir` -- persistent region for proven timestep kernel
- `persistent_region_neighborhood_legal_positive.mlir` -- persistent region for proven neighborhood kernel
- `persistent_region_proof_failure_fallback.mlir` -- fallback when ownership proof fails
- `persistent_region_cost_gate_fallback.mlir` -- fallback when cost model rejects
- `persistent_region_irregular_rejection.mlir` -- no persistence for irregular loop shapes
- `persistent_region_nested_rejection.mlir` -- no persistence for nested unsafe shapes
- `persistent_region_stable_slice_ownership.mlir` -- persistent launch owns stable local slice
- `persistent_region_explicit_halo_refresh.mlir` -- halo refresh is explicit within region
- `structured_fallback_persistent_disabled_generic.mlir` -- generic path when persistence disabled

## 7. Summary

| Test Class | Phase Introduced | Minimum Tests | Positive/Negative Pairs |
|------------|-----------------|---------------|------------------------|
| `structured_plan_*` | Phase 1 | 9 | 4 synthesis + 2 rejection + 3 preservation |
| `state_dep_schema_*` | Phase 2-3 | 10 | 7 positive + 3 negative/absence |
| `ownership_proof_*` | Phase 4 | 9 | 4 positive + 4 rejection + 1 preservation |
| `persistent_region_*` | Phase 5 | 8 | 3 positive + 4 rejection/fallback + 1 refresh |
| `structured_fallback_*` | Phase 1-5 | 4 | 4 fallback guards (paired with positives above) |
| **Total** | | **40** | **20 positive / 20 negative or fallback** |

All 40 tests maintain the pairing invariant: every positive structured-path test
has a corresponding fallback or rejection test using the same IR shape family.
