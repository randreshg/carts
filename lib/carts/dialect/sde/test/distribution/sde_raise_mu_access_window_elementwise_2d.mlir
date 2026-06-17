// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,sde-storage-to-arts-db)' 2>&1 | %FileCheck %s

// After rank expansion converts the committed single-owner block layout
// () into memref<8x16x64xf32>, query-derived boundary lowering stamps
// arts.db_access_window on the expanded written MU root in the SAME block-grid
// coordinate system: C is write-only (out window) spanning the full grid [0, 8)
// with in-tile valid extent [16, 64].

// CHECK-LABEL: func.func @raise_window_elementwise_2d
// CHECK: arts.db_alloc
// CHECK-SAME: <coarse>
// CHECK: memref.cast {{.*}} to memref<128x64xf32>
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK: memref.cast {{.*}} to memref<16x64xf32>
// CHECK: memref.expand_shape {{.*}} into memref<1x16x64xf32>
// CHECK-NOT: sde.mu_access_window
// CHECK: arts.db_access_window
// CHECK-SAME: mode = #arts.mode<out>
// CHECK-SAME: ownerDimCount = 1
// CHECK-SAME: validExtents = [16, 64]

func.func @raise_window_elementwise_2d() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc : memref<128x64xf32>
  %C = sde.mu_alloc : memref<128x64xf32>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.array_layout_root write %C : memref<128x64xf32> array_id(0)
    sde.cu_region <single> {
      scf.for %j = %c0 to %c64 step %c1 {
        %v = memref.load %A[%i, %j] : memref<128x64xf32>
        memref.store %v, %C[%i, %j] : memref<128x64xf32>
    }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [16, 64], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "write"}]}
  return
}
