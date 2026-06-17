---
name: carts-code-health
description: Use when reviewing, planning, or documenting CARTS pass, utility, contract, attribute, or code-health cleanup.
---

# CARTS Code Health

Use this before proposing or reviewing cleanup of CARTS compiler/runtime passes,
helpers, dialect contracts, ODS attrs, or utility tiers.

## Hard Rule

Do not delete or weaken SDE/ARTS/ARTS-RT facts because their current encoding is
unhealthy. Promote required facts to stronger structure, or file a backlog item
for the owning layer.

## Contract Ladder

Prefer the strongest rung that can make the fact true:

1. Real transformation or structural rewrite
2. Dialect op
3. Type
4. Op-interface
5. Verifier or trait
6. Typed attribute
7. Raw/string attribute bridge

## Principles

1. Single responsibility: one pass/helper/verifier has one primary job.
2. Plans are transform ops or structural rewrites, not downstream attribute
   promises.
3. Contracts use the strongest suitable rung: transformation, op, type,
   op-interface, verifier/trait, typed attr.
4. Committed vision facts are promoted, not blanket-deleted.
5. Triggers are structural and code-agnostic.
6. Composition beats configuration and hidden pass-order coupling.
7. Prefer reusable MLIR/CARTS abstractions over repeated ad hoc matching.
8. Public headers avoid `using namespace`; `.cpp` files may use local using.
9. ODS/TableGen and generated accessors are the first source of truth.
10. Remove dead code; label dormant scaffolding instead of deleting it.
11. Comments explain non-obvious why, not routine what.
12. Fix root cause at the owning layer; downstream verifies or rejects.

## Procedure

1. Classify the surface: pass split, helper placement, attr/contract ladder,
   dead/dormant hygiene, namespace/style, or source comment quality.
2. Read the relevant reference:
   - `references/mlir-pass-anatomy.md`
   - `references/contracts-over-attributes.md`
   - `references/cpp-style.md`
3. For pass edits, also use [[carts-pass-dev]] and [[carts-dialect-map]].
4. For attr or enum edits, delegate enum-hoist and raw-string policy to
   [[carts-attr-consolidation]].
5. Before adding helpers, use [[carts-check-utils]]; before consolidating
   duplicates, use [[carts-refactor-utils]].
6. For SDE/ARTS/ARTS-RT ownership or vision-fact questions, use
   [[carts-vision]].
7. Before finishing, run [[carts-simplify]] and state what was simplified or why
   no simplification was appropriate.

## Review Checklist

- Does the patch reduce responsibilities without moving facts to the wrong
  layer?
- Is every fact represented at the strongest practical contract-ladder rung?
- Are raw attrs copied only as a bridge to a stronger planned contract?
- Are dead and dormant paths distinguished with evidence?
- Are tests or docs scoped to the owning surface?

## Required Answer

State the classified surface, principle numbers applied, target contract rung,
related sibling skills used, and verification evidence.
