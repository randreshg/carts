# Failure Signatures

Use this table to decide which debugging skill should own the first pass.

| Symptom | First Owner | Why |
|---|---|---|
| wrong checksum / wrong output / semantic drift | `carts-miscompile-triage` | semantic equivalence failed |
| hang / timeout after compile succeeded | `carts-runtime-triage` | runtime structure or progress issue |
| only fails with `--distributed-db` or multi-node | `carts-distributed-triage` | ownership or routing path is special |
| behavior changes with pass order or `--start-from` | `carts-analysis-triage` | stale analysis or invalidation likely |
| huge failing case needs a lit/C/MLIR reduction | `carts-reproducer` | preserve the symptom while shrinking |
| benchmark runner failure or suspicious speedup | `carts-benchmark-triage` | harness + compiler split first |

## Escalation Rules

- If OpenMP baseline is also wrong, stop blaming CARTS until that is resolved.
- If single-node passes and multi-node fails, switch to distributed triage immediately.
- If the first bad artifact is already IR, stay in compiler-side triage rather than runtime triage.
- If a bug disappears after broad invalidation or graph rebuild, suspect analysis freshness before changing heuristics.
