# Stage Focus Guide

Use this as a starting point for pipeline bisection.

| Symptom | First Stages to Inspect |
|---|---|
| OpenMP structure missing or wrong task boundaries | `openmp-to-arts`, `edt-transforms` |
| Wrong loop shape or reordered access pattern | `pattern-pipeline` |
| DB count, acquire mode, or DB shape wrong | `create-dbs`, `db-opt` |
| Work chunks or distribution attrs look wrong | `edt-distribution` |
| Full-range/coarse/block decision wrong | `db-partitioning`, `post-db-refinement` |
| Runtime call structure wrong but high-level IR is fine | `pre-lowering`, `arts-to-llvm` |

If you do not know where to begin, dump:

1. `openmp-to-arts`
2. `pattern-pipeline`
3. `create-dbs`
4. `edt-distribution`
5. `db-partitioning`
6. `post-db-refinement`
7. `pre-lowering`
