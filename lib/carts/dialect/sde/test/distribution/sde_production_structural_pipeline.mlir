// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline=sde-planning | %FileCheck %s

// Staged SDE planning, not a textual pass pipeline, must produce the production
// structural carriers before SDE-to-ARTS conversion.

// CHECK-LABEL: func.func @production_window_1d
// CHECK: sde.mu_alloc : memref<2x512xf32>
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.cu_region <single>
// CHECK-NOT: sde.mu_access_window
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<2x512xf32>

func.func @production_window_1d() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024xf32>
  sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i] : memref<1024xf32>
      sde.yield
    } {serialReason = #sde.serial_reason<residual_source>}
    sde.yield
  }
  return
}

// CHECK-LABEL: func.func @owner_local_pipeline_budget_retiled
// CHECK: %[[A:.*]] = sde.mu_alloc : memref<2x32768x8192xf32>
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.su_iterate (%{{.*}}) to (%c65536) step (%{{.*}}) {{.*}}classification(<elementwise_pipeline>)
// CHECK-NOT: sde.mu_access_window
// CHECK: partialReductionOwnerDims = [0]

func.func @owner_local_pipeline_budget_retiled() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8192 = arith.constant 8192 : index
  %c65536 = arith.constant 65536 : index
  %zero = arith.constant 0.0 : f32
  %one = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<65536x8192xf32>
  sde.su_iterate (%c0) to (%c65536) step (%c1)
      classification(<elementwise_pipeline>) {
  ^bb0(%i: index):
    sde.array_layout_root write %A : memref<65536x8192xf32> array_id(1)
    sde.cu_region <parallel> {
      %sum = memref.alloca() : memref<f32>
      memref.store %zero, %sum[] : memref<f32>
      scf.for %j = %c0 to %c8192 step %c1 {
        %v = memref.load %A[%i, %j] : memref<65536x8192xf32>
        %old = memref.load %sum[] : memref<f32>
        %next = arith.addf %old, %v : f32
        memref.store %next, %sum[] : memref<f32>
      }
      %mean = memref.load %sum[] : memref<f32>
      scf.for %j = %c0 to %c8192 step %c1 {
        %v = memref.load %A[%i, %j] : memref<65536x8192xf32>
        %next = arith.addf %v, %mean : f32
        %out = arith.mulf %next, %one : f32
        memref.store %out, %A[%i, %j] : memref<65536x8192xf32>
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 1 : i64, blockShape = [32768, 8192],
       budgetBlockShape = [64, 8192],
       kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0],
       role = "write"}],
     inPlaceSharedState, partialReduction, partialReductionDims = [1],
     partialReductionOwnerDims = [0, 1]}
  return
}

// CHECK-LABEL: func.func @promoted_reduction_budget_retiled
// CHECK: %[[A:.*]] = sde.mu_alloc : memref<2x2x256x128x3136xf32>
// CHECK: %[[B:.*]] = sde.mu_alloc : memref<2x256x256xf32>
// CHECK: sde.su_iterate (%{{.*}}, %{{.*}}) to (%c512, %c256) step (%{{.*}}, %{{.*}}) {{.*}}classification(<reduction>)
// CHECK: sde.array_layout_root read %[[A]] : memref<2x2x256x128x3136xf32> array_id(3)
// CHECK-NOT: sde.mu_access_window
// CHECK: } {groupBlockCount = [2, 2]}
// CHECK: } {arrayLayout = [{arrayId = 3 : i64

func.func @promoted_reduction_budget_retiled() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c512 = arith.constant 512 : index
  %c256 = arith.constant 256 : index
  %c3136 = arith.constant 3136 : index
  %zero = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<512x256x3136xf32>
  %B = sde.mu_alloc : memref<512x256xf32>
  sde.su_iterate (%c0, %c0) to (%c512, %c256) step (%c1, %c1)
      classification(<reduction>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %B : memref<512x256xf32> array_id(2)
    sde.array_layout_root read %A : memref<512x256x3136xf32> array_id(3)
    sde.cu_region <parallel> {
      %sum = memref.alloca() : memref<f32>
      memref.store %zero, %sum[] : memref<f32>
      scf.for %k = %c0 to %c3136 step %c1 {
        %v = memref.load %A[%i, %j, %k] : memref<512x256x3136xf32>
        %old = memref.load %sum[] : memref<f32>
        %next = arith.addf %old, %v : f32
        memref.store %next, %sum[] : memref<f32>
      }
      %out = memref.load %sum[] : memref<f32>
      memref.store %out, %B[%i, %j] : memref<512x256xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 2 : i64, blockShape = [256, 128],
       budgetBlockShape = [512, 256],
       kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1],
       role = "write"},
      {arrayId = 3 : i64, blockShape = [256, 256, 3136],
       budgetBlockShape = [1, 256, 3136],
       kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0],
       role = "read"}],
     inPlaceSharedState}
  return
}

