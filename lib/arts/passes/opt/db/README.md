# opt/db/ -- DataBlock Optimization Passes

Passes that optimize DataBlock allocation, access modes, and partitioning.

| Pass | Stage | Purpose |
|------|-------|---------|
| DbModeTightening | db-opt | Narrow DB access modes (inout -> in/out) |
| DbScratchElimination | db-opt | Eliminate unnecessary scratch DBs |
| DbTransformsPass | db-opt | Composite DB optimization driver |
| DbPartitioning | db-partitioning | Apply H1 heuristics for block partitioning |
| ContractValidation | post-db-refinement | Validate lowering contracts after partitioning |
| BlockLoopStripMining | post-db-refinement | Strip-mine loops for block-partitioned DBs |
| DbDistributedOwnership | post-db-refinement | Assign distributed ownership to partitioned DBs |

Split files (`*Support.cpp`) contain extracted helpers.
