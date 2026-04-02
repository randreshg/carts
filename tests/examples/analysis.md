# CARTS Examples: Analysis Guide

For pipeline steps, debug commands, and troubleshooting, see:

- `AGENTS.md`
- `docs/compiler/pipeline.md`

## Running an Example

```bash
# Full compilation
carts compile tests/examples/matrixmul/matrixmul.cpp -O3

# Inspect DB partitioning
carts compile <file>.mlir --pipeline=db-partitioning

# Debug a specific pass
carts compile <file>.mlir --pipeline=db-partitioning --arts-debug=db_partitioning 2>&1
```

For multi-node experiments, pass `--arts-config` at compile time — the config is embedded in the binary.
