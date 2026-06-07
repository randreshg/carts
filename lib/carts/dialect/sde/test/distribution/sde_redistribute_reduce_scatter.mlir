// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute,verify-sde-redistribute)' 2>&1 | %FileCheck %s

// A sibling-distributed intermediate %T has a committed block_contraction home
// (owner [0]); a consumer contracts it on its owned row axis, so the layout
// assignment marks %T in `layoutsDisagree`. sde-redistribute makes that
// committed redistribution edge explicit as a `reduce_scatter_like` movement
// over the home block layout (source == target; the movement is the reduction,
// not a re-layout), and verify-sde-redistribute grounds it (exit 0). The home
// layout is read verbatim from the committed `arrayLayout` write entry; the
// family comes from the consumer's grounded contraction (reductionIndexed) read.

// CHECK-LABEL: func.func @reduce_scatter_contraction
// CHECK: sde.redist <reduce_scatter_like> %{{.*}} : memref<256x256xf32> from owner [0] block [128, 256] to owner [0] block [128, 256] cost 2097152

func.func @reduce_scatter_contraction(%T: memref<256x256xf32>,
                                      %E: memref<256x256xf32>,
                                      %G: memref<256x256xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %cst = arith.constant 0.0 : f32
  sde.cu_region <parallel> {
    // Producer of %T: committed block_contraction home, owner [0].
    sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      memref.store %cst, %T[%i, %j] : memref<256x256xf32>
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write", commVolumeBytes = 0 : i64}]}
    // Consumer contracts %T on its row axis %k (a reduction read of the owned
    // dim) -> a cross-owner reduction redistribution edge.
    sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      scf.for %k = %c0 to %c256 step %c1 {
        %e = memref.load %E[%i, %k] : memref<256x256xf32>
        %t = memref.load %T[%k, %j] : memref<256x256xf32>
        %g = memref.load %G[%i, %j] : memref<256x256xf32>
        %p = arith.mulf %e, %t : f32
        %s = arith.addf %g, %p : f32
        memref.store %s, %G[%i, %j] : memref<256x256xf32>
      }
      sde.yield
    } {arrayLayout = [{arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "read", commVolumeBytes = 0 : i64}, {arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "read", commVolumeBytes = 2097152 : i64}, {arrayId = 2 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write", commVolumeBytes = 0 : i64}], layoutsDisagree = [0]}
    sde.yield
  }
  return
}
