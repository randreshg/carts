// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// sde-redistribute consumes the temporary mismatch marker into one explicit
// reduce_scatter_like movement and removes the marker before ARTS.

// CHECK-LABEL: func.func @reduce_scatter_contraction
// CHECK: sde.su_distribute <owner_compute>
// CHECK: sde.su_reduce_scatter %{{.*}} : memref<256x256xf32> array_id(0) owner [0] block [128, 256] reduce 0 kind <add>

func.func @reduce_scatter_contraction(%T: memref<256x256xf32>,
                                      %E: memref<256x256xf32>,
                                      %G: memref<256x256xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %cst = arith.constant 0.0 : f32
  // Producer of %T: committed block_contraction home, owner [0].
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %T : memref<256x256xf32> array_id(0)
    sde.cu_region <single> {
      memref.store %cst, %T[%i, %j] : memref<256x256xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write"}]}
  // Consumer reduces through %T on its row axis %k (a reduction read of the owned
  // dim) -> a cross-owner reduction redistribution edge.
  sde.su_distribute <owner_compute> {
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %E : memref<256x256xf32> array_id(1)
    sde.array_layout_root read %T : memref<256x256xf32> array_id(0)
    sde.array_layout_root write %G : memref<256x256xf32> array_id(2)
    sde.cu_region <single> {
      scf.for %k = %c0 to %c256 step %c1 {
        %e = memref.load %E[%i, %k] : memref<256x256xf32>
        %t = memref.load %T[%k, %j] : memref<256x256xf32>
        %g = memref.load %G[%i, %j] : memref<256x256xf32>
        %p = arith.mulf %e, %t : f32
        %s = arith.addf %g, %p : f32
        memref.store %s, %G[%i, %j] : memref<256x256xf32>
    }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "read"}, {arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "read"}, {arrayId = 2 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write"}]}
  }
  return
}
