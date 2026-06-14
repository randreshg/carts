// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline=sde-planning | %FileCheck %s

// A full-rank stencil writer may inherit the compatible co-iterated read's
// owner dims. SDE must commit the writer MU at the same owner tile grain and
// keep the same-owner stencil read on halo movement, never all_to_all.

// CHECK-LABEL: func.func @coiterated_stencil_writer_layout
// CHECK-DAG: %[[IN:.*]] = sde.mu_alloc : memref<2x2x128x128xf32>
// CHECK-DAG: %[[OUT:.*]] = sde.mu_alloc : memref<2x2x128x128xf32>
// CHECK: sde.su_distribute <owner_compute>
// CHECK: sde.su_halo %[[IN]] : memref<2x2x128x128xf32> array_id(0) owner [0, 1] block [1, 1, 128, 128] halo [1, 0, 0, 0]
// CHECK-NOT: sde.su_all_to_all
// CHECK: sde.array_layout_root write %[[OUT]] : memref<2x2x128x128xf32> array_id(1)

func.func @coiterated_stencil_writer_layout() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %zero = arith.constant 0.0 : f32
  %src = memref.alloc() : memref<256x256xf32>
  %dst = sde.mu_alloc : memref<256x256xf32>
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %src : memref<256x256xf32> array_id(0)
    sde.cu_region <parallel> {
      memref.store %zero, %src[%i, %j] : memref<256x256xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [64, 64],
       kind = "block_parallel", muBlockCount = 16 : i64, ownerDims = [0, 1],
       role = "write"}]}
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1)
      classification(<stencil>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %src : memref<256x256xf32> array_id(0)
    sde.array_layout_root write %dst : memref<256x256xf32> array_id(1)
    sde.cu_region <parallel> {
      %im1 = arith.subi %i, %c1 : index
      %v = memref.load %src[%im1, %j] : memref<256x256xf32>
      memref.store %v, %dst[%i, %j] : memref<256x256xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [64, 64],
       kind = "block_parallel", muBlockCount = 16 : i64, ownerDims = [0, 1],
       role = "read"},
      {arrayId = 1 : i64, blockShape = [64, 64],
       kind = "block_parallel", muBlockCount = 16 : i64,
       role = "write"}],
     accessMinOffsets = [-1, 0], accessMaxOffsets = [0, 0],
     ownerDims = [0, 1], spatialDims = [0, 1], writeFootprint = [0, 0]}
  return
}
