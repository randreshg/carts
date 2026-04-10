# tests/ — Test Suites

## Directory Layout

```
contracts/         MLIR lit tests (FileCheck regression tests)
  sde/             SDE dialect tests (stages 1-5: normalization, metadata, patterns)
  core/            Core ARTS dialect tests (stages 6-16: EDT, DB, epochs)
    partitioning/  DB partitioning contract tests (modes/ + safety/)
  rt/              Runtime dialect tests (stages 17-18: pre-lowering, arts-to-llvm)
  verify/          Verification barrier pass tests
  cli/             CLI flag validation tests
  inputs/          Shared test input files
examples/          End-to-end C/C++ integration tests
```

## Which directory for new tests?

| Pipeline stage                          | Directory            |
| --------------------------------------- | -------------------- |
| 1 raise-memref-dimensionality           | `sde/`               |
| 2 collect-metadata                      | `sde/`               |
| 3 initial-cleanup                       | `sde/`               |
| 4 openmp-to-arts                        | `sde/`               |
| 5 pattern-pipeline                      | `sde/`               |
| 6 edt-transforms                        | `core/`              |
| 7 create-dbs                            | `core/`              |
| 8 db-opt                                | `core/`              |
| 9 edt-opt                               | `core/`              |
| 10 concurrency                          | `core/`              |
| 11 edt-distribution                     | `core/`              |
| 12 post-distribution-cleanup            | `core/`              |
| 13 db-partitioning                      | `core/partitioning/` |
| 14 post-db-refinement                   | `core/`              |
| 15 late-concurrency-cleanup             | `core/`              |
| 16 epochs                               | `core/`              |
| 17 pre-lowering                         | `rt/`                |
| 18 arts-to-llvm                         | `rt/`                |
| Verification passes (VerifyLowered etc) | `verify/`            |
| CLI flag/option validation              | `cli/`               |
