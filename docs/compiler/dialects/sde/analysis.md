# SDE Analyses

SDE analyses operate while source semantics and structured memref accesses are
still visible.

Owned analyses:

- `PatternAnalysis`: classifies elementwise, reduction, matmul, stencil,
  wavefront, timestep, and unknown families.
- Memref root/access analysis: records roots, ranks, access modes, affine or
  structured index maps, self-read status, and side effects.
- Dependence/window analysis: proves owner-slice independence, halos, phase
  edges, task dependency slices, and barrier requirements.
- Reduction analysis: distinguishes scalar reductions, per-output reductions,
  local accumulators, partial storage, tree, and atomic candidates.
- Logical resource analysis: uses `sde.resource_query <logical_workers>` or
  target-neutral estimates without naming ARTS topology.
- Barrier analysis: proves control edges before ARTS sees flat EDT/epoch
  objects.

Emitted facts:

- `sde.pattern`
- `structuredClassification`
- owner/spatial/reduction dims
- `physicalOwnerDims`, `physicalBlockShape`, `physicalHaloShape`
- `logicalWorkerSlice`
- MU roots and MU tokens
- reduction attributes
- token/codelet boundary requirements for SDE-to-ARTS

SDE analysis facts are consumed by SDE transforms and SDE-to-ARTS conversion.
They must not become hidden ARTS analysis inputs.
