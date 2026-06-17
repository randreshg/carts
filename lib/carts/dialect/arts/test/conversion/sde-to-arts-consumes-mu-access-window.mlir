// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,sde-storage-to-arts-db)' 2>&1 | %FileCheck %s --check-prefix=STORAGE --implicit-check-not=sde.mu_alloc --implicit-check-not=sde.mu_access_window
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --check-prefix=ARTS --implicit-check-not=sde.mu_alloc --implicit-check-not=sde.mu_access_window --implicit-check-not=arts.db_access_window --implicit-check-not='partitioning(<coarse>)'

// Direct SDE-to-ARTS lowering consumes committed access-window facts.

// STORAGE-LABEL: func.func @consume_window_1d
// STORAGE: arts.db_alloc
// STORAGE-SAME: <block>
// STORAGE-SAME: elementSizes[%c256]
// STORAGE: arts.db_access_window
// STORAGE-SAME: mode = #arts.mode<out>
// STORAGE-SAME: ownerDimCount = 1
// STORAGE-SAME: validExtents = [256]

// ARTS-LABEL: func.func @consume_window_1d
// ARTS: arts.db_alloc
// ARTS-SAME: <block>
// ARTS-SAME: elementSizes[%c256]
// ARTS: arts.db_acquire
// ARTS-SAME: partitioning(<block>)
// ARTS-SAME: offsets[%{{[^]]+}}]
// ARTS-SAME: sizes[%{{[^]]+}}]
// ARTS: arts.edt
// ARTS: memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<?xf32>

// ARTS-LABEL: func.func @consume_full_rank_partial_window
// ARTS: arts.db_alloc
// ARTS-SAME: <block>
// ARTS-SAME: elementSizes[%{{[^,]+}}, %{{[^,]+}}, %{{[^]]+}}]
// ARTS: %[[GROUP:.*]] = arith.constant 4 : index
// ARTS: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %[[GROUP]]
// ARTS: arts.db_acquire
// ARTS-SAME: partitioning(<block>)
// ARTS: arts.edt
// ARTS: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}] : memref<?x?x?xf64>

// ARTS-LABEL: func.func @consume_rank_expanded_read_window_offsets
// ARTS: scf.for %[[B:.*]] = %{{.*}} to %{{.*}} step %{{.*}} {
// ARTS: arts.db_acquire[<out>]
// ARTS-SAME: offsets[
// ARTS: arts.db_acquire[<in>]
// ARTS-SAME: offsets[
// ARTS: arts.edt
// ARTS: memref.load

// ARTS-LABEL: func.func @consume_nonleading_read_window_offsets
// ARTS: scf.for %[[BASE:.*]] = %{{.*}} to %{{.*}} step %{{.*}} {
// ARTS: %[[TWO:.*]] = arith.constant 2 : index
// ARTS: %[[A_OFFSET:.*]] = arith.divui %{{.*}}, %[[TWO]] : index
// ARTS: arts.db_acquire[<in>]
// ARTS-SAME: offsets[%[[A_OFFSET]]]
// ARTS: arts.edt
// ARTS: %[[LOAD_BLOCK:.*]] = arith.divui %{{.*}}, %{{.*}} : index
// ARTS: %[[LOCAL_BLOCK:.*]] = arith.subi %[[LOAD_BLOCK]], %{{.*}} : index
// ARTS: arts.db_ref %{{.*}}[%[[LOCAL_BLOCK]]]

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
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [256], muBlockCount = 4 : i64, role = "write"}]}
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
        scf.for %slot = %c0 to %c16 step %c1 {
          memref.store %cst, %P[%b, %c0, %slot, %c0] : memref<8x1x16x1xf64>
        }
      } {groupBlockCount = [4]}
    } {arrayLayout = [{arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [1, 16, 1], muBlockCount = 8 : i64, role = "write"}]}
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
        scf.for %slot = %c0 to %c16 step %c1 {
          %v = memref.load %A[%b, %slot] : memref<8x16xf32>
          %w = arith.extf %v : f32 to f64
          memref.store %w, %P[%b, %c0, %slot, %c0] : memref<8x1x16x1xf64>
        }
      }
    } {arrayLayout = [{arrayId = 2 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [1, 16], muBlockCount = 8 : i64, role = "read"}, {arrayId = 3 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [1, 16, 1], muBlockCount = 8 : i64, role = "write"}]}
    return
  }

  func.func @consume_nonleading_read_window_offsets() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %c32 = arith.constant 32 : index
    %A = sde.mu_alloc {arrayId = 4 : i64} : memref<16x32x2xf32>
    %P = sde.mu_alloc {arrayId = 5 : i64} : memref<4x8xf32>
    sde.su_iterate (%c0) to (%c32) step (%c8) classification(<elementwise_pipeline>) {
    ^bb0(%ch: index):
      sde.array_layout_root read %A : memref<16x32x2xf32> array_id(4)
      sde.array_layout_root write %P : memref<4x8xf32> array_id(5)
      sde.cu_region <parallel> {
        %hi = arith.addi %ch, %c8 : index
        scf.for %i = %ch to %hi step %c1 {
          %a_block = arith.divui %i, %c2 : index
          %a_lane = arith.remui %i, %c2 : index
          %v = memref.load %A[%a_block, %c0, %a_lane] : memref<16x32x2xf32>
          %p_block = arith.divui %i, %c8 : index
          %p_lane = arith.remui %i, %c8 : index
          memref.store %v, %P[%p_block, %p_lane] : memref<4x8xf32>
        }
      }
    } {arrayLayout = [{arrayId = 4 : i64, kind = "block_parallel", ownerDims = [1], blockShape = [32, 16], budgetBlockShape = [32, 2], muBlockCount = 16 : i64, role = "read"}, {arrayId = 5 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [8], muBlockCount = 4 : i64, role = "write"}]}
    return
  }
}
