# CARTS Dialect Documentation

This is the target documentation root for CARTS compiler dialects. Each dialect
owns its own IR, query helpers, transforms, conversions, and boundary checks.
Shared facts may cross dialect boundaries only as explicit IR attributes,
operands, or types that the receiving dialect consumes directly.

Target stack:

```text
Polygeist -> sde -> arts -> arts-rt -> LLVM
```

## Dialect Ownership

| Dialect | IR | Analysis | Optimizations | Conversion |
|---|---|---|---|---|
| [`sde`](./sde/README.md) | Source semantic structure: MU/CU/SU, patterns, tokens, barriers. | Source legality, memref access, dependence, reductions, pattern facts. | Tiling, fusion, chunking, distribution transforms, barrier transforms, reduction strategy. | OpenMP/Polygeist to SDE, SDE to ARTS. |
| [`arts`](./arts/README.md) | Isolated task bodies, deps, params, token-local views, DB/EDT/epoch/dep objects. | Capture/dependency ABI, token-local access, DB/EDT/epoch queries, dep windows, placement/resource binding. | Task canonicalization, scalar capture cleanup, token mode refinement, DB mode tightening, EDT/epoch orchestration, dep-slot refinement, boundary checking. | ARTS to ARTS-RT. |
| [`arts-rt`](./arts-rt/README.md) | Runtime-call-shaped ABI IR. | Runtime-call purity, packing, pointer/dependency-slot locality, alias facts. | Runtime call hoisting, packing cleanup, scalar replacement, pointer hoisting, LLVM-facing cleanup. | ARTS-RT to LLVM. |

## Boundary Rule

Analysis objects do not cross dialect boundaries as hidden side channels. If a
later dialect needs a fact, the earlier dialect must either transform the IR so
that the fact is already true, represent the fact as ordinary IR, or fail
closed before conversion.

Examples:

- SDE pattern facts become `sde.pattern`, owner dims, block shapes, token
  windows, and reduction facts.
- ARTS capture and resource analysis become explicit deps, params, yielded
  values, token-local view operands, ARTS object attributes, dependency slots,
  DB layouts, and epoch/EDT structure.
- ARTS-RT lowering facts become runtime ABI operands, metadata, or LLVM-facing
  attributes.

## Documentation Rule

Every dialect documentation directory should contain:

- `README.md`: responsibilities and boundary rules.
- `analysis.md`: analyses owned by the dialect and facts they may emit.
- `optimizations.md`: transformations owned by the dialect and their legality
  inputs.

The source-tree target layout mirrors this structure with `IR/`, `Analysis/`,
`Transforms/`, `Conversion/`, and `Verify/`.
