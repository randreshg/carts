// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// A single-output initializer that writes a rank-expanded DB can be the source
// for a later stencil halo. SDE must author the producer write fact from the
// committed consumer layout before emitting the halo movement.

// CHECK-LABEL: func.func @halo_backed_init_writer
// CHECK: sde.array_layout_root write %[[A:.*]] : memref<2x2x8x8xf32> array_id(1)
// CHECK: arrayLayout = [{arrayId = 1 : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "write"
// CHECK: sde.su_halo

func.func @halo_backed_init_writer() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c8 = arith.constant 8 : index
  %zero = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<2x2x8x8xf32>
  %B = sde.mu_alloc : memref<2x2x8x8xf32>
  sde.su_distribute <blocked> {
    sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      sde.cu_region <parallel> {
        %bi = arith.divui %i, %c8 : index
        %bj = arith.divui %j, %c8 : index
        %li = arith.remui %i, %c8 : index
        %lj = arith.remui %j, %c8 : index
        memref.store %zero, %A[%bi, %bj, %li, %lj]
            : memref<2x2x8x8xf32>
        sde.yield
      }
      sde.yield
    }
  }
  sde.su_distribute <owner_compute> {
    sde.su_iterate (%c1, %c1) to (%c16, %c16) step (%c1, %c1)
        classification(<stencil>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root read %A : memref<2x2x8x8xf32> array_id(1)
      sde.array_layout_root write %B : memref<2x2x8x8xf32> array_id(0)
      sde.cu_region <parallel> {
        %bi = arith.divui %i, %c8 : index
        %bj = arith.divui %j, %c8 : index
        %li = arith.remui %i, %c8 : index
        %lj = arith.remui %j, %c8 : index
        %v = memref.load %A[%bi, %bj, %li, %lj] : memref<2x2x8x8xf32>
        memref.store %v, %B[%bi, %bj, %li, %lj] : memref<2x2x8x8xf32>
        sde.yield
      }
      sde.yield
    } {accessMinOffsets = [-1, -1], accessMaxOffsets = [1, 1],
       arrayLayout = [
         {arrayId = 0 : i64, blockShape = [8, 8], kind = "block_parallel",
          muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"},
         {arrayId = 1 : i64, blockShape = [8, 8], kind = "block_parallel",
          muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"}],
       ownerDims = [0, 1], pattern = #sde.pattern<stencil_tiling_nd>,
       spatialDims = [0, 1], writeFootprint = [1, 1]}
  }
  return
}
