# Debug Command Patterns

Reuse these before improvising.

## Pipeline Introspection

```bash
carts pipeline
carts pipeline --json
carts pipeline --pipeline=db-partitioning
```

## Stage Dump

```bash
carts compile input.mlir --pipeline=db-partitioning
carts compile input.mlir --all-pipelines -o outdir/
```

## Diagnose Bundle

```bash
carts compile input.mlir --diagnose --diagnose-output diagnose.json
```

## ARTS Debug Build

```bash
carts build --arts --debug 3
carts build --arts --counters 2
```

## Benchmark Triage

```bash
carts triage-benchmark suite/name --size small --threads 2
```

## Multi-Node Benchmark

```bash
carts benchmarks run polybench/2mm \
  --size small \
  --nodes 2 \
  --threads 4 \
  --arts-config docker/arts-docker-2node.cfg \
  --debug 2
```
