---
name: carts-benchmark-triage
description: Triage CARTS benchmark failures by rerunning the benchmark, locating runner artifacts, checking the OpenMP baseline built with carts clang, dumping selected pipeline stages, and reading the pass/heuristic ownership docs. Use when a benchmark fails, times out, produces wrong checksums, shows suspicious speedups, or needs pass-by-pass/runtime diagnosis.
user-invocable: true
allowed-tools: Bash, Read, Write, Grep, Glob, Agent
argument-hint: [<benchmark-path>]
---

# CARTS Benchmark Triage

Use bundled helpers when they fit:
- `scripts/rerun-benchmark.sh` — rerun `carts triage-benchmark` with explicit size/threads/stages
- `scripts/locate-run-artifacts.sh` — locate logs, flags, and configs under a benchmark results tree
- `scripts/find-benchmark-codepaths.sh` — grep benchmark-driver and compiler codepaths

Read these before classifying the regression:
- `references/failure-classification.md`
- `../debug/references/failure-signatures.md`
- `../debug/references/command-patterns.md`

## Primary Command

```bash
carts triage-benchmark <suite/name> --size small --threads 2
```

Optional narrowing to specific stages:
```bash
carts triage-benchmark <suite/name> --stages edt-distribution,db-partitioning
```

Only use stage names from `carts pipeline --json`.

## Triage Order

1. `carts doctor` + `carts build` to confirm environment health
2. Rerun the failing benchmark with same size and thread count
3. Check runner artifacts: `arts.log`, `omp.log`, `.arts_cflags`, `.omp_cflags`, `arts.cfg`
4. Classify the failure:
   - OpenMP baseline also broken (test with `carts clang`)
   - ARTS-only regression from a pipeline stage
   - Benchmark-side UB or invalid verification
   - Runtime/distributed lowering issue
5. Read stage dumps in order: `openmp-to-arts` → `create-dbs` → `edt-distribution` → `post-distribution-cleanup` → `db-partitioning` → `post-db-refinement` → `late-concurrency-cleanup` → `pre-lowering` → `arts-to-llvm`

If the regression is multi-node specific or depends on `--distributed-db`, switch to `carts-distributed-triage`.

## OpenMP Baseline

Always check the reference path before assuming a CARTS transform bug:
```bash
carts cgeist source.c -O3 -S --emit-llvm -fopenmp ... -o bench-omp.ll
carts clang bench-omp.ll ... -o bench_omp
```

## Key Docs

- `docs/compiler/pipeline.md`, `docs/heuristics/partitioning.md`, `docs/heuristics/distribution.md`
- `tools/compile/Compile.cpp`, `lib/arts/passes/transforms/ForLowering.cpp`
- `external/carts-benchmarks/common/carts.mk`

## Validation

Always rerun before closing: `carts test`, the affected benchmark, and the relevant sweep. For scheduling/lowering/ownership changes, also rerun a larger case + 64-thread case.
