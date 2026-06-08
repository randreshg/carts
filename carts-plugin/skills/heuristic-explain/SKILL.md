---
name: carts-heuristic-explain
description: Use when a benchmark or test has unexpected partitioning, wrong distribution mode, or heuristic drift in ARTS DB/EDT placement decisions.
user-invocable: true
allowed-tools: Bash, Read, Grep, Glob, Agent
argument-hint: [partition <file> | distribution <file> | thresholds]
parameters:
  - name: input_file
    type: str
    gather: "Path to MLIR or C input file to analyze"
  - name: focus
    type: str
    gather: "What to explain: 'partition' (DB partitioning), 'distribution' (EDT strategy), or 'both'"
---

# CARTS Heuristic Decision Explainer

## Purpose

Trace and explain why the compiler chose a specific partitioning mode or
distribution strategy. The live compiler no longer has a monolithic
partitioning-heuristic pass; `DbHeuristics` records decisions while
`DbAnalysis`, `DbLayoutPlanUtils`, and the DB refinement passes own the
evidence and rewrites.

Use [[carts-vision]] for placement questions. Heuristics explain structural
evidence; they must not become benchmark-name policy or downstream
recomputation of SDE/CODIR facts.

## Quick Diagnostic Commands

```bash
# Dump IR at partitioning stage to see partition modes
dekk carts compile <file> --pipeline=post-db-refinement 2>/dev/null | grep 'partition_mode'

# Dump IR after SDE planning/materialization to see distribution strategy
dekk carts compile <file> --pipeline=sde-planning 2>/dev/null | grep 'distribution_kind'

# Enable debug output for partitioning decisions
dekk carts compile <file> --pipeline=post-db-refinement --arts-debug=db_transforms 2>&1

# Enable debug output for DB mode/refinement decisions
dekk carts compile <file> --pipeline=db-opt --arts-debug=db_mode_tightening 2>&1

# Full diagnostic JSON
dekk carts compile <file> --diagnose --diagnose-output .carts/outputs/heuristics/<topic>-diag.json 2>/dev/null
```

## DB Partition Decision Surface

Use these diagnostic categories as explanations, not as source-file entry
points:

| Category | Condition | Result | Typical Trigger |
|----------|-----------|--------|-----------------|
| tiny read-only coefficient | Tiny read-only stencil coefficient | COARSE | Small constant arrays |
| pointer-of-pointer | Pointer-of-pointer type | COARSE | `memref<memref<T>>` |
| single-node read-only | Single-node + all read-only | COARSE | Read-only arrays on 1 node |
| explicit coarse contract | Explicit coarse contract | COARSE | Consumer override |
| no block capability | No block/element capability | COARSE | No partition dims found |
| indirect reads with block writes | Indirect reads + block writes | BLOCK | Mixed access patterns |
| uniform direct access | Uniform direct access | BLOCK | Regular array operations |
| double-buffer stencil | Double-buffer stencil (Jacobi) | BLOCK | Alternating buffers |
| indexed block-capable access | Indexed access + block capable | BLOCK | Index-based patterns |
| element-wise stencil | Element-wise stencil | STENCIL | Fine-grained stencil |
| block-capable stencil | Block-capable stencil | STENCIL | Block stencil with halo |
| element-wise capable | Element-wise capable | FINE | Per-element partitioning |
| Residual raw bridge | Unsupported or unproven ownership | COARSE or diagnostic | Coarse only for residual raw memrefs; non-coarse raw layout plans fail at `CreateDbs` |

## Distribution Strategy Selection

| Pattern | Machine | Result |
|---------|---------|--------|
| Matmul + internode | numNodes > 1 | Tiling2D |
| Matmul + intranode | numNodes == 1 | Block |
| Any + internode | numNodes > 1 | TwoLevel |
| Triangular | any | BlockCyclic |
| Stencil/Uniform/Unknown | any | Block |

## Vision Guardrails

- Heuristic triggers must be code-agnostic: affine structure, typed attrs,
  graph facts, contracts, layout mismatch, and runtime topology.
- SDE owns owner dims and block layout facts; CODIR owns collective/bridge
  family selection from SDE facts; ARTS consumes those facts to realize DBs,
  EDTs, owner maps, and grouped execution.
- DB/MU partition grain is not the same decision as CU/bridge grouping.
- Hypergraph evidence guides CU grouping, bridge coalescing, and partition
  quality over committed MU facts. It must not hardcode owner dims, block
  shapes, or benchmark-specific storage grain.

## Key Source Files

```
include/carts/dialect/arts/Analysis/heuristics/DbHeuristics.h — DB decision records
lib/carts/dialect/arts/Analysis/heuristics/DbHeuristics.cpp — diagnostic recording
include/carts/dialect/arts/Analysis/db/DbAnalysis.h — canonical DB/acquire facts
lib/carts/dialect/arts/Analysis/db/DbAnalysis.cpp — acquire summaries and refinement facts
include/carts/dialect/arts/Transforms/db/DbLayoutPlanUtils.h — layout plan helpers
lib/carts/dialect/arts/Transforms/db/DbTransformsPass.cpp — DB refinement controller
lib/carts/dialect/codir/Conversion/SdeToCodir/SdeToCodir.cpp — SDE-to-CODIR materialization
lib/carts/dialect/codir/Conversion/CodirToArts/CodirToArts.cpp — CODIR-to-ARTS materialization
```

## Instructions

When the user asks to explain a heuristic decision:

1. Identify the focus: partitioning or distribution.
2. Compile with the relevant `--arts-debug` channel to capture decisions
3. Dump IR at the decision stage (`sde-planning`, `codir-to-arts`, `db-opt`, or `post-db-refinement`)
4. Parse debug output for which heuristic rule fired
5. Explain: what the rule checks, why it matched, what alternatives exist
6. If the decision seems wrong, suggest: which input properties to change,
   or which heuristic rule to investigate in the source code
7. Cross-reference with `docs/heuristics/partitioning.md` and `distribution.md`
