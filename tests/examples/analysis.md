# CARTS Examples: Analysis Guide

For pipeline stages, debug commands, and troubleshooting, see the main guide:

- **[/opt/carts/docs/agents.md](/opt/carts/docs/agents.md)**

## Running an Example

```bash
# Full compilation
carts compile tests/examples/matrixmul/matrixmul.cpp -O3

# Inspect pipeline stage
carts compile <file>.mlir --pipeline concurrency-opt

# Debug a specific pass
carts compile <file>.mlir --pipeline concurrency-opt --debug-only=db_partitioning 2>&1
```

For multi-node experiments, pass `--arts-config` at compile time — the config is embedded in the binary.
