# LULESH Task Guide (Updated Pipeline)

This note tracks how to inspect LULESH through the current CARTS distribution + partitioning pipeline.

## Scope

- Focus: compiler pipeline behavior, not runtime model redesign.
- Uses current stages:
  - `concurrency` (stage 10)
  - `edt-distribution` (stage 11)
  - `concurrency-opt` (stage 12)
- Uses only active distribution and partitioning contracts.

## Pipeline Checkpoints

### 1) After `create-dbs` (stage 7)

```bash
carts compile lulesh.mlir --stop-at=create-dbs -o lulesh_create_dbs.mlir
```

What to inspect:
- `arts.db_alloc` and `arts.db_acquire` are present.
- Initial partition intent is attached through typed attributes.
- No distribution strategy has been selected yet.

### 2) After `concurrency` (stage 10)

```bash
carts compile lulesh.mlir --stop-at=concurrency -o lulesh_concurrency.mlir
```

What to inspect:
- `arts.for` loops are preserved and annotated by `ArtsForOptimization`.
- Access-pattern analysis is derived from shared DB/EDT analysis APIs.

### 3) After `edt-distribution` (stage 11)

```bash
carts compile lulesh.mlir --stop-at=edt-distribution -o lulesh_edt_distribution.mlir
```

What to inspect:
- `EdtDistributionPass` has selected strategy attributes on `arts.for`:
  - `distribution_kind`
  - `distribution_pattern`
  - `distribution_version`
- `ForLowering` has lowered loops using strategy helpers (`EdtTaskLoopLowering`) and acquire planning (`AcquireRewritePlanning`).
- Strategy logic is not centralized in `ForLowering`.

### 4) After `concurrency-opt` (stage 12)

```bash
carts compile lulesh.mlir --stop-at=concurrency-opt -o lulesh_concurrency_opt.mlir
```

What to inspect:
- `DbPartitioning` runs as orchestration.
- Mode-specific logic is delegated through planner/arbiter/lowering contracts:
  - `DbPartitionPlanner`
  - `DbPartitionDecisionArbiter`
  - `StencilBoundsLowering`
- Final allocation/acquire shapes are visible in lowered DB ops.

## Recommended Debug Commands

```bash
# Distribution decisions
carts compile lulesh.mlir --stop-at=edt-distribution \
  --debug-only=edt_distribution,for_lowering 2>&1 | tee lulesh_dist.log

# Partitioning decisions
carts compile lulesh.mlir --stop-at=concurrency-opt \
  --debug-only=db_partitioning,db 2>&1 | tee lulesh_partition.log
```

## Multi-node Verification

Use docker config and benchmark runner:

```bash
ARTS_CFG=/opt/carts/docker/arts-docker.cfg \
  carts benchmarks run lulesh --size small --arts-exec-args "--partition-fallback=fine"
```

Check logs/counters:
- rank 0 and remote ranks should all show non-zero task/data activity for distributed runs.
- if using distributed DB ownership, verify with `--distributed-db-ownership` and compare per-node memory behavior.

## Known LULESH Behavior

- LULESH has indirect access patterns (`nodelist`-style gather/scatter), so fallback policy can matter.
- Use `--partition-fallback=fine` when you want non-affine paths to avoid coarse-only fallback.
- Validate checksum equivalence between fallback modes before comparing performance.

## Minimal Validation Script

```bash
# 1) Build
ninja -C build carts-compile

# 2) Stage dumps
carts compile lulesh.mlir --stop-at=concurrency -o /tmp/lulesh_concurrency.mlir
carts compile lulesh.mlir --stop-at=edt-distribution -o /tmp/lulesh_edt_distribution.mlir
carts compile lulesh.mlir --stop-at=concurrency-opt -o /tmp/lulesh_concurrency_opt.mlir

# 3) Local correctness
carts benchmarks run lulesh --size small

# 4) Multi-node correctness/distribution
ARTS_CFG=/opt/carts/docker/arts-docker.cfg \
  carts benchmarks run lulesh --size small --arts-exec-args "--partition-fallback=fine"
```

## References

- `docs/heuristics/distribution/distribution.md`
- `docs/heuristics/partitioning/partitioning.md`
- `docs/agents.md`
