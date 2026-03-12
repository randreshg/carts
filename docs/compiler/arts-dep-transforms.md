# ArtsDepTransforms

Canonical docs for transform ownership now live under
[`docs/transforms/`](../transforms/README.md).

Use:

- [`../transforms/dependence.md`](../transforms/dependence.md)
  for `ArtsDepTransforms`
- [`pipeline.md`](./pipeline.md)
  for stage placement and ordering
- optional owner/tile metadata when the transform creates tiled wavefronts

`DbPartitioning` should consume those hints. It should not re-derive the schedule transform itself.

## Design Rule

If a transformation is justified by:

- "this loop schedule is semantically valid and exposes a better dependence shape"

it belongs in `ArtsDepTransforms`.

If it is justified by:

- "this order is more cache-friendly"
- "this reduction can be distributed differently"
- "this DB acquire can be partitioned more safely"

it belongs somewhere else.
