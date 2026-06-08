// Distribution has no user-facing compiler toggle. Multinode builds distribute
// by default; the old opt-in and opt-out flags no longer exist.

// RUN: not %carts-compile --distributed-db --print-pipeline-manifest-json 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=REMOVED
// RUN: not %carts-compile --no-distributed-db --print-pipeline-manifest-json 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=REMOVED-NEGATIVE
// RUN: %carts-compile --help 2>&1 | %FileCheck %s --check-prefix=HELP --implicit-check-not=no-distributed-db --implicit-check-not=distributed-db

// REMOVED: Unknown command line argument '--distributed-db'
// REMOVED-NEGATIVE: Unknown command line argument '--no-distributed-db'

// HELP: OVERVIEW: MLIR Optimization Driver
