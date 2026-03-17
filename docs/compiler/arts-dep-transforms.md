# DepTransforms

Use:

- [`pipeline.md`](./pipeline.md)
  for pipeline-step placement and ordering
- this document for `DepTransforms` ownership boundaries
- optional owner/tile metadata when the transform creates tiled wavefronts

`DbPartitioning` should consume those hints. It should not re-derive the schedule transform itself.

## Design Rule

If a transformation is justified by:

- "this loop schedule is semantically valid and exposes a better dependence shape"

it belongs in `DepTransforms`.

If it is justified by:

- "this order is more cache-friendly"
- "this reduction can be distributed differently"
- "this DB acquire can be partitioned more safely"

it belongs somewhere else.
