// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 | %FileCheck %s

// CHECK: reduce-scatter movement is not backed by committed partial-reduction dims/owner dims

func.func @stale_partial_reduction_dims(%Y: memref<64xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %zero = arith.constant 0.0 : f32
  sde.su_iterate (%c0) to (%c64) step (%c1) {
  ^bb0(%i: index):
    sde.array_layout_root write %Y : memref<64xf32> array_id(2)
    sde.cu_region <single> {
      memref.store %zero, %Y[%i] : memref<64xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 2 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [64], muBlockCount = 1 : i64, role = "write"}]}
  sde.su_distribute <owner_compute> {
    sde.su_reduce_scatter %Y : memref<64xf32> array_id(2) owner [0] block [64] reduce 0 kind <add>
    sde.su_iterate (%c0) to (%c64) step (%c1) {
    ^bb0(%i: index):
      sde.array_layout_root write %Y : memref<64xf32> array_id(2)
      sde.cu_region <single> {
        memref.store %zero, %Y[%i] : memref<64xf32>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [{arrayId = 2 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [64], muBlockCount = 1 : i64, role = "write"}], partialReduction, partialReductionDims = [999], partialReductionOwnerDims = [0]}
  }
  return
}
