// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,sde-mu-access-window-sync-opt,verify-sde-mu-access-window-sync)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @barrier_before_source_effect
// CHECK: sde.su_iterate
// CHECK: sde.su_barrier
// CHECK: sde.cu_region <single>
// CHECK: func.call @observe
func.func @barrier_before_source_effect(%timer: i64) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c8 = arith.constant 8 : index
  %zero = arith.constant 0.0 : f64
  %A = sde.mu_alloc {arrayId = 0 : i64} : memref<8x4xf64>
  sde.su_iterate (%c0, %c0) to (%c8, %c4) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %A : memref<8x4xf64> array_id(0)
    sde.cu_region <parallel> {
      memref.store %zero, %A[%i, %j] : memref<8x4xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [4, 4], muBlockCount = 2 : i64, role = "write"}]}
  sde.su_barrier
  sde.cu_region <single> {
    func.call @observe(%timer) : (i64) -> ()
    sde.yield
  }
  return
}

func.func private @observe(i64)
