# Partitioning Design

This document describes the partitioning design we want the compiler to use.

The goal is simple:
- keep one canonical analysis owner
- keep `DbPartitioning` as an orchestrator
- keep planners / rewriters / indexers unchanged
- make new partitioning modes pluggable without rebuilding the whole stack

## Core Model

Partitioning facts live on the DB graph.

```text
DbGraph
  -> DbAllocNode
     -> DbAcquireNode
        -> DbAcquirePartitionFacts
           -> DbPartitionEntryFact[]
           -> DbDimPartitionFact[]
```

`DbAcquireNode` is the source of truth for acquire-level partition analysis.
`DbAllocNode` remains the place where acquire facts are aggregated.
`DbAnalysis` exposes graph-backed queries for passes.

## Canonical Contracts

Keep these contracts as they are:

- `PartitionInfo`
  Runtime / rewrite semantics for one acquire entry.
- `DbRewriteAcquire`
  Final per-acquire rewrite payload.
- `DbRewritePlan`
  Final allocation rewrite plan.
- `DbAcquirePartitionView`
  Planner-facing normalized input.

These are already the right lowering contracts. They should not become
analysis storage.

## Graph-Backed Analysis

The new canonical analysis object is `DbAcquirePartitionFacts`.

It stores:
- requested partition mode
- access pattern
- direct / indirect access flags
- distribution-contract presence
- block-hint presence
- inferred block capability from acquire entries
- `DbPartitionEntryFact[]`
- `DbDimPartitionFact[]`

`DbPartitionEntryFact` stores:
- the original `PartitionInfo` entry
- representative offset / size
- mapped logical dimension, if known
- `needsFullRange`
- `preservesDistributedContract`

`DbDimPartitionFact` stores:
- logical dimension number
- dimension access pattern from alloc metadata
- whether it is the leading dynamic access dim
- whether any partition entry selects it
- whether selecting it requires full-range

This is intentionally small. It gives us:
- per-acquire information
- per-entry information
- per-dimension information

without creating a second planning universe beside the existing lowering path.

## Responsibility Split

The pipeline is:

```text
CreateDbs / ForLowering
  -> writes partition hints on DbAcquireOp

DbDimAnalyzer
  -> computes DbAcquirePartitionFacts on DbAcquireNode

DbAnalysis
  -> exposes derived summaries from graph-backed facts

DbPartitioning
  -> translates facts into heuristic context
  -> arbitrates allocation mode
  -> builds rewrite inputs

DbPartitionPlanner
  -> converts pass decisions into DbRewriteAcquire / DbRewritePlan

DbRewriter / DbIndexer
  -> execute the chosen partitioning mode
```

### What Each Layer Owns

- `DbDimAnalyzer`
  Canonical acquire legality and dimension mapping.
- `DbAnalysis`
  Stable query interface over graph state.
- `DbPartitioning`
  Coordination, reconciliation, and fallback policy.
- `PartitioningHeuristics`
  Allocation-mode choice from prepared facts.
- planners / rewriters / indexers
  Lowering only.

## Why This Is Minimal

We are not replacing the rewrite path.
We are only moving canonical legality into the graph.

That removes the duplication where `DbPartitioning` rebuilt:
- block capability
- full-range legality
- partition-dim mapping

from raw node queries every time.

The pass still decides policy, but it no longer owns the underlying facts.

## Adding A New Partitioning Mode

To add a new mode, the extension points are:

1. `PartitionMode`
2. `evaluatePartitioningHeuristics(...)`
3. `computeAllocationShape(...)`
4. `buildRewriteAcquire(...)`
5. `DbRewriter::create(...)`
6. the mode-specific rewriter / indexer

`DbAcquirePartitionFacts` should stay mode-agnostic. It describes legality and
dimensional evidence, not the lowering strategy.

## Current Contract Suite

The partitioning contract suite should always cover:
- `coarse`
- `fine_grained`
- `block`
- `stencil`
- block + full-range acquire safety
- cross-dimension full-range safety

That keeps the design honest: every mode has a direct contract, and the safety
fallbacks are tested separately from the mode selection itself.
