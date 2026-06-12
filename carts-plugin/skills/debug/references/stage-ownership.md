# Stage Ownership

Use this when deciding where to start a bisection.

| Stage | Primary Question |
|---|---|
| `sde-planning` | Did OpenMP structure become the right SDE fact shape? Did SDE pattern discovery (`DistributionPlanning`, `LoopInterchange`, ...) commit the right facts before ARTS? |
| `sde-to-arts` | Did SDE codelet facts become isolated ARTS codelets with explicit deps, params, and token-local views? |
| `sde-to-arts` | Did ARTS realize the right ARTS DB/acquire/EDT shape, and did any SDE op survive the boundary? |
| `edt-local-cleanup` | Did EDT structure or pointer rematerialization change dependencies or task boundaries? |
| `create-dbs` | Did the right allocations become DBs with plausible initial partition hints? |
| `db-opt` | Did load/store reality produce the correct in/out/inout modes? |
| `post-db-refinement` | Did validation or cleanup destroy valid DB/EDT facts? |
| `pre-lowering` | Is the runtime-facing IR still semantically correct before LLVM conversion? |
| `arts-rt-to-llvm` | Did final lowering or route emission break otherwise-correct ARTS facts? |

Fast default for unknown compiler failures:

1. `create-dbs`
2. `db-opt`
3. `post-db-refinement`
4. `pre-lowering`
