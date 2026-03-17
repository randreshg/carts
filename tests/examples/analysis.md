# CARTS Examples: Analysis Guide

For pipeline steps, debug commands, and troubleshooting, see the main guide:

- `docs/agents.md`

## Running an Example

```bash
# Full compilation
carts compile tests/examples/matrixmul/matrixmul.cpp -O3

# Inspect pipeline step
carts compile <file>.mlir --pipeline=concurrency-opt

# Debug a specific pass
carts compile <file>.mlir --pipeline=concurrency-opt --arts-debug=db_partitioning 2>&1
```

For multi-node experiments, pass `--arts-config` at compile time — the config is embedded in the binary.
