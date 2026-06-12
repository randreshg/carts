// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,sde-storage-to-arts-db)' 2>&1 | %FileCheck %s --check-prefix=STORAGE --implicit-check-not=sde.mu_alloc --implicit-check-not=sde.mu_access_window
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --check-prefix=ARTS --implicit-check-not=sde.mu_alloc --implicit-check-not=sde.mu_access_window --implicit-check-not=arts.db_access_window --implicit-check-not='partitioning(<coarse>)'

// Direct SDE-to-ARTS lowering consumes committed access-window facts.

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

// ARTS-LABEL: func.func @consume_full_rank_partial_window
// ARTS: arts.db_alloc
// ARTS-SAME: <block>
// ARTS-SAME: elementSizes[%{{[^,]+}}, %{{[^,]+}}, %{{[^,]+}}, %{{[^]]+}}]
// ARTS: arts.db_acquire
// ARTS-SAME: partitioning(<block>)
// ARTS: arts.edt
// ARTS: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<?x?x?x?xf64>

// ARTS-LABEL: func.func @consume_rank_expanded_read_window_offsets
// ARTS: scf.for %[[B:.*]] = %{{.*}} to %{{.*}} step %{{.*}} {
// ARTS: %[[BLOCK:.*]] = arith.divui %{{.*}}, %{{.*}} : index
// ARTS: arts.db_acquire[<in>]
// ARTS-SAME: offsets[%[[BLOCK]]]
// ARTS-SAME: sizes[%{{.*}}]
// ARTS-NOT: arts.db_acquire[<in>]
// ARTS: arts.db_acquire[<out>]
// ARTS-SAME: offsets[%[[BLOCK]]]
// ARTS: arts.edt
// ARTS: memref.load %{{.*}}[%{{.*}}, %{{.*}}] : memref<?x?xf32>

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

  func.func @consume_full_rank_partial_window() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %cst = arith.constant 1.0 : f64
    %P = sde.mu_alloc {arrayId = 1 : i64} : memref<8x1x16x1xf64>
    sde.su_iterate (%c0) to (%c8) step (%c1) classification(<elementwise>) {
    ^bb0(%b: index):
      sde.array_layout_root write %P : memref<8x1x16x1xf64> array_id(1)
      sde.cu_region <parallel> {
        sde.mu_access_window write %P : memref<8x1x16x1xf64> array_id(1) owner_dims(1) block_lo [0] block_hi [8] valid [1, 16, 1]
        scf.for %slot = %c0 to %c16 step %c1 {
          memref.store %cst, %P[%b, %c0, %slot, %c0] : memref<8x1x16x1xf64>
        }
      }
    } {arrayLayout = [{arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [1, 16, 1], muBlockCount = 8 : i64, role = "write"}], iterationTopology = #sde.iteration_topology<owner_strip>, logicalWorkerSlice = [1], physicalOwnerDims = [0], physicalBlockShape = [1, 16, 1]}
    return
  }

  func.func @consume_rank_expanded_read_window_offsets() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %A = sde.mu_alloc {arrayId = 2 : i64} : memref<8x16xf32>
    %P = sde.mu_alloc {arrayId = 3 : i64} : memref<8x1x16x1xf64>
    sde.su_iterate (%c0) to (%c8) step (%c1) classification(<elementwise>) {
    ^bb0(%b: index):
      sde.array_layout_root read %A : memref<8x16xf32> array_id(2)
      sde.array_layout_root write %P : memref<8x1x16x1xf64> array_id(3)
      sde.cu_region <parallel> {
        sde.mu_access_window read %A : memref<8x16xf32> array_id(2) owner_dims(1) block_lo [0] block_hi [8] valid [16]
        sde.mu_access_window write %P : memref<8x1x16x1xf64> array_id(3) owner_dims(1) block_lo [0] block_hi [8] valid [1, 16, 1]
        scf.for %slot = %c0 to %c16 step %c1 {
          %v = memref.load %A[%b, %slot] : memref<8x16xf32>
          %w = arith.extf %v : f32 to f64
          memref.store %w, %P[%b, %c0, %slot, %c0] : memref<8x1x16x1xf64>
        }
      }
    } {arrayLayout = [{arrayId = 2 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [1, 16], muBlockCount = 8 : i64, role = "read"}, {arrayId = 3 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [1, 16, 1], muBlockCount = 8 : i64, role = "write"}], iterationTopology = #sde.iteration_topology<owner_strip>, logicalWorkerSlice = [1], physicalOwnerDims = [0], physicalBlockShape = [1, 16, 1]}
    return
  }
}
