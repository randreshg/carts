# Live Pipeline Reference

Source of truth: `tools/compile/Compile.cpp`, especially `getStageRegistry()`
and the pass arrays. Validate with:

```bash
dekk carts pipeline --json
```

Use the JSON manifest when exact pass labels, dependency edges, or bracketed
pass names matter.

## Canonical Stage Order

1. `sde-input-normalization`
2. `initial-cleanup`
3. `sde-planning`
4. `sde-to-arts`
5. `edt-dep-realization`
6. `edt-local-cleanup`
7. `create-dbs`
8. `db-opt`
9. `post-db-refinement`
10. `late-concurrency-cleanup`
11. `epochs`
12. `pre-lowering`
13. `arts-rt-to-llvm`

`--pipeline` also accepts the sentinel `complete`. `--start-from` accepts
canonical stages only.

## Conditional Epilogues

- `post-o3-opt` runs when `--O3` is active.
- `llvm-ir-emission` runs when LLVM IR emission is requested.

Do not use stale epilogue names such as `complete-mlir` or `emit-llvm` unless
you are specifically fixing outdated docs.

## High-Value Drift Checks

- `sde-input-normalization` includes `ScalarForwarding`,
  `SdeMemrefNormalization`, and `SdeHandleDeps`.
- `sde-planning` owns OpenMP-to-SDE conversion, pattern/distribution/reduction
  transforms, iteration-space decomposition, and MU realization intent.
- `sde-to-arts` owns mechanical SDE-to-ARTS DB/acquire/EDT realization and
  rejects any surviving SDE operation.
- `edt-dep-realization` realizes EDT distribution deps and verifies the ARTS
  object boundary.
- `post-db-refinement` runs DB/EDT refinements and validation after
  DB mode tightening.
- `pre-lowering` lowers ARTS DB/EDT/epoch objects to ARTS-RT-shaped operations and
  verifies the result with `VerifyPreLowered`.
