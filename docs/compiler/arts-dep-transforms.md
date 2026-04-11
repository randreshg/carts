# DepTransforms

Use:

- [`pipeline.md`](./pipeline.md)
  for pipeline-step placement and ordering
- this document for `DepTransforms` ownership boundaries
- optional forwarded owner/tile metadata when ARTS structurally realizes a
  wavefront contract that was selected earlier in SDE

`DbPartitioning` should consume those hints. It should not re-derive the schedule transform itself.

## Design Rule

If a transformation is justified by:

- "this ARTS loop/EDT structure can be cleaned up or realized more directly
  from an already-selected SDE contract"

it belongs in `DepTransforms`.

If it is justified by:

- "this loop should become a wavefront, stencil tile, or other semantic
  schedule family"
- "this order is more cache-friendly"
- "this reduction can be distributed differently"
- "this DB acquire can be partitioned more safely"

it belongs somewhere else.

Architectural note:

- `DepTransforms` does not own semantic pattern choice. Wavefront, stencil,
  matmul, and other structured pattern decisions belong to SDE tensor/linalg
  analyses and SDE cost-model-driven planning.
- If ARTS still performs a wavefront-oriented rewrite on this branch, that
  should be read as transitional realization debt rather than the desired
  long-term ownership model.
