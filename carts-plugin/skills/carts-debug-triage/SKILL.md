---
name: carts-debug-triage
description: Use when debugging crashes, miscompiles, or runtime issues in CARTS, or when tracing MLIR dialect ops (SDE, ARTS, ARTS-RT).
---

# CARTS Debug & Triage

- **No Hardcoded Mappings**: Find the truth in the code.
- **Trace Operations Dynamically**: Use `grep_search` across `lib/carts/` and `include/carts/` to trace an operation's lifecycle (look for creation with `builder.create`, transformation with `isa<>`, or erasure with `replaceOp`).
- **Dialect Definitions**: Look up dialect definitions and invariants dynamically in `include/carts/dialect/*/IR/*.td`.
- **Pipeline Boundaries**: Trace boundary verifiers between SDE, ARTS, and ARTS-RT to find where the pipeline fails.
