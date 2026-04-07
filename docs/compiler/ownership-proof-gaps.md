# Ownership Proof Gaps

Reference document for the CARTS DB ownership decision flow, current proof
gaps, the `DbPartitionState` infrastructure, and how RFC Phase 4 will replace
the current system with proof-driven ownership.

See also: [Structured Kernel Plan RFC](structured-kernel-plan-rfc.md)

## 1. Ownership Decision Flow

Ownership decisions -- which physical dimensions are owned, what partition
mode to use, and what halo widening is legal -- flow through five pipeline
stages.

### 1.1 Pattern Discovery (stages 5-6)

**Passes**: `pattern-pipeline`, `edt-transforms`

**Source files**:
- `lib/arts/passes/opt/general/` (loop reordering, kernel transforms)
- `lib/arts/passes/opt/edt/` (EDT structural optimization)

**What happens**: Loop and array metadata from `collect-metadata` are analyzed
to detect dep patterns (`ArtsDepPattern`), distribution kinds, and stencil
footprints. Results are stamped as `LoweringContractOp` attributes on DB
source pointers and as `depPattern` / `distribution_*` attributes on EDT and
loop operations.

**Key data produced**:
- `dep_pattern` on `EdtOp` (e.g., `stencil`, `uniform`, `matmul`)
- `distribution_kind` / `distribution_pattern` on loop ops
- `LoweringContractOp` with `owner_dims`, `min_offsets`, `max_offsets`,
  `block_shape`, `supported_block_halo`

### 1.2 DbAnalysis (stage 8)

**Pass**: `db-opt` (mode tightening, scratch elimination)

**Source files**:
- `include/arts/analysis/db/DbAnalysis.h`
- `lib/arts/analysis/db/DbAnalysis.cpp`

**What happens**: `DbAnalysis` builds `DbGraph` per function, computing
`AcquireContractSummary` and `AcquirePartitionSummary` for each `DbAcquireOp`.
The summary reconciles the canonical IR contract (`LoweringContractInfo`) with
graph-derived evidence (access pattern classification, partition dim inference).

**Key interfaces**:
- `DbAnalysis::buildCanonicalAcquireContractSummary(acquire)` -- IR-only view
- `DbAnalysis::getAcquireContractSummary(acquire)` -- graph-refined view
- `DbAnalysis::analyzeAcquirePartition(acquire, builder)` -- partition plan

**Proof gap**: `AcquireContractSummary` carries both `contract` (canonical)
and `derivedAccessPattern` (graph-derived), but there is no formal proof that
the derived pattern is consistent with the canonical contract. The
`refinedByDbAnalysis` flag indicates graph refinement occurred, but not whether
it was sound.

### 1.3 DbPartitioning (stage 13)

**Pass**: `db-partitioning`

**Source files**:
- `lib/arts/passes/opt/db/DbPartitioning.cpp`

**What happens**: For each `DbAllocOp`, the pass:
1. Queries `DbAnalysis::analyzeAcquirePartition` for all acquires.
2. Decides partition mode (`coarse`, `block`, `stencil`, `fine_grained`).
3. Rewrites `DbAcquireOp` with `partition_offsets`, `partition_sizes`.
4. Handles stencil-specific halo bounds insertion.

**Key decisions**:
- Owner dim selection: uses `LoweringContractInfo.spatial.ownerDims`.
- Block shape: uses contract `blockShape` or heuristic chunk sizes.
- Stencil bounds guards: inserts runtime clamping for halo regions.

**Proof gap**: The pass trusts `ownerDims` from the contract but does not
verify that the selected owner dims are reachable from the EDT's iteration
space. A contract stamped in pattern-pipeline may become invalid if
intermediate passes restructure the iteration topology.

### 1.4 ForLowering (stage 11)

**Pass**: `edt-distribution` (contains `ForLowering`)

**Source files**:
- `lib/arts/passes/transforms/ForLowering.cpp`
- `lib/arts/passes/transforms/EdtDistribution.cpp`

