// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: SDE operation reached CODIR-to-ARTS
// CHECK-SAME: convert-sde-boundary-to-arts

module {
  func.func @residual_sde_is_rejected(%out: memref<1xindex>) {
    %workers = sde.resource_query <logical_workers>
    memref.store %workers, %out[%workers] : memref<1xindex>
    return
  }
}
