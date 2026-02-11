# CARTS Distribution Framework

## Purpose

This document is the technical reference for CARTS H2 distribution:
- runtime capability constraints from ARTS
- algorithm compatibility and tradeoffs
- pattern classification ownership (analysis API)
- selected lowering architecture and pipeline integration

H2 decides *work distribution* and task loop structure.
H1 partitioning still decides DB layout/rewrite details.

Related guide:
- `docs/heuristics/partitioning/partitioning.md`

## 0. Motivation: Bridging AMT, OpenMP, and MPI

CARTS bridges three major parallel programming models, combining the strengths
of each while avoiding their individual limitations:

| Model | Data Distribution | Computation Distribution | Programmer Burden |
|-------|------------------|-------------------------|-------------------|
| **OpenMP** | Shared memory (single node) | Implicit (pragmas) | Low |
| **MPI** | Explicit per-node allocation | Explicit message passing | High |
| **AMT (ARTS)** | Was: single-node allocation | Task-based + work-stealing | Medium |
| **CARTS** | Compiler-managed distributed | Compiler-managed task distribution | Low (OpenMP-like source) |

**The problem.** OpenMP-style source code expresses parallelism through simple
loop annotations, but the memory model is restricted to a single shared address
space. MPI achieves multi-node memory scaling through explicit per-node
allocation and message passing, but at the cost of significant programmer effort.
ARTS (the underlying AMT runtime) provides dynamic task scheduling, work-stealing,
and task migration across nodes, but its programming model still requires manual
data placement decisions.

**What CARTS achieves.** The compiler takes OpenMP-annotated C source and
automatically:

1. **Distributes memory like MPI** — each node allocates only its portion of
   data (via `DistributedDbOwnershipPass` and round-robin route selection in
   `ConvertArtsToLLVM`). Memory capacity scales with node count.
2. **Distributes computation like AMT** — tasks are routed to nodes based on
   data ownership, with work-stealing for dynamic load balancing. No manual
   message passing required.
3. **Preserves OpenMP-like source** — the programmer writes standard parallel
   loops. Distribution strategy, data placement, and communication are all
   compiler-managed decisions.

This positions CARTS as a practical bridge: the ease of OpenMP, the scalability
of MPI, and the runtime flexibility of AMT — all derived from the same source
annotation level.

## 1. ARTS Runtime Capability Summary

### 1.1 Memory and ownership model

| Capability | Status | Implication |
|---|---|---|
| DB owner node fixed by GUID | Yes | owner-computes is natural for writes |
| Single writer per generation | Yes | no concurrent multi-writer update of one DB |
| Read sharing | Yes | multiple readers can acquire concurrently |
| Partial read dependency (`artsRecordDepAt`) | Yes | stencil/halo exchanges are efficient |
| Partial point-to-point put (`artsPutInDb`) | Yes | targeted transfer works |
| Partial write-back of modified DB | No (disabled) | write release remains full DB cost |

### 1.2 EDT execution model

| Capability | Status | Implication |
|---|---|---|
| Route-based EDT placement | Yes | static first placement is controllable |
| EDT migration | Yes | runtime can relocate work |
| Work stealing (intra + inter) | Yes | dynamic balance exists but static mapping still matters |
| Active messages (`artsActiveMessageWithDb`) | Yes | compute-to-data patterns are feasible |
| Dynamic route table update | Yes | signals follow migrated EDTs |

### 1.3 Practical constraints for distribution

- Keep write ownership simple (prefer owner-computes).
- Avoid expensive remote write-back when possible.
- Use read-only fan-out for shared inputs when possible.
- Expect no native high-level collectives; compose from EDT/event primitives.

## 2. Algorithm Compatibility (Research)

### 2.1 Compatibility matrix

| Algorithm | ARTS fit | Current status in CARTS |
|---|---|---|
| Block | Strong | Implemented baseline |
| Two-level block (internode) | Strong | Implemented default internode path |
| Block-cyclic | Good | Implemented task-loop lowering path |
| 2D block tiling (matmul-oriented) | Good | Implemented strategy + task loop striping + owner hints |
| Cannon-style shifts | Feasible but complex | Not implemented (future) |
| SUMMA-style broadcast panels | Feasible via events/active messages | Not implemented (future) |
| 2.5D replication | Possible but high complexity | Not implemented (future) |
| Stencil halo | Strong | Implemented via block + halo-aware rewrite |

