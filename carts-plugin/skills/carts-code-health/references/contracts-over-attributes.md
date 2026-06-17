# Contracts Over Attributes

Use attributes only when weaker rungs are insufficient. In CARTS, weak
attribute propagation is especially risky because SDE, ARTS, and ARTS-RT have
separate ownership of layout, graph, DB/EDT, and runtime facts.

## Contract Ladder

1. Real transformation or structural rewrite
2. Dialect op
3. Type
4. Op-interface
5. Verifier or trait
6. Typed attribute
7. Raw/string attribute bridge

Raw attributes are acceptable as migration bridges, but the backlog target must
name a stronger rung or an owning-layer rejection.

## Evidence

- Polygeist expresses view-like semantics with `ViewLikeOpInterface` on ops:
  `external/Polygeist/include/polygeist/PolygeistOps.td:136`,
  `external/Polygeist/include/polygeist/PolygeistOps.td:246`,
  `external/Polygeist/include/polygeist/PolygeistOps.td:263`,
  `external/Polygeist/include/polygeist/PolygeistOps.td:279`.
- Those ops expose generated/source hooks such as `getViewSource`, so callers do
  not need per-op switches:
  `external/Polygeist/include/polygeist/PolygeistOps.td:148`,
  `external/Polygeist/include/polygeist/PolygeistOps.td:258`,
  `external/Polygeist/include/polygeist/PolygeistOps.td:274`.
- MLIR models mixed static/dynamic slice facts with `OpFoldResult`:
  `external/Polygeist/llvm-project/mlir/lib/Interfaces/ViewLikeInterface.cpp:75`,
  `external/Polygeist/llvm-project/mlir/lib/Interfaces/ViewLikeInterface.cpp:79`,
  `external/Polygeist/llvm-project/mlir/lib/Interfaces/ViewLikeInterface.cpp:90`.
- `ValueOrInt` folds constant values through `m_Constant` instead of requiring
  every caller to duplicate that logic:
  `external/Polygeist/include/polygeist/Ops.h:152`,
  `external/Polygeist/include/polygeist/Ops.h:158`,
  `external/Polygeist/include/polygeist/Ops.h:162`.
- `ValueBoundsOpInterface` uses the same mixed value/attribute model:
  `external/Polygeist/llvm-project/mlir/lib/Interfaces/ValueBoundsOpInterface.cpp:38`,
  `external/Polygeist/llvm-project/mlir/lib/Interfaces/ValueBoundsOpInterface.cpp:64`,
  `external/Polygeist/llvm-project/mlir/lib/Interfaces/ValueBoundsOpInterface.cpp:65`.

## CARTS Application

- SDE layout/access/movement facts: promote to ops, types, interfaces, or
  verifiers before considering deletion.
- ARTS DB/EDT facts: keep generated accessors and typed attrs as bridges until
  ownership can become explicit graph structure.
- ARTS-RT hints: lower mechanically from ARTS facts; do not infer missing
  planning facts from raw runtime attrs.
