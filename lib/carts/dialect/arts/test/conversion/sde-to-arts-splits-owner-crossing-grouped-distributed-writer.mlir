// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.su_iterate --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_plan --implicit-check-not="groups distributed writer blocks across owner routes"

// A grouped writer range that crosses owner-map route boundaries is split into
// the largest proven owner-local physical DB-block ranges before launch.

// CHECK-LABEL: func.func @owner_crossing_grouped_distributed_writer_split
// CHECK: %[[DB_BLOCKS:.*]] = arith.constant 4 : index
// CHECK: arts.db_alloc
// CHECK-SAME: sizes[%[[DB_BLOCKS]]]
// CHECK: %[[STEP:.*]] = arith.constant 8 : index
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %[[STEP]]
// CHECK: %[[TWO_BLOCKS:.*]] = arith.constant 2 : index
// CHECK: %[[GROUP:.*]] = arith.minui {{.*}}, %[[TWO_BLOCKS]] : index
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: sizes[%[[GROUP]]]
// CHECK: arts.edt <task> <internode> route
// CHECK-SAME: ownerLocalWriterSplit
// CHECK-SAME: planLogicalWorkerSlice = [8]
// CHECK-SAME: planOwnerDims = [0]
// CHECK-SAME: planPhysicalBlockShape = [4]

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @owner_crossing_grouped_distributed_writer_split() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c4] {planOwnerDims = [0], planPhysicalBlockShape = [1, 4]} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %A = memref.cast %block : memref<?x?xf32> to memref<4x4xf32>

    sde.su_iterate (%c0) to (%c16) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.cu_region <parallel> {
        "arts.db_access_plan"(%A) <{blockHi = [4], blockLo = [0], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [4]}> : (memref<4x4xf32>) -> ()
        %owner = arith.divui %i, %c4 : index
        %local = arith.remui %i, %c4 : index
        memref.store %value, %A[%owner, %local] : memref<4x4xf32>
      }
    } {physicalOwnerDims = [0], physicalBlockShape = [4], logicalWorkerSlice = [12]}
    return
  }
}
