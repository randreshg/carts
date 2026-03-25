# lib/arts/ — Core Compiler Implementation

## Directory Layout

```
dialect/           MLIR dialect ops, types, and interfaces (ArtsOps.td, ArtsTypes.td)
analysis/          Analysis framework
  graphs/          DB and EDT dependency graphs
  heuristics/      Partitioning and distribution decision logic
passes/
  opt/             Optimization passes
    db/            DataBlock optimizations (partitioning, mode tightening, scratch elimination)
    edt/           EDT optimizations (structural, epoch, fusion)
    general/       General optimizations (inlining, memref canonicalization, loop reordering)
  transforms/      Lowering and conversion passes
utils/             Shared utilities (attribute names, debug macros, value helpers)
```
