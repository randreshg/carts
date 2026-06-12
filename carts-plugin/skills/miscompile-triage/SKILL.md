---
name: carts-miscompile-triage
description: Use when a program compiles but produces wrong output, checksum mismatches, phase-equivalence failures, or suspicious partitioning/distribution decisions.
user-invocable: true
allowed-tools: Bash, Read, Write, Grep, Glob, Agent
argument-hint: [<input-file | benchmark-path>]
parameters:
  - name: bug_description
    type: str
    gather: "Describe the miscompilation symptom — wrong output, checksum mismatch, etc."
  - name: trace_file
    type: str
    gather: "Path to the failing test file or MLIR input"
---

# CARTS Miscompile Triage

Goal: find the first stage where CARTS stops being semantically equivalent to the reference path.

The first bad stage is not automatically the fix stage. Use [[carts-vision]]
to attribute the wrong committed fact: SDE fixes source/layout/tiling facts,
ARTS fixes graph/contraction realization, ARTS fixes
DB/EDT/distributed ownership realization and grouped execution, and ARTS-RT fixes only
mechanical lowering.

Use bundled helpers when they fit:
- `scripts/prepare-c-inputs.sh` — produce sequential and OpenMP MLIR from a C/C++ source
- `scripts/dump-focus-stages.sh` — materialize the highest-value stage dumps into one directory
- `scripts/collect-diagnose.sh` — capture `--diagnose` JSON plus the pipeline manifest
- `scripts/find-semantic-codepaths.sh` — grep the main semantic fact codepaths

Read these before guessing:
- `references/stage-focus.md`
- `../debug/references/failure-signatures.md`
- `../debug/references/stage-ownership.md`
- `../debug/references/codepath-map.md`

## Triage Order

1. Establish the oracle first.
   - Prefer the OpenMP baseline or known-good sequential result.
   - Record the checksum, diff, or phase-equivalence criterion before changing anything.
2. Reproduce on the smallest input that still fails.
   - If only a large benchmark fails, try to keep the same access pattern while shrinking dimensions.
3. Normalize the input form.
   - For C/C++ input: use `dekk carts cgeist ... -S` to capture the starting MLIR.
   - For MLIR input: work from the failing stage dump directly.
4. Bisect the pipeline.
   - Use `dekk carts pipeline --json` for the stage list.
   - Use `dekk carts compile <file>.mlir --pipeline=<stage>` and `--all-pipelines -o DIR/`.
   - Focus on: `sde-planning` -> `sde-to-arts` -> `sde-to-arts` -> `edt-transforms` -> `create-dbs` -> `db-opt` -> `post-db-refinement` -> `pre-lowering`.
5. Inspect semantic facts before low-level codegen.
   - `dekk carts compile <file> --diagnose --diagnose-output diag.json -O3`
   - Check lowering facts, `distribution_*`, partition modes, full-range vs
     coarse, and `preserve_access_mode`.
   - Check whether downstream layers are consuming committed facts or
     recomputing source semantics, owner dims, movement family, storage
     grain, or runtime policy.
6. Reduce the failure once the first bad stage is known.
   - Preserve the exact failing pattern and expected output.

## Reference Path

Always validate the non-ARTS path before blaming a transform:

```bash
dekk carts cgeist source.c -O3 -S -fopenmp --raise-scf-to-affine -o source.omp.mlir
dekk carts clang source.ll ... -o baseline
```

For benchmark kernels, also consult `carts-benchmark-triage`.

## Key Docs

- `docs/compiler/pipeline.md`
- `docs/heuristics/partitioning.md`
- `docs/heuristics/distribution.md`
- `tools/compile/Compile.cpp`
- `lib/carts/dialect/arts/Transforms/SdeToArtsBoundary.cpp`
- `lib/carts/dialect/arts/Transforms/db/DbModeTightening.cpp`
- `lib/carts/dialect/arts/Transforms/db/CreateDbs.cpp`
- `lib/carts/dialect/sde/Transforms/effect/distribution/DistributionPlanning.cpp`
- `lib/carts/dialect/sde/Transforms/effect/scheduling/ReductionStrategy.cpp`

## Hand-off

- Runtime hang/crash after correct IR -> `carts-runtime-triage`
- Stale analysis or pass-ordering sensitivity -> `carts-analysis-triage`
- Multi-node only failure -> `carts-distributed-triage`
- Need a minimal reproducer -> `carts-reproducer`
- Heuristic decision seems wrong -> `/heuristic-explain`
- Need to compare adjacent stages -> `/stage-diff`

## Validation

Do not stop at “found suspicious IR”.
Rerun the reference path, the failing path, and the smallest preserved reproducer before closing.
