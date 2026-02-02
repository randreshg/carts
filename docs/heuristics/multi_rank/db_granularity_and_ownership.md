# Multi-rank heuristics: DB granularity and ownership

This note is scoped to **multi-rank** runs (distributed memory or multiple
ranks per node). It focuses on DB granularity, ownership, and the resulting
communication patterns.

Primary audiences:
- CARTS/ARTS compiler-pass authors (MLIR passes)
- ARTS runtime authors (DB memory model / CDAG)
- performance engineers tuning distributed runs

## What changes with multiple ranks

With multiple ranks, DB ownership and data movement dominate performance:
- **Remote acquires** trigger network traffic or owner handoff.
- **Owner updates** on WRITE introduce latency and contention.
- **Full DB transfers** are expensive when payloads are large.

These effects are driven by DB layout and the access pattern that CARTS
recognizes (block, stencil, fine-grained, or coarse).

## Practical heuristics (multi-rank)

1. **Prefer block partitioning for writable arrays.**
   Distinct DBs per rank reduce cross-rank write contention.

2. **Avoid coarse layouts for hot write paths.**
   A single DB forces all writers through one owner, which serializes progress.

3. **Minimize full-range acquires.**
   Full-range acquires pull all blocks even when each EDT only touches its
   local slice, amplifying traffic.

4. **Keep ownership aligned with computation.**
   If possible, partition so that a rank mostly accesses DBs it owns.

5. **Stencils: use stencil/block partitioning with halos.**
   This keeps communication bounded while preserving correctness at boundaries.

6. **Balance DB count vs. network overhead.**
   Too many DBs increases metadata traffic; too few increases payload size per
   transfer. Tune block sizes to match the communication granularity of the
   algorithm.

For partitioning mechanics and mode definitions, see
`docs/heuristics/partitioning/partitioning.md`.
