// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Same-CU read/write normally becomes one readwrite window. When the same MU
// also has a committed halo read, SDE must split the read and write windows so
// downstream ARTS never materializes a writable halo dependency.

// CHECK-LABEL: func.func @split_writable_halo_window
// CHECK: sde.mu_access_window read %[[A:.*]] : memref<8x16x64xf32> array_id(0) owner_dims(1) block_lo [0] block_hi [8] valid [16, 64]
// CHECK: sde.mu_access_window write %[[A]] : memref<8x16x64xf32> array_id(0) owner_dims(1) block_lo [0] block_hi [8] valid [16, 64]
// CHECK-NOT: sde.mu_access_window readwrite %[[A]]

func.func @split_writable_halo_window() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc {arrayId = 0 : i64} : memref<128x64xf32>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<stencil>) {
  ^bb0(%i: index):
    sde.array_layout_root read %A : memref<128x64xf32> array_id(0)
    sde.cu_region <single> {
      scf.for %j = %c0 to %c64 step %c1 {
        %v = memref.load %A[%i, %j] : memref<128x64xf32>
        %w = arith.addf %v, %v : f32
        memref.store %w, %A[%i, %j] : memref<128x64xf32>
      }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [16, 64], budgetBlockShape = [16, 64], budgetMuBlockCount = 8 : i64, commVolumeBytes = 1024 : i64, kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "read"}], layoutsDisagree = [0], physicalOwnerDims = [0], physicalBlockShape = [16, 64], physicalHaloShape = [1]}
  return
}
