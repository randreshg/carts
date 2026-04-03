# Distributed Codepaths

Primary files for distributed ownership and multi-node routing:

- `docs/heuristics/distribution.md`
- `lib/arts/passes/opt/db/DbDistributedOwnership.cpp`
- `lib/arts/passes/opt/loop/DistributedHostLoopOutlining.cpp`
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`
- `lib/arts/passes/transforms/ForLowering.cpp`
- `lib/arts/Dialect/Codegen.cpp`

High-value grep tokens:

- `distributed`
- `--distributed-db`
- `DbDistributedOwnership`
- `DistributedHostLoopOutlining`
- `distributed_db_init`
- `distributed_db_init_worker`
- `artsGetTotalNodes`
- `artsGuidGetRank`
- `route = linearIndex`
