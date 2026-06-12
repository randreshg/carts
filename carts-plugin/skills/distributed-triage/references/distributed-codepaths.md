# Distributed Codepaths

Primary files for distributed ownership realization and multi-node routing:

- `docs/heuristics/distribution.md`
- `lib/carts/dialect/arts/Transforms/db/DbDistributedOwnershipRealization.cpp`
- `lib/carts/dialect/arts/Transforms/SdeToArtsBoundary.cpp`
- `lib/carts/dialect/sde/Transforms/effect/distribution/DistributionPlanning.cpp`
- `lib/carts/dialect/arts/Transforms/db/DbDistributedRuntimeInit.cpp`
- `lib/carts/dialect/arts-rt/Conversion/ArtsRtToLLVM/ConvertArtsRtToLLVM.cpp`

High-value grep tokens:

- `distributed`
- `DbDistributedOwnershipRealization`
- `DistributionPlanning`
- `distributed_db_init`
- `distributed_db_init_worker`
- `db-distributed-runtime-init`
- `arts_rt.db_guid_reserve`
- `arts_rt.db_create_with_guid_local`
- `artsGetTotalNodes`
- `artsGuidGetRank`
