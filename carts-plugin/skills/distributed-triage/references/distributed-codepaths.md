# Distributed Codepaths

Primary files for owner-map realization and multi-node routing:

- `docs/heuristics/distribution.md`
- `lib/carts/dialect/arts/Transforms/db/DbOwnerMapRealization.cpp`
- `lib/carts/dialect/codir/Conversion/SdeToCodir/SdeToCodir.cpp`
- `lib/carts/dialect/codir/Conversion/CodirToArts/CodirToArts.cpp`
- `lib/carts/dialect/sde/Transforms/effect/distribution/DistributionPlanning.cpp`
- `lib/carts/dialect/arts-rt/Conversion/ArtsRtToLLVM/ConvertArtsRtToLLVM.cpp`
- `lib/carts/codegen/Codegen.cpp`

High-value grep tokens:

- `distributed`
- `DbOwnerMapRealization`
- `DistributionPlanning`
- `distributed_db_init`
- `distributed_db_init_worker`
- `artsGetTotalNodes`
- `artsGuidGetRank`
- `route = linearIndex`