### 2.2 Why CARTS currently prefers 2D tiling over Cannon/SUMMA

For current compiler/runtime integration:
- 2D tiling keeps ownership decisions explicit and local to task/DB hints.
- It avoids multi-phase shift orchestration required by Cannon.
- It avoids introducing new broadcast protocol machinery needed by SUMMA.

Cannon and SUMMA remain viable future paths once collective-like orchestration is first-class in lowering/runtime APIs.

## 3. Strategy Selection Policy (H2)

Current policy is implemented in
`DistributionHeuristics::selectDistributionKind` (`lib/arts/Analysis/DistributionHeuristics.cpp`).

1. If EDT concurrency is internode: `two_level`
2. Otherwise (intranode):
   - `triangular` -> `block_cyclic`
   - `matmul` -> `tiling_2d`
   - `stencil`/`uniform`/`unknown` -> `block`

## 4. Pattern Detection Ownership

Pattern detection is centralized in analysis APIs, not in lowering passes.

### 4.1 DB-backed source of truth

- `DbAnalysis::analyzeLoopDbAccessPatterns(ForOp)` computes loop summary from DB graph + access bounds.
- `DbAnalysis::getLoopDistributionPattern(ForOp)` exposes pattern.
- `ArtsAnalysisManager::getLoopDistributionPattern(Operation *)` is the unified pass-facing API.

### 4.2 EDT-facing view

- `EdtAnalysis` consumes DB-derived loop summaries for EDT-level reporting.
- Passes do not need to rescan memops directly to classify loop patterns.

### 4.3 Access-pattern unification

- Shared utility: `AccessPatternAnalysis` (`include/arts/Analysis/AccessPatternAnalysis.h`, `lib/arts/Analysis/AccessPatternAnalysis.cpp`)
- DB graph nodes and DB analysis both use the same bounds logic.

## 5. Pipeline Architecture

Distribution is now a dedicated stage:

- `concurrency`: prepares parallel structure
- `edt-distribution`: runs `EdtDistributionPass`, annotates distribution attrs, then `ForLowering`
- `concurrency-opt`: DB partitioning and downstream optimization

Key files:
- `tools/run/carts-run.cpp`
- `lib/arts/Passes/Transformations/EdtDistributionPass.cpp`

Useful stop points:

```bash
carts-run input.mlir --stop-at=concurrency
carts-run input.mlir --stop-at=edt-distribution
carts-run input.mlir --stop-at=concurrency-opt
```

## 6. Distributed DB Ownership

Goal:
- distribute eligible host-level DB allocations across nodes so memory scales
  with node count.

Current implementation:
- `DbAllocOp` supports a `distributed` marker attribute.
- New pass: `DistributedDbOwnershipPass`
  (`lib/arts/Passes/Optimizations/DistributedDbOwnership.cpp`).
- Pipeline placement: `DbPartitioning -> DistributedDbOwnership -> DbPass`
  (gated by `--distributed-db-ownership` in `carts-run`).
- Lowering support: `ConvertArtsToLLVM` uses round-robin route selection for
  marked multi-DB allocations:
  - route = `linearIndex % artsGetTotalNodes()`
  - unmarked allocations keep the existing route behavior.

Current eligibility policy is intentionally conservative:
- allocation is host-level (outside `arts.edt`)
- not `DbAllocType::global`
- has multiple DB blocks
- handle uses are restricted to DB dependency flow (`db_acquire`, `db_free`,
  EDT dependency wiring)
- allocation is consumed by internode EDTs

Distributed init split is implemented:
- `ArtsCodegen.cpp` generates a `distributed_db_init` callback that runs on ALL
  nodes inside `initPerNode`. Each node reserves the same deterministic GUID
  sequence but only allocates DBs whose GUID encodes its rank.
- `ConvertArtsToLLVM.cpp` lowers marked multi-DB allocations with
  `route = linearIndex % artsGetTotalNodes()` and guards `artsDbCreateWithGuid`
  behind `artsIsGuidLocal`.

