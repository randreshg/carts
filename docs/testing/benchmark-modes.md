# Benchmark Testing Modes

This guide defines a repeatable 3-mode benchmark workflow:

1. Single node baseline
2. Multi-node without distributed DB flag
3. Multi-node with distributed DB flag (`--distributed-db`)

Use the same benchmark set and size in all three modes.

## Prerequisites

1. Build CARTS:
```bash
carts build
```

2. Pick a benchmark set:
```bash
# Smoke set (fast)
BENCH_SET="polybench/gemm polybench/2mm polybench/3mm polybench/atax polybench/bicg"

# Full suite (slower): unset BENCH_SET and run without benchmark args.
# This runs all benchmarks discovered by the harness.
# unset BENCH_SET
```

3. Pick dataset size:
```bash
# Supported sizes: small, medium, large, extralarge
# Note: "extra-large" is passed as: --size extralarge
SIZE=small
```

4. Prepare ARTS configs:
- Single-node config example: `configs/local.cfg`
- Multi-node config: use your cluster config (ssh/slurm/lsf) with `nodeCount > 1`

Minimal multi-node checklist:
- `launcher` matches your execution mode
- `nodeCount` matches intended run size
- `nodes` list is valid/reachable

5. Pick a counter profile (optional):
- `configs/profile-none.cfg` - counters OFF (baseline overhead)
- `configs/profile-timing.cfg` - timing-only counters
- `configs/profile-workload.cfg` - workload characterization counters
- `configs/profile-overhead.cfg` - full overhead counters
- `configs/profile-thread-edt.cfg` - thread-level EDT activity counters

Profiles live in: `configs/` (absolute path in this repo: `/opt/carts/configs/`)

Use a profile directly from benchmark runs (this triggers ARTS rebuild):
```bash
PROFILE=configs/profile-timing.cfg
```
Omit `--profile` if you want to reuse the currently built ARTS runtime as-is.

Shell note for multiline commands: keep `\` as the last character on each line.
If you copy/paste with `\ ` (backslash + space), the runner may receive blank
benchmark names.

6. For SSH multi-node runs, make sure your ARTS config lists all nodes:
```ini
# Example (in /path/to/multi-node.cfg)
launcher=ssh
nodeCount=6
nodes=node01,node02,node03,node04,node05,node06
```
The `nodes=` list must include every host that will participate in the run.

## Mode 1: Single-Node Baseline

Run on one node, no distributed DB flag.

```bash
carts benchmarks run ${BENCH_SET} \
  --size ${SIZE} \
  --nodes 1 \
  --timeout 240 \
  --arts-config configs/local.cfg \
  --launcher local \
  --profile "${PROFILE}" \
  --runs 1
```

Save the results path printed at the end.

Run all benchmarks (full suite):
```bash
carts benchmarks run \
  --size ${SIZE} \
  --nodes 1 \
  --timeout 240 \
  --arts-config configs/local.cfg \
  --launcher local \
  --profile "${PROFILE}" \
  --runs 1
```

Run all dataset sizes:
```bash
for SIZE in small medium large extralarge; do
  carts benchmarks run \
    --size "${SIZE}" \
    --nodes 1 \
    --timeout 240 \
    --arts-config configs/local.cfg \
    --launcher local \
    --profile "${PROFILE}" \
    --runs 1
done
```

Run all benchmarks with a thread sweep in one command:
```bash
carts benchmarks run \
  --size extralarge \
  --nodes 1 \
  --threads 4,8,16,32,64 \
  --timeout 240 \
  --arts-config configs/local.cfg \
  --launcher local \
  --profile configs/profile-workload.cfg \
  --runs 1
```

## Mode 2: Multi-Node (No Distributed DB Flag)

Run on multiple nodes with the same benchmark set, still without `--distributed-db`.
If your cluster launcher is not SSH, replace `--launcher ssh` accordingly.

```bash
carts benchmarks run ${BENCH_SET} \
  --size ${SIZE} \
  --nodes 6 \
  --timeout 240 \
  --arts-config /path/to/multi-node.cfg \
  --launcher ssh \
  --profile "${PROFILE}" \
  --runs 1
