// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,sde-storage-to-arts-db)' 2>&1 | %FileCheck %s --check-prefix=STORAGE --implicit-check-not=sde.mu_alloc --implicit-check-not=sde.mu_access_window
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --check-prefix=ARTS --implicit-check-not=sde.mu_alloc --implicit-check-not=sde.mu_access_window --implicit-check-not=arts.db_access_window --implicit-check-not='partitioning(<coarse>)'

// Direct SDE-to-ARTS lowering consumes the committed access-window facts and
// creates the block DB, one-block acquire, and EDT.

// STORAGE-LABEL: func.func @consume_window_1d
// STORAGE: arts.db_alloc
// STORAGE-SAME: <block>
// STORAGE-SAME: elementSizes[%{{[^,]+}}, %c256]
// STORAGE: arts.db_access_window
// STORAGE-SAME: mode = #arts.mode<out>
// STORAGE-SAME: ownerDimCount = 1
// STORAGE-SAME: validExtents = [256]

// ARTS-LABEL: func.func @consume_window_1d
// ARTS: arts.db_alloc
// ARTS-SAME: <block>
// ARTS-SAME: elementSizes[%{{[^,]+}}, %c256]
// ARTS: arts.db_acquire
// ARTS-SAME: partitioning(<block>)
// ARTS-SAME: offsets[%{{[^]]+}}]
// ARTS-SAME: sizes[%{{[^]]+}}]
// ARTS: arts.edt
// ARTS: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<?x?xf32>

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @consume_window_1d() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c1024 = arith.constant 1024 : index
    %cst = arith.constant 1.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<1024xf32>
    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<1024xf32> array_id(0)
      sde.cu_region <parallel> {
        memref.store %cst, %A[%i] : memref<1024xf32>
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [256], muBlockCount = 4 : i64, role = "write", commVolumeBytes = 0 : i64}], physicalOwnerDims = [0], physicalBlockShape = [256]}
    return
  }
}
