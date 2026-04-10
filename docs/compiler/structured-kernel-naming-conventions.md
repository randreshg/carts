# Structured Kernel Naming Conventions

Frozen naming conventions for the structured kernel plan architecture.
All subsequent phases (1-5) must reference this document for attribute names,
op names, analysis names, pass names, test class names, and enum names.

See also: [Structured Kernel Plan RFC](structured-kernel-plan-rfc.md)

## 1. Attribute Prefixes

### 1.1 `arts.plan.*` -- Structured Kernel Plan Attributes

Produced by `StructuredKernelPlanAnalysis`. Consumed by distribution,
partitioning, and lowering passes.

| Attribute | Type | Semantics |
|-----------|------|-----------|
| `arts.plan.kernel_family` | `StringAttr` | One of: `uniform`, `stencil`, `wavefront`, `reduction_mixed`, `timestep_chain` |
| `arts.plan.iteration_topology` | `StringAttr` | One of: `serial`, `owner_strip`, `owner_tile`, `wavefront`, `persistent_strip`, `persistent_tile` |
| `arts.plan.async_strategy` | `StringAttr` | One of: `blocking`, `advance_edt`, `cps_chain`, `persistent_structured` |
| `arts.plan.repetition_structure` | `StringAttr` | Repetition classification (e.g., `repeated_trip`, `pair_step`, `k_step`) |
| `arts.plan.owner_dims` | `DenseI64ArrayAttr` | Physical dimensions owned by each worker |
| `arts.plan.block_shape` | `DenseI64ArrayAttr` | Physical block shape per dimension |
| `arts.plan.halo_shape` | `DenseI64ArrayAttr` | Halo widths per dimension |
| `arts.plan.worker_slice_shape` | `DenseI64ArrayAttr` | Logical worker slice shape |
| `arts.plan.cost.scheduler_overhead` | `FloatAttr` | Estimated scheduler overhead |
| `arts.plan.cost.slice_widening_pressure` | `FloatAttr` | Slice widening pressure |
| `arts.plan.cost.local_work_per_launch` | `FloatAttr` | Expected local work per launch |
| `arts.plan.cost.relaunch_amortization` | `FloatAttr` | Relaunch amortization benefit |

### 1.2 `arts.launch.*` -- Split Launch State Attributes

Produced during Phase 2 IR construction. Consumed by CPS schema and lowering.

| Attribute | Type | Semantics |
|-----------|------|-----------|
| `arts.launch.state_width` | `IntegerAttr(i64)` | Number of scalar state slots in the state channel |
| `arts.launch.dep_width` | `IntegerAttr(i64)` | Number of dep slots in the dep channel |
| `arts.launch.state_schema` | `DenseI64ArrayAttr` | Ordered type tags for state channel slots |
| `arts.launch.dep_schema` | `DenseI64ArrayAttr` | Ordered type tags for dep channel slots |

### 1.3 `arts.proof.*` -- Ownership Proof Attributes

Produced during Phase 4 proof construction. Consumed by `ContractValidation`
and lowering passes.

| Attribute | Type | Semantics |
|-----------|------|-----------|
| `arts.proof.owner_dim_reachable` | `DenseBoolArrayAttr` | Per-dim flag: iteration variable maps to this owner dim |
| `arts.proof.access_contained` | `UnitAttr` | All acquire footprints fit within partition window |
| `arts.proof.halo_sufficient` | `UnitAttr` | Halo window covers maximum read footprint |
| `arts.proof.narrow_sound` | `UnitAttr` | Narrowed dep window contains all accessed elements |
| `arts.proof.persistent_valid` | `UnitAttr` | State/dep channels remain valid across persistent iterations |
| `arts.proof.provenance` | `StringAttr` | One of: `ir_contract`, `graph_inferred`, `heuristic` |

## 2. Reserved Op Names

These operation names are reserved for Phase 2 split launch state IR.
No existing pass or dialect should introduce ops with these names.

