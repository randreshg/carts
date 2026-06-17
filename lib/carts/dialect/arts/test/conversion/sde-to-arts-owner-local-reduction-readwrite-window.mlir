// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db)' 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=STORAGE --implicit-check-not='partitioning(<coarse>)'
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=ARTS --implicit-check-not=sde.su_iterate --implicit-check-not=arts.db_access_window --implicit-check-not='partitioning(<coarse>)'

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @owner_local_reduction_readwrite_window() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %zero = arith.constant 0.0 : f32
    %C = sde.mu_alloc : memref<2x4xf32>

    sde.su_iterate (%c0) to (%c8) step (%c4) classification(<reduction>) {
    ^bb0(%i: index):
      sde.array_layout_root write %C : memref<2x4xf32> array_id(0)
      sde.cu_region <parallel> {
        %end = arith.addi %i, %c4 : index
        scf.for %ii = %i to %end step %c1 {
          %ib = arith.divui %i, %c4 : index
          %base = arith.remui %i, %c4 : index
          %off = arith.subi %ii, %i : index
          %il = arith.addi %base, %off : index
          memref.store %zero, %C[%ib, %il] : memref<2x4xf32>
          scf.for %k = %c0 to %c8 step %c1 {
            %old = memref.load %C[%ib, %il] : memref<2x4xf32>
            %sum = arith.addf %old, %zero : f32
            memref.store %sum, %C[%ib, %il] : memref<2x4xf32>
          }
        }
      }
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [4], muBlockCount = 2 : i64, role = "write"}]}
    return
  }
}

// STORAGE-LABEL: func.func @owner_local_reduction_readwrite_window
// STORAGE: arts.db_alloc
// STORAGE-SAME: <block>
// STORAGE: arts.db_access_window
// STORAGE-SAME: arrayId = 0 : i64
// STORAGE-SAME: mode = #arts.mode<inout>
// STORAGE-SAME: ownerDimCount = 1 : i64
// STORAGE-SAME: validExtents = [4]

// ARTS-LABEL: func.func @owner_local_reduction_readwrite_window
// ARTS: arts.edt
