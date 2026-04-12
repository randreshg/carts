# Stage Ownership

Use this when deciding where to start a bisection.

| Stage | Primary Question |
|---|---|
| `openmp-to-arts` | Did OpenMP structure become the right EDT/for/barrier shape? Did SDE pattern discovery (`ScopeSelection`, `DistributionPlanning`, `LoopInterchange`, ...) stamp the right contracts before `ConvertSdeToArts`? |
| `edt-transforms` | Did EDT structure/invariant motion change dependencies or task boundaries? |
| `create-dbs` | Did the right allocations become DBs with plausible initial partition hints? |
| `db-opt` | Did load/store reality produce the correct in/out/inout modes? |
| `edt-distribution` | Did H2 choose the right distribution kind/pattern and task structure? |
| `db-partitioning` | Did H1 choose coarse/block/stencil/fine-grained correctly? |
| `post-db-refinement` | Did validation or cleanup destroy a valid contract? |
| `pre-lowering` | Is the runtime-facing IR still semantically correct before LLVM conversion? |
| `arts-to-llvm` | Did final lowering or route emission break an otherwise-correct plan? |

Fast default for unknown compiler failures:

1. `create-dbs`
2. `edt-distribution`
3. `db-partitioning`
4. `post-db-refinement`
5. `pre-lowering`