| Op Name | Phase | Purpose |
|---------|-------|---------|
| `arts.state_pack` | Phase 2 | Pack scalar state into the state channel |
| `arts.state_unpack` | Phase 2 | Unpack scalar state from the state channel |
| `arts.dep.bind` | Phase 2 | Bind a DB family to a dep channel slot |
| `arts.dep.forward` | Phase 2 | Forward a dep slot from parent to continuation |
| `arts.edt_launch` | Phase 2 | Unified launch op consuming state + dep channels |

## 3. Analysis Names

| Analysis | Phase | Source |
|----------|-------|--------|
| `StructuredKernelPlanAnalysis` | Phase 1 | New analysis producing `StructuredKernelPlan` |
| `DbAnalysis` | Existing | `include/arts/dialect/core/Analysis/db/DbAnalysis.h` |
| `EdtAnalysis` | Existing | `include/arts/dialect/core/Analysis/edt/EdtAnalysis.h` |
| `LoopAnalysis` | Existing | `include/arts/dialect/core/Analysis/loop/LoopAnalysis.h` |
| `EpochAnalysis` | Existing | `include/arts/dialect/core/Analysis/edt/EpochAnalysis.h` |

## 4. Pass Naming Conventions

### 4.1 Existing Prefixes

| Prefix | Scope | Examples |
|--------|-------|---------|
| `Db` | DataBlock passes | `DbPartitioning`, `DbModeTightening`, `DbScratchElimination` |
| `Edt` | EDT passes | `EdtStructuralOpt`, `EdtDistribution`, `EdtFusion` |
| `Contract` | Validation passes | `ContractValidation` |
| `Epoch` | Epoch passes | `EpochOpt`, `EpochLowering` |

### 4.2 Structured Kernel Pass Names

New passes introduced by the RFC must follow these naming rules:

| Pass Name | Phase | Location |
|-----------|-------|----------|
| `StructuredKernelPlanAnalysis` | Phase 1 | `lib/arts/dialect/core/Analysis/` |
| `StructuredKernelPlanPass` | Phase 1 | `lib/arts/passes/opt/general/` |
| `SplitLaunchStatePass` | Phase 2 | `lib/arts/passes/transforms/` |
| `CpsSchemaCleanupPass` | Phase 3 | `lib/arts/passes/transforms/` |
| `OwnershipProofPass` | Phase 4 | `lib/arts/passes/opt/db/` |
| `PersistentRegionPass` | Phase 5 | `lib/arts/passes/opt/edt/` |

### 4.3 Debug Channel Convention

Debug channels follow the existing convention: `snake_case(PassFilename)`.

| Pass File | Debug Channel |
|-----------|---------------|
| `StructuredKernelPlanPass.cpp` | `structured_kernel_plan_pass` |
| `SplitLaunchStatePass.cpp` | `split_launch_state_pass` |
| `CpsSchemaCleanupPass.cpp` | `cps_schema_cleanup_pass` |
| `OwnershipProofPass.cpp` | `ownership_proof_pass` |
| `PersistentRegionPass.cpp` | `persistent_region_pass` |

## 5. Test Class Names

Contract test files in `tests/contracts/` must use these prefixes:

### 5.1 `structured_plan_*`

Plan creation and rejection tests.

| Test Name | Purpose |
|-----------|---------|
| `structured_plan_stencil_synthesis.mlir` | Plan emitted for regular stencil kernel |
| `structured_plan_timestep_synthesis.mlir` | Plan emitted for timestep chain kernel |
| `structured_plan_irregular_rejection.mlir` | Plan rejected for non-affine kernel |
| `structured_plan_pass_preservation.mlir` | Plan survives distribution and partitioning |

### 5.2 `state_dep_schema_*`

Split launch state schema tests.

| Test Name | Purpose |
|-----------|---------|
| `state_dep_schema_cps_relaunch.mlir` | CPS relaunch consumes explicit schema |
| `state_dep_schema_non_cps.mlir` | Non-CPS launch uses same schema surface |
| `state_dep_schema_width_mismatch.mlir` | Verifier rejects state/dep width mismatch |

### 5.3 `ownership_proof_*`

Ownership proof correctness tests.

