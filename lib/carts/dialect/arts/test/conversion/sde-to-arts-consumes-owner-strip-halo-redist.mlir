// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not=sde.su_halo --implicit-check-not=sde.su_iterate --implicit-check-not=arts.db_access_window --implicit-check-not='owner rank of at least 2' --implicit-check-not='full-block halo byte-window'

// ARTS consumes a rank-expanded owner-strip halo where the owner rank is one
// but the payload tile is two-dimensional.

// CHECK-LABEL: func.func @consumes_owner_strip_halo_redist
// CHECK-COUNT-2: compact_halo_payload
// CHECK: compactHaloPack
// CHECK: scf.for [[DISPATCH:%[^ ]+]] = {{%c1[^ ]*}} to {{%c255[^ ]*}} step {{%c128[^ ]*}}
// CHECK: [[BLOCK_ID:%[^ ]+]] = arith.divui [[DISPATCH]], {{%c128[^ ]*}}
// CHECK: [[BLOCK_START:%[^ ]+]] = arith.muli [[BLOCK_ID]], {{%c128[^ ]*}}
// CHECK: [[NEXT_BLOCK:%[^ ]+]] = arith.addi [[BLOCK_ID]], {{%c1[^ ]*}}
// CHECK: [[BLOCK_END:%[^ ]+]] = arith.muli [[NEXT_BLOCK]], {{%c128[^ ]*}}
// CHECK-DAG: [[LOCAL_LO:%[^ ]+]] = arith.maxui [[BLOCK_START]], {{%c1[^ ]*}}
// CHECK-DAG: [[LOCAL_HI:%[^ ]+]] = arith.minui [[BLOCK_END]], {{%c255[^ ]*}}
// CHECK: arts.edt
// CHECK-SAME: params([[LOCAL_LO]]
// CHECK-SAME: [[LOCAL_HI]]

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @consumes_owner_strip_halo_redist() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    %c255 = arith.constant 255 : index
    %c256 = arith.constant 256 : index
    %zero = arith.constant 0.0 : f64
    %P = sde.mu_alloc {arrayId = 0 : i64} : memref<2x128x256xf64>
    %Q = sde.mu_alloc {arrayId = 1 : i64} : memref<2x128x256xf64>

    sde.su_distribute <blocked> {
      sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        sde.array_layout_root write %P : memref<2x128x256xf64> array_id(0)
        sde.array_layout_root write %Q : memref<2x128x256xf64> array_id(1)
        sde.cu_region <parallel> {
          %bi = arith.divui %i, %c128 : index
          %li = arith.remui %i, %c128 : index
          memref.store %zero, %P[%bi, %li, %j] : memref<2x128x256xf64>
          memref.store %zero, %Q[%bi, %li, %j] : memref<2x128x256xf64>
        }
        sde.yield
      } {arrayLayout = [
        {arrayId = 0 : i64, blockShape = [128, 256],
         budgetBlockShape = [256, 256], kind = "block_parallel",
         muBlockCount = 2 : i64, ownerDims = [0], role = "write"},
        {arrayId = 1 : i64, blockShape = [128, 256],
         kind = "block_parallel", muBlockCount = 2 : i64,
         ownerDims = [0], role = "write"}]}
    }

    sde.su_distribute <owner_compute> {
      sde.su_halo %P : memref<2x128x256xf64> array_id(0) owner [0] block [1, 128, 256] halo [1, 0, 0]
      sde.su_iterate (%c1) to (%c255) step (%c1) classification(<stencil>) {
      ^bb0(%i: index):
        sde.array_layout_root read %P : memref<2x128x256xf64> array_id(0)
        sde.array_layout_root write %Q : memref<2x128x256xf64> array_id(1)
        sde.cu_region <parallel> {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          scf.for %j = %c0 to %c256 step %c1 {
            %b0 = arith.divui %im1, %c128 : index
            %l0 = arith.remui %im1, %c128 : index
            %left = memref.load %P[%b0, %l0, %j] : memref<2x128x256xf64>
            %b1 = arith.divui %ip1, %c128 : index
            %l1 = arith.remui %ip1, %c128 : index
            %right = memref.load %P[%b1, %l1, %j] : memref<2x128x256xf64>
            %sum = arith.addf %left, %right : f64
            %bo = arith.divui %i, %c128 : index
            %lo = arith.remui %i, %c128 : index
            %out = arith.addf %sum, %zero : f64
            memref.store %out, %Q[%bo, %lo, %j] : memref<2x128x256xf64>
          }
        }
        sde.yield
      } {arrayLayout = [
        {arrayId = 0 : i64, blockShape = [128, 256],
         budgetBlockShape = [256, 256], kind = "block_parallel",
         muBlockCount = 2 : i64, ownerDims = [0], role = "read"},
        {arrayId = 1 : i64, blockShape = [128, 256],
         kind = "block_parallel", muBlockCount = 2 : i64,
         ownerDims = [0], role = "write"}],
         accessMinOffsets = [-1, 0], accessMaxOffsets = [1, 0],
         ownerDims = [0], spatialDims = [0, 1], writeFootprint = [0, 0]}
    }
    return
  }
}
