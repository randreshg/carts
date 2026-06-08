---
name: carts-distributed-triage
description: Use when a failure only appears in multinode/distributed runs (vs the `--no-distributed-db` baseline), multiple nodes, SDE/CODIR/ARTS distributed work materialization, or uneven remote work distribution.
user-invocable: true
allowed-tools: Bash, Read, Write, Grep, Glob, Agent
argument-hint: [<input-file | benchmark-path>]
---

# CARTS Distributed Triage

Goal: determine whether a multi-node failure comes from ownership marking, lowering, runtime routing, or benchmark/runtime configuration.

Layer rule: diagnose the first layer where the committed fact is wrong, then fix
that layer with a real transformation. SDE owns layout and source/SU/CU/MU
rewrites; CODIR owns collective/bridge/contraction materialization; ARTS owns
per-block single-writer DB/EDT/owner-map realization and grouped
compute/bridge/communication CUs; ARTS-RT lowers mechanically. Do not repair a
bad SDE layout in CODIR/ARTS, and do not repair missing CODIR collective intent
in ARTS-RT.

Use bundled helpers when they fit:
- `scripts/run-multinode-benchmark.sh` — rerun a benchmark with logs retained
- `scripts/inspect-distributed-ir.sh` — capture the key distributed pipeline stages
- `scripts/find-distributed-sites.sh` — grep the main ownership/routing codepaths
- `scripts/summarize-distributed-artifacts.sh` — locate logs and per-node counter JSONs

Read these before patching anything:
- `references/distributed-checklist.md`
- `references/distributed-codepaths.md`
- `../debug/references/failure-signatures.md`
- `../debug/references/codepath-map.md`
- `../debug/references/command-patterns.md`

## Triage Order

1. Confirm that single-node still works.
2. Reproduce with explicit node/thread/config inputs.
3. Confirm the requested transport. Single-node benchmark configs use TCP;
   multinode configs default to GASNet unless `--no-rdma` is set.
4. Check whether distribution is actually active in the failing path.
   It is default-on for multinode; `--no-distributed-db` forces the origin-node
   baseline, and distribution is stripped from single-node rows.
5. Inspect IR around:
   - `sde-planning`
   - `codir-to-arts`
   - `post-db-refinement`
   - `pre-lowering`
6. Check ownership constraints:
   - SDE layout facts are backed by an actual loop/layout transformation, not
     metadata that downstream must reinterpret
   - CODIR represents already-transformed SDE movement structure when a
     distributed edge requires communication
   - `distributed` marker present on eligible `DbAllocOp`
   - SDE/CODIR/ARTS materialized structure is present when required
   - DB/MU block grain and grouped CU/bridge grain are both sane; tiny DBs with
     one EDT each and coarse DBs that serialize independent writers are both
     failures to investigate
   - routed work and owner hints agree
7. Inspect runtime artifacts:
   - `arts.log`, `omp.log`
   - `cluster.json`, `n0.json`, `n1.json`, ...
8. If the bug reduces to wrong output rather than multi-node structure, hand off to `carts-miscompile-triage`.

## Common Commands

```bash
# Generate stage dumps with distributed ownership enabled (default-on for multinode)
dekk carts compile input.mlir --pipeline=post-db-refinement

# Multi-node benchmark run
dekk carts benchmarks run polybench/2mm \
  --size small \
  --nodes 2 \
  --threads 4 \
  --arts-config docker/arts-docker-2node.cfg \
  --debug 2
```

## Key Files

- `docs/heuristics/distribution.md`
- `lib/carts/dialect/arts/Transforms/db/DbOwnerMapRealization.cpp`
- `lib/carts/dialect/codir/Conversion/SdeToCodir/SdeToCodir.cpp`
- `lib/carts/dialect/codir/Conversion/CodirToArts/CodirToArts.cpp`
- `lib/carts/dialect/arts-rt/Conversion/ArtsRtToLLVM/ConvertArtsRtToLLVM.cpp`
- `lib/carts/codegen/Codegen.cpp`

## Validation

Rerun the distributed workload after every change, and compare against the single-node path before closing.