| Test Name | Purpose |
|-----------|---------|
| `ownership_proof_owner_halo_reject.mlir` | Impossible owner-dim/halo combination rejected |
| `ownership_proof_block_local_reject.mlir` | Block-local read rejected when proof requires full-range |
| `ownership_proof_exact_reachability.mlir` | Exact owner-local reachability accepted |
| `ownership_proof_narrowing_soundness.mlir` | Dep narrowing validated against access footprint |

### 5.4 `persistent_region_*`

Persistent structured region legality tests.

| Test Name | Purpose |
|-----------|---------|
| `persistent_region_legal_timestep.mlir` | Legal persistent region for timestep kernel |
| `persistent_region_proof_fallback.mlir` | Fallback when proof fails |
| `persistent_region_cost_fallback.mlir` | Fallback when cost gate fails |

### 5.5 `structured_fallback_*`

Generic fallback correctness tests.

| Test Name | Purpose |
|-----------|---------|
| `structured_fallback_disabled_path.mlir` | Same IR compiles through generic path when structured path disabled |
| `structured_fallback_rejected_kernel.mlir` | Rejected kernel compiles through generic path |

## 6. Enum Names

### 6.1 KernelFamily

Classifies the high-level computational pattern:

| Value | Meaning |
|-------|---------|
| `Uniform` | Element-wise or embarrassingly parallel |
| `Stencil` | Neighbor-access pattern with halo regions |
| `Wavefront` | Diagonal sweep with split-phase dependencies |
| `ReductionMixed` | Mixed reduction with element-wise or stencil components |
| `TimestepChain` | Repeated time-step loop over structured body |

### 6.2 IterationTopology

How the iteration space is mapped to workers:

| Value | Meaning |
|-------|---------|
| `Serial` | No distribution; single worker |
| `OwnerStrip` | 1D strip-mined distribution along owner dim |
| `OwnerTile` | N-D tiled distribution along owner dims |
| `Wavefront` | Diagonal wavefront distribution |
| `PersistentStrip` | Persistent 1D strip with multi-step execution |
| `PersistentTile` | Persistent N-D tile with multi-step execution |

### 6.3 AsyncStrategy

How asynchronous execution is managed:

| Value | Meaning |
|-------|---------|
| `Blocking` | Synchronous wait per iteration |
| `AdvanceEdt` | Single advance EDT per iteration (CPS driver) |
| `CpsChain` | Full CPS continuation chain across epochs |
| `PersistentStructured` | Persistent region with explicit state management |

### 6.4 RepetitionStructure

How iterations are grouped:

| Value | Meaning |
|-------|---------|
| `RepeatedTrip` | Simple `for` loop with constant trip count |
| `PairStep` | Two-phase alternating (e.g., Jacobi double buffer) |
| `KStep` | K-way grouped iterations |

## 7. Existing Attribute Constants Reference

For completeness, the existing attribute constants that interact with the
structured kernel system are defined in:

- `include/arts/utils/OperationAttributes.h` -- `AttrNames::Operation::*`
- `include/arts/utils/StencilAttributes.h` -- `AttrNames::Operation::Stencil::*`

Key existing constants used by the structured kernel plan:

| Constant Path | IR Name |
|---------------|---------|
| `AttrNames::Operation::DepPatternAttr` | `depPattern` |
| `AttrNames::Operation::DistributionKind` | `distribution_kind` |
| `AttrNames::Operation::DistributionPattern` | `distribution_pattern` |
| `AttrNames::Operation::Contract::OwnerDims` | `owner_dims` |
| `AttrNames::Operation::Contract::SupportedBlockHalo` | `supported_block_halo` |
| `AttrNames::Operation::Contract::SpatialDims` | `spatial_dims` |
| `AttrNames::Operation::Contract::NarrowableDep` | `narrowable_dep` |
| `AttrNames::Operation::Stencil::FootprintMinOffsets` | `stencil_min_offsets` |
| `AttrNames::Operation::Stencil::FootprintMaxOffsets` | `stencil_max_offsets` |
| `AttrNames::Operation::Stencil::OwnerDims` | `stencil_owner_dims` |
| `AttrNames::Operation::Stencil::BlockShape` | `stencil_block_shape` |
