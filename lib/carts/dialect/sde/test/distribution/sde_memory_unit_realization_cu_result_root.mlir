// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-memory-unit-realization,sde-rank-expand-mu,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// A memref allocated inside a producer CU and yielded as a CU result is still
// SDE-owned storage once later SUs commit block-layout facts for that result.
// SDE must canonicalize those external uses to a visible MU before the ARTS
// boundary; ARTS must not rediscover or repair this root.

// CHECK-LABEL: func.func @cu_result_root_realizes_visible_mu
// CHECK: %[[MU:.*]] = sde.mu_alloc : memref<4x16xf32>
// CHECK: sde.array_layout_root write %[[MU]] : memref<4x16xf32> array_id(0)
// CHECK-NOT: sde.mu_access_window
// CHECK: memref.store %{{.*}}, %[[MU]][%{{.*}}] : memref<4x16xf32>

func.func @cu_result_root_realizes_visible_mu() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %one = arith.constant 1.0 : f32
  %A = sde.cu_region <single> -> (memref<64xf32>) {
    %alloc = memref.alloc() : memref<64xf32>
    sde.yield %alloc : memref<64xf32>
  }
  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.array_layout_root write %A : memref<64xf32> array_id(0)
    sde.cu_region <parallel> {
      memref.store %one, %A[%i] : memref<64xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [16], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}]}
  return
}
