// RUN: not %carts-compile %S/../inputs/uniform_block.mlir --pipeline-stage=pre-lowering 2>&1 | %FileCheck %s

// CHECK: Unknown command line argument '--pipeline-stage=pre-lowering'
