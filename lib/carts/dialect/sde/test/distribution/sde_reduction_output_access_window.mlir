// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu)' 2>&1 | %FileCheck %s

// A reduction-classified SU with no SDE reduction accumulator writes an
// owner-indexed output block directly. SDE may realize it with the same
// rank-expanded MU/window carrier as elementwise output writers.

// CHECK-LABEL: func.func @reduction_output_window
// CHECK: %[[MU:.*]] = sde.mu_alloc : memref<4x256xf32>
// CHECK-NOT: sde.mu_access_window
// CHECK: memref.store %{{.*}}, %[[MU]][%{{.*}}] : memref<4x256xf32>

func.func @reduction_output_window() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %v = arith.constant 1.0 : f32
  %out = sde.mu_alloc : memref<1024xf32>
  sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<reduction>) {
  ^bb0(%i: index):
    sde.array_layout_root write %out : memref<1024xf32> array_id(0)
    sde.cu_region <parallel> {
      memref.store %v, %out[%i] : memref<1024xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [256], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}]}
  return
}
