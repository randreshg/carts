// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not=sde.su_halo --implicit-check-not=sde.su_iterate --implicit-check-not=arts.db_access_window --implicit-check-not='full-block halo byte-window'

// ARTS groups exact compact N-D halo pack tasks over the committed compute
// group while preserving one compact payload DB block per SDE MU/DB block.

// CHECK-LABEL: func.func @groups_compact_nd_halo_packs
// CHECK-COUNT-1: compact_halo_payload
// CHECK: scf.for %[[OUTER_IV:arg0]] = {{.*}} to {{.*}} step %c1{{(_[0-9]+)?}}
// CHECK: scf.for %[[INNER_IV:arg1]] = {{.*}} to {{.*}} step %c2{{(_[0-9]+)?}}
// CHECK: arts.db_acquire[<in>]{{.*}}offsets[%[[OUTER_IV]], %[[INNER_IV]]]
// CHECK-SAME: sizes[
// CHECK: arts.edt
// CHECK-SAME: compactHaloPack
// CHECK: scf.for %[[L0:[A-Za-z0-9_]+]] = {{.*}} to {{.*}} step
// CHECK: scf.for %[[L1:[A-Za-z0-9_]+]] = {{.*}} to {{.*}} step
// CHECK: arts.db_ref %{{.*}}[%[[L0]], %[[L1]]]
// CHECK: scf.if

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @groups_compact_nd_halo_packs() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c15 = arith.constant 15 : index
    %c16 = arith.constant 16 : index
    %zero = arith.constant 0.0 : f32
    %P = sde.mu_alloc {arrayId = 0 : i64} : memref<2x2x8x8xf32>
    %Q = sde.mu_alloc {arrayId = 1 : i64} : memref<2x2x8x8xf32>

    sde.su_distribute <blocked> {
      sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        sde.array_layout_root write %P : memref<2x2x8x8xf32> array_id(0)
        sde.array_layout_root write %Q : memref<2x2x8x8xf32> array_id(1)
        sde.cu_region <parallel> {
          %bi = arith.divui %i, %c8 : index
          %bj = arith.divui %j, %c8 : index
          %ei = arith.remui %i, %c8 : index
          %ej = arith.remui %j, %c8 : index
          memref.store %zero, %P[%bi, %bj, %ei, %ej] : memref<2x2x8x8xf32>
          memref.store %zero, %Q[%bi, %bj, %ei, %ej] : memref<2x2x8x8xf32>
        }
        sde.yield
      } {arrayLayout = [
        {arrayId = 0 : i64, blockShape = [8, 8], kind = "block_parallel",
         muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"},
        {arrayId = 1 : i64, blockShape = [8, 8], kind = "block_parallel",
         muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}]}
    }

    sde.su_distribute <owner_compute> {
      sde.su_halo %P : memref<2x2x8x8xf32> array_id(0) owner [0, 1] block [1, 1, 8, 8] halo [1, 1, 0, 0]
      sde.su_iterate (%c1, %c1) to (%c15, %c15) step (%c1, %c1) classification(<stencil>) {
      ^bb0(%i: index, %j: index):
        sde.array_layout_root read %P : memref<2x2x8x8xf32> array_id(0)
        sde.array_layout_root write %Q : memref<2x2x8x8xf32> array_id(1)
        sde.cu_region <parallel> {
          %im1 = arith.subi %i, %c1 : index
          %jm1 = arith.subi %j, %c1 : index
          %bi = arith.divui %im1, %c8 : index
          %bj = arith.divui %jm1, %c8 : index
          %ei = arith.remui %im1, %c8 : index
          %ej = arith.remui %jm1, %c8 : index
          %value = memref.load %P[%bi, %bj, %ei, %ej] : memref<2x2x8x8xf32>
          %wbi = arith.divui %i, %c8 : index
          %wbj = arith.divui %j, %c8 : index
          %wei = arith.remui %i, %c8 : index
          %wej = arith.remui %j, %c8 : index
          %out = arith.addf %value, %zero : f32
          memref.store %out, %Q[%wbi, %wbj, %wei, %wej] : memref<2x2x8x8xf32>
        } {groupBlockCount = [2, 2]}
        sde.yield
      } {arrayLayout = [
        {arrayId = 0 : i64, blockShape = [8, 8], kind = "block_parallel",
         muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"},
        {arrayId = 1 : i64, blockShape = [8, 8], kind = "block_parallel",
         muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}],
         accessMinOffsets = [-1, -1], accessMaxOffsets = [1, 1],
         ownerDims = [0, 1], spatialDims = [0, 1], writeFootprint = [1, 1]}
    }
    return
  }
}
