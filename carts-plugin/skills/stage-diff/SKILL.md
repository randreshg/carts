---
name: carts-stage-diff
description: Compare MLIR IR between pipeline stages to find where semantics diverge, identify what a pass changed, or bisect which stage introduces a bug. Use when debugging miscompiles, verifying pass correctness, or understanding pipeline transformations.
user-invocable: true
allowed-tools: Bash, Read, Grep, Glob, Agent
argument-hint: [<file> <stage1> <stage2> | bisect <file> | all <file>]
parameters:
  - name: input_file
    type: str
    gather: "Path to the MLIR or C input file to compile"
  - name: action
    type: str
    gather: "Action: 'diff <stage1> <stage2>', 'bisect' (find first bad stage), or 'all' (dump all stages)"
---

# CARTS Pipeline Stage Comparison

## Purpose

Compare IR between pipeline stages to understand transformations, find the
first stage where semantics diverge, or bisect a miscompilation.

## Quick Commands

### Dump all stages
```bash
dekk carts compile <file> --all-pipelines -o /tmp/stages/
ls /tmp/stages/
```

### Compare two stages
```bash
dekk carts compile <file> --pipeline=<stage1> > /tmp/before.mlir
dekk carts compile <file> --pipeline=<stage2> > /tmp/after.mlir
diff /tmp/before.mlir /tmp/after.mlir | head -100
```

### Bisect: find first divergent stage
```bash
# Dump all stages, then compare adjacent pairs
dekk carts compile <file> --all-pipelines -o /tmp/stages/
# Check each stage for the symptom (e.g., missing op, wrong attribute)
for f in /tmp/stages/*.mlir; do
  echo "=== $(basename $f) ==="
  grep -c "symptom_pattern" "$f" || true
done
```

## 18 Pipeline Stages (Canonical Order)

| # | Stage | Primary Question |
|---|-------|-----------------|
| 1 | raise-memref-dimensionality | Are memrefs normalized? |
| 2 | collect-metadata | Is loop/array metadata correct? |
| 3 | initial-cleanup | Is dead code removed? |
| 4 | openmp-to-arts | Are OMP regions converted to EDTs? |
| 5 | pattern-pipeline | Are patterns (stencil, matmul) recognized? |
| 6 | edt-transforms | Is EDT structure optimized? |
| 7 | create-dbs | Are DataBlock allocations created? |
| 8 | db-opt | Are DB access modes correct? |
| 9 | edt-opt | Are EDTs fused/optimized? |
| 10 | concurrency | Is the concurrency graph correct? |
| 11 | edt-distribution | Is distribution strategy assigned? |
| 12 | post-distribution-cleanup | Are loops lowered correctly? |
| 13 | db-partitioning | Are DBs partitioned correctly? |
| 14 | post-db-refinement | Are contracts validated? |
| 15 | late-concurrency-cleanup | Is hoisting/sinking correct? |
| 16 | epochs | Are epochs created correctly? |
| 17 | pre-lowering | Are EDTs/DBs/epochs lowered to RT calls? |
| 18 | arts-to-llvm | Is final LLVM IR correct? |

## Bisection Strategy by Symptom

| Symptom | Start Checking At |
|---------|------------------|
| Wrong array values | db-partitioning (13), then post-db-refinement (14) |
| Missing parallelism | concurrency (10), then edt-distribution (11) |
| Deadlock/hang | epochs (16), then pre-lowering (17) |
| Wrong loop bounds | edt-distribution (11), then post-distribution-cleanup (12) |
| Missing DB | create-dbs (7), then db-opt (8) |
| Pattern not applied | pattern-pipeline (5), then collect-metadata (2) |
| LLVM crash | arts-to-llvm (18), then pre-lowering (17) |

## What to Look for in Diffs

### Structural changes (ops added/removed)
```bash
# Count ops by type at each stage
grep -o 'arts\.[a-z_]*' /tmp/stages/*.mlir | sort | uniq -c | sort -rn
```

### Attribute changes
```bash
# Track specific attributes across stages
grep 'arts.dep_pattern\|arts.partition_mode\|arts.distribution_kind' /tmp/stages/*.mlir
```

### Loop structure changes
```bash
# Count loops at each stage
for f in /tmp/stages/*.mlir; do
  echo "$(basename $f): $(grep -c 'arts.for\|scf.for' $f) loops"
done
```

## Instructions

When the user asks to compare stages or bisect:

1. Determine input file and action (diff, bisect, or all)
2. Dump relevant stages using `dekk carts compile --pipeline=<stage>`
3. For bisection: use the symptom table to pick starting stages
4. Compare adjacent stages, narrowing to the first divergent one
5. Once found, report: which stage, what changed, which pass is responsible
6. Suggest debug channel: `--arts-debug=<snake_case_pass_name>`
