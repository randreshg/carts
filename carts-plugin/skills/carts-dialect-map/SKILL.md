---
name: carts-dialect-map
description: Use when locating CARTS dialect code, tracing op lifecycle, choosing owners, or checking boundary invariants.
---

# CARTS Dialect Map

Read `references/dialect-map.md` when deciding where an operation, pass,
analysis, or test belongs.

## Hard Rule

- SDE owns OpenMP semantics, HPF-style `DISTRIBUTE`/`ALIGN`, per-array layouts,
  abstract communication-volume cost, and the real source/SU/CU/MU loop/layout
  transformations needed to make those facts true. It names no collective,
  DB, EDT, route, GUID, or runtime policy.
- CODIR owns isolated codelets with explicit deps/params and first-class
  distribution patterns. It consumes committed SDE layout facts, chooses
  collectives/bridges from compute pattern plus layout mismatch, and
  materializes contraction, redistribution, and halo structure.
- ARTS owns DB/EDT/epoch orchestration, analyses, placement, distributed
  ownership, owner maps, per-block single-writer DB realization, and grouped
  compute/bridge/communication CUs.
- ARTS-RT owns lowering-ready runtime ABI shape before LLVM. It mechanically
  lowers ARTS facts and does not infer scheduling, ownership, partition, or
  collective policy.
- Do not patch downstream symptoms when the semantic owner is upstream. Later
  layers consume, verify, realize, or reject committed upstream facts; they do
  not silently recompute them.
- Keep DB grain separate from CU/bridge grain, and treat hypergraph decisions as
  CU grouping evidence over committed MU facts rather than benchmark-specific
  owner-dim repair.

## Procedure

1. Find the op definition in the dialect-specific TableGen file.
2. Find creation sites with `rg "OpName|op mnemonic" include lib tools docs`.
3. Check where it is transformed, verified, and erased or lowered.
4. Confirm stage ownership with [[carts-pipeline-map]] and the live compiler.
5. State the dialect contract and what that dialect must not own.
6. Add tests in the owning dialect test directory.
7. For duplicated attribute enums or convert-only boundaries, invoke
   [[carts-attr-consolidation]].

## Shared Attribute Boundary

A `convert<Name>` switch that maps identical enum cases across dialects is a
boundary smell, not a durable API. Hoist identical enums instead of preserving
rename-only conversion code.

## Required Answer

State the owning dialect, lifecycle path, boundary rule, first affected pipeline
stage, and test location.