**What happens**: Distributes loop iterations across workers. Uses
`LoweringContractInfo` to determine:
- Worker-local slice shape.
- Halo extension for stencil reads.
- Whether to use partition slice or parent dep range as the dependency window.

**Key contract queries** (from `include/arts/utils/LoweringContractUtils.h`):
- `shouldApplyStencilHalo(contract, acquire)` -- mode=in + block halo support
- `shouldUsePartitionSliceAsDepWindow(contract, acquire)` -- stencil/wavefront
- `shouldPreserveParentDepRange(contract, acquire)` -- no narrowing

**Proof gap**: `ForLowering` reads the contract but does not re-validate
ownership. If `DbPartitioning` changed partition dims or shapes,
`ForLowering` may use stale contract information.

### 1.5 Late DB/EDT Lowering (stages 17-18)

**Passes**: `pre-lowering`, `arts-to-llvm`

**Source files**:
- `lib/arts/passes/transforms/EdtLowering.cpp`
- `lib/arts/passes/transforms/EpochLowering.cpp`
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`

**What happens**: Lowers ARTS dialect ops to runtime API calls. At this point,
ownership decisions are baked into `DbAcquireOp` structure (partition mode,
offsets, sizes). Lowering trusts the IR is correct.

**Proof gap**: No late verification. If earlier passes produced inconsistent
partition state, lowering emits incorrect runtime calls silently.

## 2. Proof Gaps

### 2.1 Owner-Dim Reachability

**Gap**: A `LoweringContractOp` with `owner_dims = [0]` asserts that physical
dimension 0 is owned (partitioned). But there is no proof that the EDT's
iteration variable actually maps to dimension 0 of the DB's address space.

**Where it matters**:
- `DbPartitioning.cpp` uses `contract.spatial.ownerDims` to select which dims
  get block sizes.
- `ForLowering` uses owner dims to compute worker-local slices.

**Current validation**: `ContractValidation` (line 114) checks that owner_dims
values are non-negative and (with `warnOnProofGaps`) that they are less than
the memref rank. But it does not verify that the EDT's loop induction variable
actually indexes into the declared owner dim.

**Required proof**: For each `owner_dim d`, there exists a monotone mapping
from the EDT's iteration variable to dimension `d` of the DB's access pattern.

### 2.2 Partition-to-Access Mapping

**Gap**: `DbPartitioning` computes partition offsets and sizes, but does not
formally verify that every `DbAcquireOp` in the partitioned DB's use set
accesses only within the declared partition window.

**Where it matters**:
- Block-partitioned DBs assume each worker accesses `[offset, offset+size)`.
- If an access falls outside this window, the runtime returns the wrong data
  (no bounds checking at the DB level).

**Current validation**: `DbAnalysis::analyzeAcquirePartition` computes a
`partitionSummary` with `isValid` flag, but this is advisory. The pass does
not abort if the summary reports inconsistencies.

**Required proof**: For each `DbAcquireOp` under a block-partitioned
`DbAllocOp`, the access footprint (offsets, sizes, strides) must be provably
contained within the partition window.

### 2.3 Halo Legality

**Gap**: Stencil kernels extend the partition window with halo regions
(`min_offsets`, `max_offsets`). The current system stamps halo bounds from
pattern analysis but does not verify they are sufficient for all accesses.

**Where it matters**:
- `DbPartitioning` inserts stencil bounds guards (runtime clamping).
- `ForLowering` uses `shouldApplyStencilHalo` to decide whether to extend
  read slices.
- If the halo is too narrow, neighbor accesses read from the wrong partition.
- If the halo is too wide, unnecessary data is transferred.

**Current validation**: `ContractValidation` checks that `min_offsets` and
`max_offsets` have matching rank (line 96-102) and that `supported_block_halo`
requires a stencil dep pattern (line 81-93). With `warnOnProofGaps`, it warns
on stencil patterns without `supported_block_halo`.

**Required proof**: The halo window `[min_offset, max_offset]` per dimension
must cover the maximum read footprint of all stencil neighbor accesses
relative to the owner partition.

### 2.4 Dependence Slice Soundness

**Gap**: `shouldPreserveParentDepRange` and `shouldUsePartitionSliceAsDepWindow`
make binary decisions about whether a read acquire should use the narrow
partition slice or the full parent range. There is no formal proof that the
narrow slice is sufficient for correctness.

**Where it matters**:
- `ForLowering` calls `shouldPreserveParentDepRange` for read acquires
  without explicit stencil contracts.
- An incorrect narrowing causes the runtime to acquire a slice that does not
  cover all needed elements, producing wrong results.

**Current validation**: The `narrowable_dep` attribute is set by pattern
analysis as a hint. `ContractValidation` warns (with `warnOnProofGaps`) when
`narrowable_dep` is present without `owner_dims`.

**Required proof**: For each read acquire with a narrowed dep window, the
narrowed range must provably contain all accessed elements under all valid
iteration orderings.

### 2.5 Relaunch-State Soundness for Persistent Regions

**Gap**: Not yet implemented. RFC Phase 5 requires that persistent structured
regions prove their state/dep channels remain valid across multiple logical
iterations without teardown.

## 3. DbPartitionState Infrastructure

Defined in `include/arts/analysis/db/DbPartitionState.h`.

### 3.1 ProvenancedFact<T>

A single partition property with provenance tracking:

```cpp
template <typename T> struct ProvenancedFact {
  T value;
  FactProvenance provenance;   // IRContract | GraphInferred | HeuristicAssumption
  bool isKnown() const;       // provenance == IRContract
  bool isAssumed() const;     // provenance == HeuristicAssumption
  bool isGraphInferred() const;
  bool strengthen(T newValue, FactProvenance newProvenance);
};
```

Provenance ordering: `IRContract < GraphInferred < HeuristicAssumption`.
Strengthening only succeeds when the new provenance is strictly less (more
certain) than the current one.

### 3.2 FactProvenance Enum

| Value | Meaning |
|-------|---------|
| `IRContract` | Proved from `arts.lowering_contract` IR attributes. Conservative; always sound. |
| `GraphInferred` | Inferred from `DbGraph` / `DbNode` analysis (access patterns, partition bounds). Intermediate confidence. |
| `HeuristicAssumption` | Assumed from heuristic context. No evidence; best guess. Optimistic. |

### 3.3 DbPartitionState Struct

Formalized partition state for a DB allocation:

| Field | Type | Meaning |
|-------|------|---------|
| `partitionMode` | `ProvenancedFact<PartitionMode>` | coarse, block, stencil, fine_grained |
| `ownerDims` | `ProvenancedFact<SmallVector<int64_t>>` | Which physical dims are partitioned |
| `blockShape` | `ProvenancedFact<SmallVector<int64_t>>` | Static block sizes per dim |
| `minOffsets` | `ProvenancedFact<SmallVector<int64_t>>` | Stencil halo minimum |
| `maxOffsets` | `ProvenancedFact<SmallVector<int64_t>>` | Stencil halo maximum |
| `accessPattern` | `ProvenancedFact<AccessPattern>` | uniform, stencil, indexed, etc. |

Key methods:
- `isFullyKnown()`: Both `partitionMode` and `ownerDims` are `IRContract`.
- `hasAssumedProperties()`: Any field is `HeuristicAssumption`.
- `meet(other)`: Monotone meet -- narrows assumed state toward known.

### 3.4 Current Adoption Status

`DbPartitionState` is **declarative infrastructure**. The header comment
(line 63-66) states:

> This struct is declarative infrastructure for future adoption. Current code
> continues to use LoweringContractInfo + graph queries. Migration path:
> passes gradually adopt ProvenancedFact to distinguish between IR-proved
> and heuristic-assumed properties.

Current passes (`DbPartitioning`, `ForLowering`) read from
`LoweringContractInfo` and `AcquireContractSummary`, not from
`DbPartitionState` directly.

## 4. Current vs Required Validation

### 4.1 ContractValidation (Current)

Source: `lib/arts/passes/opt/db/ContractValidation.cpp`

**Phase 1 -- Structural checks** (always on):
1. Target value exists and is a valid memref.
2. `supported_block_halo` requires stencil-family `dep_pattern`.
3. `min_offsets` / `max_offsets` must have matching rank.
4. `block_shape` rank > 0 if present.
5. `owner_dims` values are non-negative.

**Phase 2 -- Orphan acquire detection** (always on):
- Warns if a `DbAcquireOp` has stencil attributes but no
  `LoweringContractOp` on its source pointer.

**Phase 4 -- Proof gap warnings** (gated by `warnOnProofGaps`):
- `owner_dims` without `block_shape`.
- Stencil pattern without `supported_block_halo`.
- `narrowable_dep` without `owner_dims`.
- `owner_dims` value >= memref rank.
- `block_shape` without `owner_dims`.
- CPS `cps_chain_id` without `cps_dep_routing`.
- CPS `cps_forward_deps` without `cps_dep_routing`.
- CPS `cps_param_perm` with negative slot.

### 4.2 Validation Gaps

| Gap | Current | Required |
|-----|---------|----------|
| Owner-dim reachability | Checks non-negative and < rank | Prove monotone mapping from EDT iteration to owner dim |
| Partition-access containment | None | Prove every acquire footprint fits within partition window |
| Halo sufficiency | Checks rank match | Prove halo covers maximum read footprint |
| Narrowing soundness | Warns on missing owner_dims | Prove narrowed range contains all accessed elements |
| Cross-pass consistency | None | Verify contract integrity at pass boundaries (post-distribution, post-partitioning, post-refinement) |
| Persistent-region state validity | None | Prove state/dep channels remain valid across iterations |

## 5. Phase 4 Replacement Plan

RFC Phase 4 (see `docs/compiler/structured-kernel-plan-rfc.md` section 7.4)
will extend `LoweringContractOp` with proof-derived ownership fields.

### 5.1 New Proof Surface

The `LoweringContractOp` will carry proof-derived fields for:

1. **Owner-dim reachability**: For each declared owner dim, a proof that the
   EDT's iteration variable maps to that dim.
2. **Partition-to-access mapping**: For each acquire under the partitioned
   alloc, a proof that the access footprint is contained in the partition
   window.
3. **Halo legality**: For stencil acquires, a proof that the halo window
   covers the maximum neighbor access footprint.
4. **Dependence slice soundness**: For narrowed dep windows, a proof that the
   narrow range is sufficient.
5. **Relaunch-state soundness**: For persistent regions, a proof that state/dep
   channels remain valid across iterations.

### 5.2 ContractValidation Upgrade

`ContractValidation` will check semantic soundness, not just structural
integrity:

- **Reject** impossible owner-dim / halo combinations (not just warn).
- **Reject** block-local reads when proof requires full-range.
- **Accept** exact owner-local reachability without widening.
- **Fail** on proof inconsistencies, not only on missing attributes.

### 5.3 Migration from DbPartitionState

The `ProvenancedFact<T>` / `DbPartitionState` infrastructure provides the
right abstraction for the proof system:

- `FactProvenance::IRContract` maps to "proof-derived, verified".
- `FactProvenance::GraphInferred` maps to "evidence-backed, unverified".
- `FactProvenance::HeuristicAssumption` maps to "best guess, needs proof".

Migration path:
1. `DbPartitioning` adopts `DbPartitionState` as its internal state.
2. Partition decisions record provenance.
3. `ContractValidation` checks that all partition decisions are at least
   `GraphInferred`, and warns on `HeuristicAssumption`.
4. Phase 4 requires `IRContract` provenance for all ownership decisions in
   structured kernels.

### 5.4 DB/EDT Lowering Consumption

Once proofs are in place, late lowering passes will consume the proof directly:
- `EdtLowering` reads the proof to determine dep window widths.
- `EpochLowering` reads the proof to determine carry state layout.
- If a proof is missing or invalid, lowering fails instead of guessing.
