// RUN: not %carts-compile %S/../inputs/uniform_block.mlir --pipeline=not-a-stage 2>&1 | %FileCheck %s

// CHECK: Unknown pipeline step: 'not-a-stage'
// CHECK: Available pipeline steps:
// CHECK: - raise-memref-dimensionality
// CHECK: - arts-to-llvm
// CHECK: - complete
