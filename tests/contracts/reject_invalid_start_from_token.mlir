// RUN: not %carts-compile %S/inputs/uniform_block.mlir --start-from=not-a-stage --pipeline=create-dbs 2>&1 | %FileCheck %s --check-prefix=UNKNOWN
// RUN: not %carts-compile %S/inputs/uniform_block.mlir --start-from=complete --pipeline=complete 2>&1 | %FileCheck %s --check-prefix=COMPLETE

// UNKNOWN: Unknown start-from pipeline step: 'not-a-stage'
// UNKNOWN: Available start-from pipeline steps:
// UNKNOWN: - raise-memref-dimensionality
// UNKNOWN: - arts-to-llvm
// UNKNOWN-NOT: - complete

// COMPLETE: Unknown start-from pipeline step: 'complete'
// COMPLETE: Available start-from pipeline steps:
// COMPLETE: - raise-memref-dimensionality
// COMPLETE: - arts-to-llvm
// COMPLETE-NOT: - complete