```

Save the results path printed at the end.

Run all benchmarks (full suite):
```bash
carts benchmarks run \
  --size ${SIZE} \
  --nodes 6 \
  --timeout 240 \
  --arts-config /path/to/multi-node.cfg \
  --launcher ssh \
  --profile "${PROFILE}" \
  --runs 1
```

Run all dataset sizes:
```bash
for SIZE in small medium large extralarge; do
  carts benchmarks run \
    --size "${SIZE}" \
    --nodes 6 \
    --timeout 240 \
    --arts-config /path/to/multi-node.cfg \
    --launcher ssh \
    --profile "${PROFILE}" \
    --runs 1
done
```

## Mode 3: Multi-Node (With Distributed DB Flag)

Run the same setup as Mode 2 but add distributed DB enablement.

```bash
carts benchmarks run ${BENCH_SET} \
  --size ${SIZE} \
  --nodes 6 \
  --timeout 240 \
  --arts-config /path/to/multi-node.cfg \
  --launcher ssh \
  --profile "${PROFILE}" \
  --arts-exec-args "--distributed-db" \
  --runs 1
```

Save the results path printed at the end.

Run all benchmarks (full suite):
```bash
carts benchmarks run \
  --size ${SIZE} \
  --nodes 6 \
  --timeout 240 \
  --arts-config /path/to/multi-node.cfg \
  --launcher ssh \
  --profile "${PROFILE}" \
  --arts-exec-args "--distributed-db" \
  --runs 1
```

Run all dataset sizes:
```bash
for SIZE in small medium large extralarge; do
  carts benchmarks run \
    --size "${SIZE}" \
    --nodes 6 \
    --timeout 240 \
    --arts-config /path/to/multi-node.cfg \
    --launcher ssh \
    --profile "${PROFILE}" \
    --arts-exec-args "--distributed-db" \
    --runs 1
done
```

Run one mode across all built-in profiles:
```bash
for PROFILE in \
  configs/profile-none.cfg \
  configs/profile-timing.cfg \
  configs/profile-workload.cfg \
  configs/profile-overhead.cfg \
  configs/profile-thread-edt.cfg; do
  carts benchmarks run ${BENCH_SET} \
    --size ${SIZE} \
    --nodes 6 \
    --timeout 240 \
    --arts-config /path/to/multi-node.cfg \
    --launcher ssh \
    --profile "${PROFILE}" \
    --arts-exec-args "--distributed-db" \
    --runs 1
done
```

## GEMM Example

Single node:
```bash
carts benchmarks run polybench/gemm \
  --size extralarge \
  --nodes 1 \
  --timeout 240 \
  --arts-config configs/local.cfg \
  --launcher local \
  --runs 1
```

Multi-node (no distributed DB flag):
```bash
carts benchmarks run polybench/gemm \
  --size extralarge \
  --nodes 6 \
  --timeout 240 \
  --arts-config /path/to/multi-node.cfg \
  --launcher ssh \
  --runs 1
```

Multi-node (distributed DB enabled):
```bash
carts benchmarks run polybench/gemm \
  --size extralarge \
  --nodes 6 \
  --timeout 240 \
  --arts-config /path/to/multi-node.cfg \
  --launcher ssh \
  --arts-exec-args "--distributed-db" \
  --runs 1
```

GEMM multi-node with workload counters (distributed DB enabled):
```bash
carts benchmarks run polybench/gemm \
  --size extralarge \
  --nodes 6 \
  --timeout 240 \
  --arts-config /path/to/multi-node.cfg \
  --launcher ssh \
  --profile configs/profile-workload.cfg \
  --arts-exec-args "--distributed-db" \
  --trace \
  --runs 1
```

Counter outputs for this run are written under:
- `external/carts-benchmarks/results/<timestamp>/polybench/gemm/<threads>t_<nodes>n/run_1/counters/`

## Compare Results

For each run, check:
- pass/fail counts
- checksum correctness (`YES`)
- skipped benchmark set (should be consistent across Mode 2 and Mode 3 unless behavior changed)

## Recommended Pass Criteria

- Mode 1 succeeds for all expected benchmarks.
- Mode 2 and Mode 3 both succeed for all expected benchmarks.
- Checksums remain correct in all modes.
- Any difference between Mode 2 and Mode 3 is understood and intentional.