// CHECK-LABEL: func.func @replicated_writer_physical_shape_window
// CHECK: %[[OUT:.*]] = sde.mu_alloc : memref<64x8x256x784xf32>
// CHECK: sde.array_layout_root write %[[OUT]] : memref<64x8x256x784xf32> array_id({{[0-9]+}})
// CHECK-NOT: sde.mu_access_window
// CHECK: sde.cu_region <single>

func.func @replicated_writer_physical_shape_window() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %c28 = arith.constant 28 : index
  %c256 = arith.constant 256 : index
  %c512 = arith.constant 512 : index
  %cst = arith.constant 1.0 : f32
  %out = sde.mu_alloc : memref<512x256x784xf32>
  sde.su_iterate (%c0) to (%c512) step (%c8)
      classification(<elementwise_pipeline>) {
  ^bb0(%i: index):
    sde.array_layout_root write %out : memref<512x256x784xf32> array_id(4)
    sde.cu_region <parallel> {
      %hi = arith.addi %i, %c8 : index
      %limit = arith.minui %hi, %c512 : index
      scf.for %ii = %i to %limit step %c1 {
        scf.for %j = %c0 to %c256 step %c1 {
          scf.for %y = %c0 to %c28 step %c1 {
            scf.for %x = %c0 to %c28 step %c1 {
              %row = arith.muli %y, %c28 : index
              %flat = arith.addi %row, %x : index
              memref.store %cst, %out[%ii, %j, %flat] : memref<512x256x784xf32>
            }
          }
        }
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 4 : i64, blockShape = [512, 256, 784],
       budgetBlockShape = [512, 256, 784],
       kind = "replicated", muBlockCount = 1 : i64, ownerDims = [],
       role = "write"}],
     logicalWorkerSlice = [8, 256, 784], inPlaceSharedState}
  sde.cu_region <single> {
    %v = memref.load %out[%c0, %c0, %c0] : memref<512x256x784xf32>
    func.call @sink_f32(%v) : (f32) -> ()
  }
  return
}

// CHECK-LABEL: func.func @block_contraction_writer_physical_shape_window
// CHECK: %[[B:.*]] = sde.mu_alloc : memref<8x128x256xf32>
// CHECK: sde.array_layout_root write %[[B]] : memref<8x128x256xf32> array_id(5)
// CHECK-NOT: sde.mu_access_window
// CHECK: arrayId = 5 : i64, blockShape = [128, 256]
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0]

func.func @block_contraction_writer_physical_shape_window() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c128 = arith.constant 128 : index
  %c256 = arith.constant 256 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %B = sde.mu_alloc : memref<1024x256xf32>
  sde.su_iterate (%c0) to (%c1024) step (%c128)
      classification(<elementwise>) {
  ^bb0(%i: index):
    sde.array_layout_root write %B : memref<1024x256xf32> array_id(5)
    sde.cu_region <parallel> {
      %hi = arith.addi %i, %c128 : index
      %limit = arith.minui %hi, %c1024 : index
      scf.for %ii = %i to %limit step %c1 {
        scf.for %j = %c0 to %c256 step %c1 {
          memref.store %cst, %B[%ii, %j] : memref<1024x256xf32>
        }
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 5 : i64, blockShape = [1024, 256],
       budgetBlockShape = [1024, 256],
       kind = "block_contraction", muBlockCount = 1 : i64, ownerDims = [0],
       role = "write"}],
     logicalWorkerSlice = [128, 256]}
  return
}

func.func private @sink_f32(f32)
