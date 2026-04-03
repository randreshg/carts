---
name: carts-runtime-triage
description: Debug CARTS runtime failures in generated ARTS executables, including hangs, deadlocks, crashes, missing EDT completion, epoch stalls, distributed ownership issues, and counter anomalies. Use when compilation succeeds but the produced program fails only at runtime.
user-invocable: true
allowed-tools: Bash, Read, Write, Grep, Glob, Agent
argument-hint: [<binary | benchmark-path>]
---

# CARTS Runtime Triage

Goal: classify the runtime failure before changing compiler code.

Use bundled helpers when they fit:
- `scripts/build-arts-debug.sh` — rebuild ARTS at a chosen debug level
- `scripts/build-arts-counters.sh` — rebuild ARTS with a chosen counters level
- `scripts/locate-runtime-artifacts.sh` — find logs and counter JSONs under a run directory
- `scripts/find-runtime-codepaths.sh` — grep the main runtime/lowering codepaths

Read these before changing code:
- `references/runtime-checklist.md`
- `../debug/references/failure-signatures.md`
- `../debug/references/codepath-map.md`
- `../debug/references/command-patterns.md`

## Triage Order

1. Confirm the environment and rebuild the relevant bits.
   - `carts doctor`
   - `carts build`
   - `carts build --arts` if the runtime may be stale
2. Classify the symptom.
   - crash / assertion / sanitizer
   - hang / deadlock / no progress
   - slow execution / regression
   - distributed-only failure
3. Switch ARTS to a debug build for runtime inspection.

```bash
carts build --arts --debug 3
```

Level 3 is for debugging only. Rebuild release ARTS before performance conclusions.

4. For performance or progress issues, collect counters.

```bash
carts build --arts --counters 1
carts build --arts --counters 2
carts build --arts --counters 3
```

5. Check the runtime structure that should exist.
   - epochs: `arts.create_epoch`, `arts.wait_on_epoch`
   - task completion / continuation edges
   - distributed DB ownership, routes, and owner-local init behavior
6. If the failure smells like compiler-lowered structure rather than runtime behavior, hand off to `carts-miscompile-triage`.
7. If the failure is specific to multi-node ownership or distributed routing, hand off to `carts-distributed-triage`.

## Distributed-Specific Checks

- Verify whether the DB is marked `distributed`
- Check route selection and owner assumptions in `ConvertArtsToLLVM.cpp`
- Check distributed init / worker-local creation in `Codegen.cpp`
- Compare single-node vs distributed runs before assuming a general runtime bug

## Key Docs

- `docs/heuristics/distribution.md`
- `docs/heuristics/partitioning.md`
- `AGENTS.md` section: Distributed Runtime Debug
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`
- `lib/arts/passes/transforms/ForLowering.cpp`
- `lib/arts/analysis/edt/`

## Validation

Always rerun the failing executable after changing build mode or instrumentation, and again after reverting to release mode.
