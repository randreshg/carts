// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,sde-storage-to-arts-db)' 2>&1 | %FileCheck %s --check-prefix=STORAGE --implicit-check-not=sde.mu_alloc --implicit-check-not=sde.mu_access_window
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --check-prefix=ARTS --implicit-check-not=sde.mu_alloc --implicit-check-not=sde.mu_access_window --implicit-check-not=arts.db_access_window --implicit-check-not='partitioning(<coarse>)'

// A committed BLOCK layout is enough to author access windows even when the SU
// body is not classifiable by loop-pattern analysis.

// STORAGE-LABEL: func.func @unclassified_committed_block_layout_window
// STORAGE: arts.db_alloc
// STORAGE-SAME: <block>
// STORAGE: arts.db_access_window
// STORAGE-SAME: mode = #arts.mode<out>
// STORAGE-SAME: ownerDimCount = 1
// STORAGE-SAME: validExtents = [2, 16]

// ARTS-LABEL: func.func @unclassified_committed_block_layout_window
// ARTS: arts.db_alloc
// ARTS-SAME: <block>
// ARTS: arts.db_acquire
// ARTS-SAME: partitioning(<block>)
// ARTS: arts.edt
// ARTS: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}] : memref<?x?x?xf32>

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @unclassified_committed_block_layout_window() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %one = arith.constant 1.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<4x16xf32>
    sde.su_iterate (%c0) to (%c4) step (%c1) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<4x16xf32> array_id(0)
      sde.cu_region <parallel> {
        func.call @opaque() : () -> ()
        scf.for %j = %c0 to %c16 step %c1 {
          memref.store %one, %A[%i, %j] : memref<4x16xf32>
        }
      }
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [2, 16],
         kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0],
         role = "write"}]}
    return
  }

  func.func private @opaque()
}
