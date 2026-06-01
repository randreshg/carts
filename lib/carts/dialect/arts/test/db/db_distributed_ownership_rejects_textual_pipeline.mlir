// RUN: not %carts-compile %s --pass-pipeline='builtin.module(db-distributed-ownership)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @textual_distributed_ownership_requires_staged_pipeline() {
    return
  }
}

// CHECK: 'db-distributed-ownership' does not refer to a registered pass or pass pipeline
