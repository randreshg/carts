---
name: carts-check-utils
description: Use before adding a CARTS helper, utility file, static helper, or utility-like attribute/string accessor.
user-invocable: true
allowed-tools: Read, Grep, Glob, Bash, Agent
argument-hint: <helper-name-or-behavior>
parameters:
  - name: function_name
    type: str
    gather: "Name or behavior of the helper you want to add"
---

# CARTS Utility Ownership Check

For pure discovery, start with [[carts-find-utils]]. This skill is for adding
a helper and choosing its canonical home.

## Hard Rule

- Add no helper until existing utilities and sibling pass helpers are searched.
- Keep pass-local only when one pass uses it and it is not a dialect invariant.
- Put shared behavior in the narrowest semantic owner, not the nearest caller.
- Declare CARTS IR operation contracts and attributes in ODS first; never add
  `AttrNames::` entries for CARTS IR attrs.
- Land declaration, implementation, caller cleanup, and verification together.

## Procedure

1. Search by exact name across `include/carts` and `lib/carts`.
2. Search by behavior: value/constant/fold/same, loop/trip/IV, invariant,
   memref/root/slice, DB/EDT/epoch/dependency, runtime ABI/packing/pointer.
3. Check sibling dialect TableGen files before adding a new enum or attribute.
4. Inspect `include/carts/utils`, `lib/carts/utils`,
   `include/carts/dialect/*/Utils`, and owning SDE analysis utilities.
5. Check pass-area support files such as `*Support.cpp`, `*Internal.h`, and
   boundary conversion helpers.
6. Choose one home from the placement matrix and remove duplicate local copies.
7. If the helper touches CARTS IR attributes, invoke [[carts-attr-consolidation]].

## Placement Matrix

| Helper kind | Canonical home |
|-------------|----------------|
| Dialect-neutral value folding, constants, casts, memref views, same-value checks | `include/carts/utils/ValueAnalysis.h` |
| Generic index builders and pure-op predicates | `include/carts/utils/Utils.h` |
| Loop shape, IVs, trip counts, nearest loops, loop depth | `include/carts/utils/LoopUtils.h` |
| Deferred op removal | `include/carts/utils/RemovalUtils.h` |
| Shared ODS attribute enum across dialects | `include/carts/IR/CommonAttrs.td` |
| SDE source semantics, HPF-style layout/alignment, affine access maps, PatternAnalysis, MU/CU/SU planning, real loop/layout facts | `include/carts/dialect/sde/Analysis` or `Utils` |
| ARTS codelet isolation, dep/param ABI, token-local views, collective/contraction/bridge/halo materialization | `include/carts/dialect/arts/Utils` |
| ARTS DB/EDT/epoch objects, dependency slots, placement, ownership, owner maps, per-block DB realization, grouped CU/bridge execution | `include/carts/dialect/arts/Utils` or `Analysis` |
| ARTS loop invariance, hoist legality, dominance | `include/carts/dialect/arts/Utils/LoopInvarianceUtils.h` |
| ARTS-RT runtime ABI packing, depv layout, runtime calls, pointer lowering only | `include/carts/dialect/arts-rt/Utils` |
| Compiler instrumentation or driver-only helpers | existing compile-driver owner |

Do not place helpers where they enable downstream recomputation of committed
facts. Owner dims belong to SDE layout analysis; collective families belong to
ARTS materialization; owner maps and grouped DB/EDT realization belong to
ARTS; ABI call mechanics belong to ARTS-RT.

## Attribute Strings

Never hardcode CARTS IR attribute strings, and do not put local op-region
legality into an aggregate pass when it belongs in an op contract. Declare the
attribute or local validation surface on its owning op, or as a free-standing
ODS attr plus op-interface for multi-op cases, in the dialect
`*Attrs.td`/`*Ops.td`; use generated `op.get*AttrName()` / `op.get*()`
accessors and op verifiers. `AttrNames::` is not a placement option for CARTS
IR attributes.

## Existing Helpers To Reuse

| Need | Existing helper |
|------|-----------------|
| zero/one/constant checks | `ValueAnalysis::{isZeroConstant,isOneConstant,tryFoldConstantIndex}` |
| same-value or range equivalence | `ValueAnalysis::{sameValue,areValuesEquivalent,areValueRangesEquivalent}` |
| create zero/one/arbitrary index | `createZeroIndex`, `createOneIndex`, `createConstantIndex` |
| loop IV, trip count, nearest loop, loop depth | `LoopUtils.h` |
| side-effect-free arithmetic-like op | `isSideEffectFreeArithmeticLikeOp` in `Utils.h` |
| SDE Polygeist dependency index clamping or OMP predicates | `PolygeistToSdeUtils.h` |
| undef-like op detection | ARTS `RuntimeOpUtils.h` |
| DB provenance/access info | `DbUtils.h` |
| EDT environment/captures | `EdtUtils.h` |
| lowering facts | `LoweringFactUtils.h` |
| runtime IDs/calls/DB ABI | ARTS-RT `Utils/` |

## Required Answer

State exactly one:

- `Use existing helper at <path> because <reason>.`
- `Extract to <path> because <owning semantic category>.`
- `Keep pass-local because all pass-local rule items are true.`
