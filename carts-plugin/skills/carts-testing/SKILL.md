---
name: carts-testing
description: Use when running lit tests, benchmarks, or creating reproducers.
---

# CARTS Testing & Benchmarks

- **Lit Tests**: Run standard `lit` tests to verify compiler passes. Use `grep_search` in `test/` to find examples.
- **Benchmarks**: Use `dekk carts benchmarks ...` for executing benchmarks.
- **Repro Cases**: When creating a reproducer, isolate the minimal MLIR or C code that triggers the failure.
