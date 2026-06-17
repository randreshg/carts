// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not=sde.su_halo --implicit-check-not=sde.su_iterate --implicit-check-not=arts.db_access_window --implicit-check-not='full-block halo byte-window'

// Compact halo payload writers are grouped only up to a proven owner-local DB
// block range. A 196-block group over a 389-block owner-contiguous grid crosses
// the validation 2-node boundary; 195 is the largest uniform route-local group.

// CHECK-LABEL: func.func @caps_compact_halo_payload_writer_groups
// CHECK-COUNT-2: compact_halo_payload
// CHECK: %[[C195:[A-Za-z0-9_]+]] = arith.constant 195 : index
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %[[C195]]
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: sizes[
// CHECK: arts.edt
// CHECK-SAME: compactHaloPack
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %[[C195]]
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: sizes[
// CHECK: arts.edt
// CHECK-SAME: compactHaloPack

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64} {
  func.func @caps_compact_halo_payload_writer_groups() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c26 = arith.constant 26 : index
    %c10111 = arith.constant 10111 : index
    %c10112 = arith.constant 10112 : index
    %zero = arith.constant 0.0 : f64
    %P = sde.mu_alloc {arrayId = 0 : i64} : memref<389x26x4xf64>
    %Q = sde.mu_alloc {arrayId = 1 : i64} : memref<389x26x4xf64>

    sde.su_distribute <blocked> {
      sde.su_iterate (%c0, %c0) to (%c10112, %c4) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        sde.array_layout_root write %P : memref<389x26x4xf64> array_id(0)
        sde.array_layout_root write %Q : memref<389x26x4xf64> array_id(1)
        sde.cu_region <parallel> {
          %bi = arith.divui %i, %c26 : index
          %li = arith.remui %i, %c26 : index
          memref.store %zero, %P[%bi, %li, %j] : memref<389x26x4xf64>
          memref.store %zero, %Q[%bi, %li, %j] : memref<389x26x4xf64>
        }
        sde.yield
      } {arrayLayout = [
        {arrayId = 0 : i64, blockShape = [26, 4],
         kind = "block_parallel", muBlockCount = 389 : i64,
         ownerDims = [0], role = "write"},
        {arrayId = 1 : i64, blockShape = [26, 4],
         kind = "block_parallel", muBlockCount = 389 : i64,
         ownerDims = [0], role = "write"}]}
    }

    sde.su_distribute <owner_compute> {
      sde.su_halo %P : memref<389x26x4xf64> array_id(0) owner [0] block [1, 26, 4] halo [1, 0, 0]
      sde.su_iterate (%c1) to (%c10111) step (%c1) classification(<stencil>) {
      ^bb0(%i: index):
        sde.array_layout_root read %P : memref<389x26x4xf64> array_id(0)
        sde.array_layout_root write %Q : memref<389x26x4xf64> array_id(1)
        sde.cu_region <parallel> {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          scf.for %j = %c0 to %c4 step %c1 {
            %b0 = arith.divui %im1, %c26 : index
            %l0 = arith.remui %im1, %c26 : index
            %left = memref.load %P[%b0, %l0, %j] : memref<389x26x4xf64>
            %b1 = arith.divui %ip1, %c26 : index
            %l1 = arith.remui %ip1, %c26 : index
            %right = memref.load %P[%b1, %l1, %j] : memref<389x26x4xf64>
            %sum = arith.addf %left, %right : f64
            %bo = arith.divui %i, %c26 : index
            %lo = arith.remui %i, %c26 : index
            memref.store %sum, %Q[%bo, %lo, %j] : memref<389x26x4xf64>
          }
        } {groupBlockCount = [196]}
        sde.yield
      } {arrayLayout = [
        {arrayId = 0 : i64, blockShape = [26, 4],
         kind = "block_parallel", muBlockCount = 389 : i64,
         ownerDims = [0], role = "read"},
        {arrayId = 1 : i64, blockShape = [26, 4],
         kind = "block_parallel", muBlockCount = 389 : i64,
         ownerDims = [0], role = "write"}],
         accessMinOffsets = [-1, 0], accessMaxOffsets = [1, 0],
         ownerDims = [0], spatialDims = [0, 1], writeFootprint = [0, 0]}
    }
    return
  }
}
