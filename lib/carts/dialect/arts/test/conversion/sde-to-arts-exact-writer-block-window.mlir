// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not=sde.mu_alloc --implicit-check-not=arts.db_access_window --implicit-check-not='partitioning(<coarse>)'

// A block-aligned writer dispatch writes one owner DB block. ARTS must not
// widen the exclusive writer acquire to adjacent blocks.

// CHECK-LABEL: func.func @exact_writer_block_window_2d
// CHECK: %[[I_BOUND:.*]] = arith.minui %{{.*}}, %c1{{(_[0-9]+)?}} : index
// CHECK: %[[I_SIZE:.*]] = arith.minui %{{.*}}, %[[I_BOUND]] : index
// CHECK: %[[J_BOUND:.*]] = arith.minui %{{.*}}, %c1{{(_[0-9]+)?}} : index
// CHECK: %[[J_SIZE:.*]] = arith.minui %{{.*}}, %[[J_BOUND]] : index
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: sizes[%[[I_SIZE]], %[[J_SIZE]]]
// CHECK: arts.edt
// CHECK: arts.db_ref %{{.*}}[%c0{{(_[0-9]+)?}}, %c0{{(_[0-9]+)?}}]
// CHECK: memref.store
// CHECK-LABEL: func.func @exact_shifted_writer_block_window_2d
// CHECK: scf.for %{{.*}} = %c1{{(_[0-9]+)?}} to %c7{{(_[0-9]+)?}} step %c4{{(_[0-9]+)?}}
// CHECK: %[[I_BOUND:.*]] = arith.minui %{{.*}}, %c1{{(_[0-9]+)?}} : index
// CHECK: %[[I_SIZE:.*]] = arith.minui %{{.*}}, %[[I_BOUND]] : index
// CHECK: %[[J_BOUND:.*]] = arith.minui %{{.*}}, %c1{{(_[0-9]+)?}} : index
// CHECK: %[[J_SIZE:.*]] = arith.minui %{{.*}}, %[[J_BOUND]] : index
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: sizes[%[[I_SIZE]], %[[J_SIZE]]]
// CHECK-LABEL: func.func @exact_shifted_inout_writer_block_window_2d
// CHECK: scf.for %{{.*}} = %c1{{(_[0-9]+)?}} to %c7{{(_[0-9]+)?}} step %c4{{(_[0-9]+)?}}
// CHECK: %[[I_BOUND:.*]] = arith.minui %{{.*}}, %c1{{(_[0-9]+)?}} : index
// CHECK: %[[I_SIZE:.*]] = arith.minui %{{.*}}, %[[I_BOUND]] : index
// CHECK: %[[J_BOUND:.*]] = arith.minui %{{.*}}, %c1{{(_[0-9]+)?}} : index
// CHECK: %[[J_SIZE:.*]] = arith.minui %{{.*}}, %[[J_BOUND]] : index
// CHECK: arts.db_acquire[<inout>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: sizes[%[[I_SIZE]], %[[J_SIZE]]]

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @exact_writer_block_window_2d() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %zero = arith.constant 0.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<8x8xf32>

    sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c4, %c4) classification(<elementwise>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %A : memref<8x8xf32> array_id(0)
      sde.cu_region <parallel> {
        memref.store %zero, %A[%i, %j] : memref<8x8xf32>
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [4, 4], muBlockCount = 4 : i64, role = "write"}]}
    return
  }

  func.func @exact_shifted_writer_block_window_2d() {
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c7 = arith.constant 7 : index
    %c8 = arith.constant 8 : index
    %zero = arith.constant 0.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<8x8xf32>

    sde.su_iterate (%c1, %c1) to (%c7, %c7) step (%c4, %c4) classification(<elementwise>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %A : memref<8x8xf32> array_id(0)
      sde.cu_region <parallel> {
        memref.store %zero, %A[%i, %j] : memref<8x8xf32>
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [4, 4], muBlockCount = 4 : i64, role = "write"}]}
    return
  }

  func.func @exact_shifted_inout_writer_block_window_2d() {
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c7 = arith.constant 7 : index
    %one = arith.constant 1.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<8x8xf32>

    sde.su_iterate (%c1, %c1) to (%c7, %c7) step (%c4, %c4) classification(<elementwise>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %A : memref<8x8xf32> array_id(0)
      sde.cu_region <parallel> {
        %old = memref.load %A[%i, %j] : memref<8x8xf32>
        %new = arith.addf %old, %one : f32
        memref.store %new, %A[%i, %j] : memref<8x8xf32>
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [4, 4], muBlockCount = 4 : i64, role = "write"}]}
    return
  }
}
