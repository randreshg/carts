// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 | %FileCheck %s

// CHECK: movement op in committed-layout scope must carry array_id
func.func @movement_missing_array_id(%A: memref<8x4xf64>) {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index
  sde.su_iterate (%c0) to (%c8) step (%c1) {
  ^bb0(%i: index):
    sde.array_layout_root write %A : memref<8x4xf64> array_id(0)
    sde.cu_region <single> {
      %v = memref.load %A[%i, %c0] : memref<8x4xf64>
      %next = arith.addf %v, %v : f64
      memref.store %next, %A[%i, %c0] : memref<8x4xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [2, 4], muBlockCount = 4 : i64, role = "write"}]}
  sde.su_distribute <owner_compute> {
    sde.su_halo %A : memref<8x4xf64> owner [0] block [2, 4] halo [1, 0]
  }
  return
}
