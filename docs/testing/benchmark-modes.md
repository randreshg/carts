# Benchmark Testing Modes

This guide defines a repeatable 3-mode benchmark workflow:

1. Single node baseline
2. Multi-node without distributed DB flag
3. Multi-node with distributed DB ownership enabled (`--compile-args "--distributed-db"`)

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
- Single-node config example: `external/carts-benchmarks/configs/local.cfg`
- Multi-node config: use your cluster config (ssh/slurm/lsf) with `node_count > 1`

Minimal multi-node checklist:
- `launcher` matches your execution mode
- `node_count` matches intended run size
- `nodes` list is valid/reachable

5. Pick a counter profile (optional):
- `profile-none.cfg` - counters OFF (baseline overhead)
- `profile-timing.cfg` - timing-only counters
- `profile-workload.cfg` - workload characterization counters
- `profile-overhead.cfg` - full overhead counters
- `profile-thread-edt.cfg` - thread-level EDT activity counters

Profiles live in: `external/carts-benchmarks/configs/profiles/`. You can pass just the filename (e.g. `profile-timing.cfg`) and auto-resolution will find it.

Use a profile directly from benchmark runs (this triggers ARTS rebuild):
```bash
PROFILE=profile-timing.cfg
```
Omit `--profile` if you want to reuse the currently built ARTS runtime as-is.

Shell note for multiline commands: keep `\` as the last character on each line.
If you copy/paste with `\ ` (backslash + space), the runner may receive blank
benchmark names.

6. For SSH multi-node runs, make sure your ARTS config lists all nodes:
```ini
# Example (in /path/to/multi-node.cfg)
launcher=ssh
node_count=6
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
  --arts-config local.cfg \
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
  --arts-config local.cfg \
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
    --arts-config local.cfg \
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
  --arts-config local.cfg \
  --launcher local \
  --profile profile-workload.cfg \
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

Run the same setup as Mode 2 but add distributed DB ownership enablement.

Important:
- Multi-node GEMM is distributed in both Mode 2 and Mode 3. The compiler still
  emits internode `tiling_2d` EDT distribution for the matmul epoch without the
  flag.
- `--compile-args "--distributed-db"` changes DB ownership and host-side
  initialization, not whether the GEMM work itself is distributed.
- For GEMM today, the flag marks the large `A` and `C` allocations as
  `distributed`; `B` remains coarse because it is a single-block read-only
  allocation under the current ownership policy.

```bash
carts benchmarks run ${BENCH_SET} \
  --size ${SIZE} \
  --nodes 6 \
  --timeout 240 \
  --arts-config /path/to/multi-node.cfg \
  --launcher ssh \
  --profile "${PROFILE}" \
  --compile-args "--distributed-db" \
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
  --compile-args "--distributed-db" \
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
    --compile-args "--distributed-db" \
    --runs 1
done
```

Run one mode across all built-in profiles:
```bash
for PROFILE in \
  profile-none.cfg \
  profile-timing.cfg \
  profile-workload.cfg \
  profile-overhead.cfg \
  profile-thread-edt.cfg; do
  carts benchmarks run ${BENCH_SET} \
    --size ${SIZE} \
    --nodes 6 \
    --timeout 240 \
    --arts-config /path/to/multi-node.cfg \
    --launcher ssh \
    --profile "${PROFILE}" \
    --compile-args "--distributed-db" \
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
  --arts-config local.cfg \
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

Multi-node (distributed DB ownership enabled):
```bash
carts benchmarks run polybench/gemm \
  --size extralarge \
  --nodes 6 \
  --timeout 240 \
  --arts-config /path/to/multi-node.cfg \
  --launcher ssh \
  --compile-args "--distributed-db" \
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
  --profile profile-workload.cfg \
  --compile-args "--distributed-db" \
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
- GEMM should show internode distributed matmul execution in both Mode 2 and
  Mode 3
- Mode 3 should additionally use distributed DB ownership for eligible GEMM
  allocations (`A` and `C` today)

## Recommended Pass Criteria

- Mode 1 succeeds for all expected benchmarks.
- Mode 2 and Mode 3 both succeed for all expected benchmarks.
- Checksums remain correct in all modes.
- Any difference between Mode 2 and Mode 3 is understood and intentional.
  For GEMM, expected differences are DB ownership/init behavior and possibly
  performance; task distribution itself should remain internode in both modes.
