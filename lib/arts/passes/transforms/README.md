# transforms/ -- Lowering and Conversion Passes

Passes that structurally transform the IR between pipeline stages.

| Pass | Stage | Purpose |
|------|-------|---------|
| CollectMetadata | collect-metadata | Extract loop/array metadata from source IR |
| PatternDiscovery | pattern-pipeline | Detect stencil, reduction, and access patterns |
| ConvertOpenMPToArts | openmp-to-arts | Convert OMP parallel regions to ARTS EDTs |
| CreateDbs | create-dbs | Create DataBlock allocations for shared memory |
| CreateEpochs | epochs | Insert epoch synchronization barriers |
| Concurrency | concurrency | Build EDT concurrency graph |
| EdtDistribution | edt-distribution | Assign distribution strategy + ForLowering |
| ForLowering | edt-distribution | Lower distributed for-loops to block iteration |
| ForOpt | post-distribution-cleanup | Structural cleanup after loop lowering |
| EdtLowering | pre-lowering | Outline EDT regions to ARTS runtime calls |
| DbLowering | pre-lowering | Lower DB operations to runtime calls |
| EpochLowering | pre-lowering | Lower epoch operations to runtime calls |
| ConvertArtsToLLVM | arts-to-llvm | Final ARTS dialect to LLVM conversion |

Support files (`*Support.cpp`, `*Patterns.cpp`) contain extracted helpers
and are not standalone passes.
