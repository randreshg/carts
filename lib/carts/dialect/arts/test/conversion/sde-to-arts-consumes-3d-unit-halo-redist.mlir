// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not=sde.redist --implicit-check-not=sde.su_halo --implicit-check-not=sde.su_iterate --implicit-check-not=arts.db_access_window --implicit-check-not='full-block halo byte-window' --implicit-check-not=local_only

// ARTS consumes a committed 3D unit halo movement as exact compact side,
// edge, and corner payload DBs. The physical owner dims are intentionally
// permuted with asymmetric block sizes; ARTS must read full-rank
// physicalBlockShape by physical dim, not by owner-slot order.

// CHECK-LABEL: func.func @consumes_3d_unit_halo_redist
// CHECK-COUNT-26: compact_halo_payload
// CHECK: compactHaloPack
// CHECK: scf.if

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @consumes_3d_unit_halo_redist() {
    %c1 = arith.constant 1 : index
    %c3 = arith.constant 3 : index
    %c5 = arith.constant 5 : index
    %c7 = arith.constant 7 : index
    %c11 = arith.constant 11 : index
    %c13 = arith.constant 13 : index
    %c14 = arith.constant 14 : index
    %P = sde.mu_alloc : memref<2x3x4x7x5x3xf32>
    %Q = sde.mu_alloc : memref<2x3x4x7x5x3xf32>

    sde.su_distribute <owner_compute> {
      sde.su_halo %P : memref<2x3x4x7x5x3xf32> array_id(0) owner [0, 1, 2] block [1, 1, 1, 7, 5, 3] halo [1, 1, 1, 0, 0, 0]
      sde.su_iterate (%c1, %c1, %c1) to (%c13, %c14, %c11) step (%c7, %c5, %c3) classification(<stencil>) {
      ^bb0(%ib: index, %jb: index, %kb: index):
        sde.cu_region <parallel> {
          sde.mu_access_window read %P : memref<2x3x4x7x5x3xf32> array_id(0)
          sde.mu_access_window write %Q : memref<2x3x4x7x5x3xf32> array_id(1)
          %iend_raw = arith.addi %ib, %c7 : index
          %iend = arith.minui %iend_raw, %c13 : index
          scf.for %i = %ib to %iend step %c1 {
            %jend_raw = arith.addi %jb, %c5 : index
            %jend = arith.minui %jend_raw, %c14 : index
            scf.for %j = %jb to %jend step %c1 {
              %kend_raw = arith.addi %kb, %c3 : index
              %kend = arith.minui %kend_raw, %c11 : index
              scf.for %k = %kb to %kend step %c1 {
                %im1 = arith.subi %i, %c1 : index
                %jp1 = arith.addi %j, %c1 : index
                %km1 = arith.subi %k, %c1 : index
                %bi = arith.divui %im1, %c7 : index
                %bj = arith.divui %jp1, %c5 : index
                %bk = arith.divui %km1, %c3 : index
                %ei = arith.remui %im1, %c7 : index
                %ej = arith.remui %jp1, %c5 : index
                %ek = arith.remui %km1, %c3 : index
                %value = memref.load %P[%bi, %bj, %bk, %ei, %ej, %ek] : memref<2x3x4x7x5x3xf32>
                %wbi = arith.divui %i, %c7 : index
                %wbj = arith.divui %j, %c5 : index
                %wbk = arith.divui %k, %c3 : index
                %wei = arith.remui %i, %c7 : index
                %wej = arith.remui %j, %c5 : index
                %wek = arith.remui %k, %c3 : index
                memref.store %value, %Q[%wbi, %wbj, %wbk, %wei, %wej, %wek] : memref<2x3x4x7x5x3xf32>
              }
            }
          }
        }
        sde.yield
      } {accessMaxOffsets = [1, 1, 1], accessMinOffsets = [-1, -1, -1], ownerDims = [2, 1, 0], pattern = #sde.pattern<cross_dim_stencil_3d>, spatialDims = [0, 1, 2], writeFootprint = [1, 1, 1]}
    }
    return
  }
}
