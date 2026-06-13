// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}has committed physical partition facts but no access-window dependencies

func.func @physical_layout_without_access_windows_fails_closed(%A: memref<128xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c128 = arith.constant 128 : index
  %v = arith.constant 1.0 : f32
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      memref.store %v, %A[%i] : memref<128xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [16], muBlockCount = 8 : i64, role = "write"}]}
  return
}