Future work:
- distributed free policy beyond current `artsShutdown()` behavior
- writer-aware ownership (align DB route with writer EDT node)
- broader eligibility (stencil arrays, global initializers with distributed init EDTs)

## 7. IR Contract

`EdtDistributionPass` annotates top-level `arts.for` in parallel EDT regions with:

- `distribution_kind` (`#arts.distribution_kind<...>`)
- `distribution_pattern` (`#arts.distribution_pattern<...>`)
- `distribution_version = 1`

These attributes are consumed by `ForLowering` and propagated to generated epoch/task EDT operations.

## 8. Loop Transform Compatibility (R8)

Question: do loop normalization/reordering transforms harm multi-node distribution?

Current answer: **no for existing 1D outer-loop distribution path**.

Reason:
- Stage 5-6 transforms mostly target inner serial `scf.for` structure.
- Distribution stage (`edt-distribution`) runs later and preserves top-level `arts.for` distribution contract.

Pass-level summary (current behavior):
- Loop normalization (triangular-to-rectangular forms): compatible
- Loop reordering/interchange: compatible for current outer-loop distribution
- Matmul inner tiling/reduction reshaping: compatible with current H2 selection
- Loop fusion with matched bounds: compatible if resulting top-level loop semantics remain equivalent

Future caveat:
- If distribution expands to full 2D outer-loop ownership transforms, some inner-loop tiling/reordering choices may need explicit coordination metadata.

## 9. Lowering Architecture

`ForLowering` now orchestrates strategy-specific helpers instead of owning all logic inline.

### 9.1 Acquire rewriting helpers

- `EdtRewriter` abstraction:
  - `BlockEdtRewriter`
  - `StencilEdtRewriter`
- Strategy-aware planning extracted into:
  - `AcquireRewritePlanning` (`include/arts/Transforms/Edt/AcquireRewritePlanning.h`, `lib/arts/Transforms/Edt/AcquireRewritePlanning.cpp`)

### 9.2 Task loop lowering helpers

- `EdtTaskLoopLowering` abstraction:
  - `BlockTaskLoopLowering`
  - `BlockCyclicTaskLoopLowering`
  - `Tiling2DTaskLoopLowering`
- Strategy helpers now also own acquire-window planning through
  `EdtTaskLoopLowering::planAcquireRewrite(...)` so `ForLowering` does not
  embed strategy-specific `acquire_offset/size/hint` branching.

### 9.3 Partitioning integration for 2D owner hints

`DbPartitioning` consumes tiling-2D owner hints on writable (`inout`) task acquires to force N-D block ownership where valid.

## 10. Heuristics Placement

`DistributionHeuristics` now centralizes:
- worker topology resolution (`resolveWorkerConfig`)
- dispatch worker count / total worker helpers
- DB alignment block-size heuristic
- coarsened block-size hint computation for `arts.for`

Passes consume the API; they do not duplicate these heuristics.

## 11. Validation Checklist

```bash
ninja -C build carts-run

# Missing config must fail fast
carts-run input.mlir --stop-at=concurrency

# Inspect distribution attributes
carts-run gemm.mlir --O3 --arts-config arts.cfg --stop-at=edt-distribution

# Multi-node counters
# (example harness depends on your local benchmark setup)
```

Expected checks:
- no `distribution_*` attrs at `--stop-at=concurrency`
- `distribution_kind/pattern/version` attrs at `--stop-at=edt-distribution`
- checksums unchanged for benchmark kernels
- node/thread counters show non-zero work on remote nodes for distributed runs

## 12. Compiler-System Design Principles (Survey Notes)

Practical principles adopted from systems like Halide/Legion/Chapel/HPF/GSPMD/Charm++:

- Separate policy from lowering: distribution plan first, IR rewrite second.
- Keep strategy catalog extensible through enums + specialized lowerers.
- Keep correctness independent from mapper choice; mapper changes should be performance policy.
- Prefer analysis-backed decisions over pass-local ad-hoc classification.

In CARTS, this maps to:
- `EdtDistributionPass` for policy annotation
- specialized task/acquire lowering components for execution
- DB/EDT analysis APIs as single pattern source of truth
