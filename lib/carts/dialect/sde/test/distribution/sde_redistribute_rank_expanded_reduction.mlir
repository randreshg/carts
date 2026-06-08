// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute,verify-sde-redistribute)' 2>&1 | %FileCheck %s

// A rank-expanded MU can carry a reduction redistribution when the writer's
// physical block-grid facts prove the logical owner dim maps to the leading
// grid coordinate.

// CHECK: sde.redist <reduce_scatter_like> %{{.*}} : memref<2x128x256xf32> from owner [0] block [1, 128, 256] to owner [0] block [1, 128, 256] cost 100

func.func @expanded_cross_owner_reduction(%T: memref<2x128x256xf32>,
                                          %G: memref<256xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c2 = arith.constant 2 : index
  %c256 = arith.constant 256 : index
  %cst = arith.constant 0.0 : f32
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c2) step (%c1) classification(<elementwise>) {
    ^bb0(%b: index):
      memref.store %cst, %T[%b, %c0, %c0] : memref<2x128x256xf32>
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128], muBlockCount = 2 : i64, role = "write", commVolumeBytes = 0 : i64}], physicalOwnerDims = [0], physicalBlockShape = [128, 256]}
    sde.su_iterate (%c0) to (%c256) step (%c1) reduction_strategy(<local_accumulate>) classification(<elementwise_pipeline>) {
    ^bb0(%j: index):
      %v = memref.load %T[%c0, %c0, %j] : memref<2x128x256xf32>
      memref.store %v, %G[%j] : memref<256xf32>
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128], muBlockCount = 2 : i64, role = "read", commVolumeBytes = 100 : i64}, {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [256], muBlockCount = 1 : i64, role = "write", commVolumeBytes = 0 : i64}], layoutsDisagree = [0], partialReduction, partialReductionDims = [0], partialReductionOwnerDims = [0]}
    sde.yield
  }
  return
}
