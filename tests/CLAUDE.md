# tests/ — Test Suites

## Directory Layout

```
cli/               CLI flag validation tests
verify/            Verification barrier pass tests
inputs/            Shared test input files (configs, snapshots)
```

Pass-level lit tests are co-located with source code:
```
lib/arts/dialect/sde/test/   SDE dialect tests (stages 1-5)
lib/arts/dialect/core/test/  Core ARTS dialect tests (stages 6-16)
  partitioning/              DB partitioning contract tests (modes/ + safety/)
lib/arts/dialect/rt/test/    Runtime dialect tests (stages 17-18)
```

Demo programs live at the project root:
```
samples/           End-to-end C/C++ demo programs (moved from tests/examples/)
```

## Which directory for new tests?

| Pipeline stage                          | Directory                          |
| --------------------------------------- | ---------------------------------- |
| 1 raise-memref-dimensionality           | `lib/arts/dialect/sde/test/`       |
| 2 collect-metadata                      | `lib/arts/dialect/sde/test/`       |
| 3 initial-cleanup                       | `lib/arts/dialect/sde/test/`       |
| 4 openmp-to-arts                        | `lib/arts/dialect/sde/test/`       |
| 5 pattern-pipeline                      | `lib/arts/dialect/sde/test/`       |
| 6 edt-transforms                        | `lib/arts/dialect/core/test/`      |
| 7 create-dbs                            | `lib/arts/dialect/core/test/`      |
| 8 db-opt                                | `lib/arts/dialect/core/test/`      |
| 9 edt-opt                               | `lib/arts/dialect/core/test/`      |
| 10 concurrency                          | `lib/arts/dialect/core/test/`      |
| 11 edt-distribution                     | `lib/arts/dialect/core/test/`      |
| 12 post-distribution-cleanup            | `lib/arts/dialect/core/test/`      |
| 13 db-partitioning                      | `lib/arts/dialect/core/test/partitioning/` |
| 14 post-db-refinement                   | `lib/arts/dialect/core/test/`      |
| 15 late-concurrency-cleanup             | `lib/arts/dialect/core/test/`      |
| 16 epochs                               | `lib/arts/dialect/core/test/`      |
| 17 pre-lowering                         | `lib/arts/dialect/rt/test/`        |
| 18 arts-to-llvm                         | `lib/arts/dialect/rt/test/`        |
| Verification passes (VerifyLowered etc) | `tests/verify/`                    |
| CLI flag/option validation              | `tests/cli/`                       |
