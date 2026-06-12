---
name: carts-finishing
description: Use when advancing or finishing active CARTS compiler/runtime work, choosing the next fix, applying regression guards, or deciding where a fix belongs.
---

# CARTS Finishing

Use this as a final integration checklist for active CARTS work. It is not a
task graph and it does not authorize deferred metadata markers.

## Hard Rule

- Fix the first wrong committed fact in the owning dialect.
- Do the real transformation, or fail closed with evidence.
- Do not create planner attrs, ownership attrs, deferred attrs, or runtime
  repair hooks.
- Keep DB/MU grain separate from CU/bridge grain.
- Run focused verification first, then broaden based on blast radius.

## Procedure

1. Identify the first failing stage with `dekk carts compile --pipeline` or
   `--all-pipelines`.
2. Assign ownership:
   - SDE: source semantics, layout/alignment, MU/CU/SU transforms, movement
     structure.
   - ARTS: isolated codelets, DB/EDT/epoch realization, distributed ownership,
     owner routes, DB modes, grouped compute/bridge/communication CUs.
   - ARTS-RT: mechanical runtime ABI lowering.
3. Inspect the live IR at the nearest boundary before editing.
4. Make the smallest production fix in the owning layer.
5. Run `dekk carts format`, the focused lit test, `dekk carts build`, and the
   broad suite required by the changed surface.
6. Before commit, run [[carts-simplify]] and [[carts-review]].

## Required Answer

State the first bad fact, owning layer, files changed, verification run, and any
remaining benchmark or distributed validation gap.
