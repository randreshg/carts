// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --pipeline=sde-planning \
// RUN:   | %FileCheck %s --implicit-check-not=arts.db_alloc --implicit-check-not=arts.db_acquire

// SDE must make a one-block writer footprint true before ARTS sees the IR.
// A nonzero logical tile lower bound with step 4 crosses physical block size 5;
// OwnerDimSelect retile dispatches on physical block starts and clips work.

// CHECK-LABEL: func.func @block_clipped_transposed_writer_footprint
// CHECK-DAG: %[[C2:.*]] = arith.constant 2 : index
// CHECK-DAG: %[[C18:.*]] = arith.constant 18 : index
// CHECK-DAG: %[[SRC:.*]] = sde.mu_alloc : memref<20x20x20xf32>
// CHECK-DAG: %[[DST:.*]] = sde.mu_alloc : memref<4x4x4x5x5x5xf32>
// CHECK: sde.su_distribute <owner_compute>
// CHECK: sde.su_iterate (%c0{{[^,]*}}, %c0{{[^,]*}}, %c0{{[^)]*}}) to (%[[C18]], %[[C18]], %[[C18]]) step (%{{.*}}, %{{.*}}, %{{.*}})
// CHECK: sde.array_layout_root write %[[DST]] : memref<4x4x4x5x5x5xf32> array_id(0)
// CHECK: sde.array_layout_root read %[[SRC]] : memref<20x20x20xf32> array_id(1)
// CHECK: arith.maxui {{.*}}, %[[C2]] : index
// CHECK: arith.minui {{.*}}, %[[C18]] : index
// CHECK: scf.for %[[K:.*]] =
// CHECK: arith.maxui {{.*}}, %[[C2]] : index
// CHECK: arith.minui {{.*}}, %[[C18]] : index
// CHECK: scf.for %[[J:.*]] =
// CHECK: arith.maxui {{.*}}, %[[C2]] : index
// CHECK: arith.minui {{.*}}, %[[C18]] : index
// CHECK: scf.for %[[I:.*]] =
// CHECK: arith.divui %[[I]], %{{.*}} : index
// CHECK: arith.divui %[[J]], %{{.*}} : index
// CHECK: arith.divui %[[K]], %{{.*}} : index
// CHECK: memref.store %{{.*}}, %[[DST]][
// CHECK-SAME: ] : memref<4x4x4x5x5x5xf32>
// CHECK: } {accessMaxOffsets = [1, 1, 1], accessMinOffsets = [-1, -1, -1]
// CHECK-SAME: blockShape = [5, 5, 5]
// CHECK-SAME: muBlockCount = 64
// CHECK-SAME: ownerDims = [0, 1, 2]
// CHECK-SAME: role = "write"
// CHECK-SAME: writeFootprint = [1, 1, 1]

module {
  func.func @block_clipped_transposed_writer_footprint() {
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c18 = arith.constant 18 : index
    %src = memref.alloc() : memref<20x20x20xf32>
    %dst = sde.mu_alloc : memref<20x20x20xf32>

    sde.su_iterate (%c2, %c2, %c2) to (%c18, %c18, %c18) step (%c4, %c4, %c4)
        classification(<stencil>) {
    ^bb0(%k: index, %j: index, %i: index):
      sde.array_layout_root write %dst : memref<20x20x20xf32> array_id(0)
      sde.array_layout_root read %src : memref<20x20x20xf32> array_id(1)
      sde.cu_region <parallel> {
        %kend0 = arith.addi %k, %c4 : index
        %kend = arith.minui %kend0, %c18 : index
        scf.for %kk = %k to %kend step %c1 {
          %jend0 = arith.addi %j, %c4 : index
          %jend = arith.minui %jend0, %c18 : index
          scf.for %jj = %j to %jend step %c1 {
            %iend0 = arith.addi %i, %c4 : index
            %iend = arith.minui %iend0, %c18 : index
            scf.for %ii = %i to %iend step %c1 {
              %im1 = arith.subi %ii, %c1 : index
              %ip1 = arith.addi %ii, %c1 : index
              %jm1 = arith.subi %jj, %c1 : index
              %jp1 = arith.addi %jj, %c1 : index
              %km1 = arith.subi %kk, %c1 : index
              %kp1 = arith.addi %kk, %c1 : index
              %v0 = memref.load %src[%km1, %jj, %ii] : memref<20x20x20xf32>
              %v1 = memref.load %src[%kp1, %jj, %ii] : memref<20x20x20xf32>
              %v2 = memref.load %src[%kk, %jm1, %ii] : memref<20x20x20xf32>
              %v3 = memref.load %src[%kk, %jp1, %ii] : memref<20x20x20xf32>
              %v4 = memref.load %src[%kk, %jj, %im1] : memref<20x20x20xf32>
              %v5 = memref.load %src[%kk, %jj, %ip1] : memref<20x20x20xf32>
              %a0 = arith.addf %v0, %v1 : f32
              %a1 = arith.addf %v2, %v3 : f32
              %a2 = arith.addf %v4, %v5 : f32
              %a3 = arith.addf %a0, %a1 : f32
              %out = arith.addf %a3, %a2 : f32
              memref.store %out, %dst[%ii, %jj, %kk] : memref<20x20x20xf32>
            }
          }
        }
        sde.yield
      }
      sde.yield
    } {accessMinOffsets = [-1, -1, -1], accessMaxOffsets = [1, 1, 1],
       arrayLayout = [
         {arrayId = 0 : i64, blockShape = [5, 5, 5],
          budgetBlockShape = [10, 10, 10], kind = "block_parallel",
          muBlockCount = 64 : i64, ownerDims = [0, 1, 2], role = "write"},
         {arrayId = 1 : i64, blockShape = [10, 10, 10],
          kind = "block_parallel", muBlockCount = 8 : i64,
          ownerDims = [0, 1, 2], role = "read"}],
       ownerDims = [0, 1, 2], spatialDims = [0, 1, 2],
       writeFootprint = [1, 1, 1]}
    return
  }
}
