// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute,verify-sde-redistribute)' 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}sde.redist: aliasing array-root use blocks redistribution

func.func @aliasing(%T: memref<256x256xf32>, %E: memref<256x256xf32>,
                    %G: memref<256x256xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %cst = arith.constant 0.0 : f32
  %alias = memref.cast %T : memref<256x256xf32> to memref<?x?xf32>
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %T : memref<256x256xf32> array_id(0)
    sde.cu_region <single> {
      memref.store %cst, %T[%i, %j] : memref<256x256xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write", commVolumeBytes = 0 : i64}]}
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %T : memref<256x256xf32> array_id(0)
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
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "read", commVolumeBytes = 2097152 : i64}], layoutsDisagree = [0]}
  return
}
