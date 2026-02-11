# CARTS Examples: Analysis and Debug Guide

This is the single analysis guide for `tests/examples`.

All previous per-example `docs/analysis.md` files were removed to avoid drift.

## 1) Build and test prerequisites

```bash
ninja -C build carts-run
python3 tools/carts_cli.py check -s all
```

## 2) Run an example

Pick any example source and run through CARTS. Example:

```bash
carts execute tests/examples/matrixmul/matrixmul.cpp -O3
```

For multi-node experiments:

```bash
ARTS_CFG=/opt/carts/docker/arts-docker.cfg carts execute tests/examples/matrixmul/matrixmul.cpp -O3
```

## 3) Inspect pipeline stages

Use `carts-run` with stop points:

```bash
# Before distribution selection
carts-run <file>.mlir --O3 --arts-config /opt/carts/docker/arts-docker.cfg --stop-at concurrency

# After distribution selection + for lowering
carts-run <file>.mlir --O3 --arts-config /opt/carts/docker/arts-docker.cfg --stop-at edt-distribution

# After DB partitioning and concurrency optimizations
carts-run <file>.mlir --O3 --arts-config /opt/carts/docker/arts-docker.cfg --stop-at concurrency-opt
```

## 4) Debug channels

```bash
# Distribution selection + lowering decisions
carts-run <file>.mlir --O3 --arts-config /opt/carts/docker/arts-docker.cfg --debug-only=edt_distribution,for_lowering

# DB partitioning decisions/planning
carts-run <file>.mlir --O3 --arts-config /opt/carts/docker/arts-docker.cfg --debug-only=db_partitioning,db_partition_decision_arbiter

# Distributed DB ownership marking/lowering (if enabled)
carts-run <file>.mlir --O3 --arts-config /opt/carts/docker/arts-docker.cfg --distributed-db-ownership --debug-only=distributed_db_ownership
```

## 5) What to expect in IR

At `--stop-at concurrency`:
- no `distribution_*` attributes yet

At `--stop-at edt-distribution`:
- `distribution_kind`
- `distribution_pattern`
- `distribution_version`

At `--stop-at concurrency-opt`:
- DB allocations/acquires rewritten with chosen `partition_mode`
- block/stencil/fine-grained rewrite shape depending on access pattern and mode

When `--distributed-db-ownership` is enabled:
- eligible `DbAllocOp`s are marked with `distributed`
- lowering uses per-block route assignment instead of forcing route 0

## 6) Runtime sanity checks

For distributed runs, verify:
- non-zero activity on remote nodes (tasks/bytes/counters)
- expected strategy mapping by workload shape:
  - matmul-like loops -> `tiling_2d`
  - triangular/non-uniform loops -> `block_cyclic`
  - stencil loops -> `block` + stencil-aware rewriting

## 7) Troubleshooting

If compilation fails early with config errors:
- pass `--arts-config <path>` explicitly, or
- provide a valid `arts_config` in the working directory

If strategy attributes are missing:
- confirm `--concurrency`/`-O3` pipeline path reaches `edt-distribution`
- inspect with `--stop-at edt-distribution`

If partitioning stays coarse unexpectedly:
- inspect `db_partitioning` debug output for capability/mode reconciliation
- check whether acquires are valid and carry partition hints/derivable bounds
