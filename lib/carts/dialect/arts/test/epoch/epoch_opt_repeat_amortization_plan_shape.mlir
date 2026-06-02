// RUN: %carts-compile %s --pipeline epochs --start-from epochs --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

module {
  func.func @repeat_uniform_no_halo_uses_existing_plan() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {planOwnerDims = [0], planPhysicalBlockShape = [1], planLogicalWorkerSlice = [1], planIterationTopology = #arts.plan_iteration_topology<owner_tile>, planRepetitionStructure = #arts.plan_repetition_structure<full_timestep>} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    scf.for %i = %c0 to %c4 step %c1 {
      arts.epoch attributes {depPattern = #arts.dep_pattern<uniform>, planIterationTopology = #arts.plan_iteration_topology<owner_tile>, planRepetitionStructure = #arts.plan_repetition_structure<full_timestep>} {
        %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<uniform>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
        arts.edt <task> <intranode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> {
        ^bb0(%dep: memref<?xmemref<?xf64>>):
          %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
          %value = memref.load %payload[%c0] : memref<?xf64>
          memref.store %value, %payload[%c0] : memref<?xf64>
          arts.yield
        }
        arts.yield
      } : i64
    }
    return
  }

  func.func @repeat_uniform_halo_plan_keeps_epoch_loop() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {planOwnerDims = [0], planPhysicalBlockShape = [1], planLogicalWorkerSlice = [1], planHaloShape = [1], planIterationTopology = #arts.plan_iteration_topology<owner_tile>, planRepetitionStructure = #arts.plan_repetition_structure<full_timestep>} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    scf.for %i = %c0 to %c4 step %c1 {
      arts.epoch attributes {depPattern = #arts.dep_pattern<uniform>, planHaloShape = [1], planIterationTopology = #arts.plan_iteration_topology<owner_tile>, planRepetitionStructure = #arts.plan_repetition_structure<full_timestep>} {
        %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<uniform>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
        arts.edt <task> <intranode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> {
        ^bb0(%dep: memref<?xmemref<?xf64>>):
          %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
          %value = memref.load %payload[%c0] : memref<?xf64>
          memref.store %value, %payload[%c0] : memref<?xf64>
          arts.yield
        }
        arts.yield
      } : i64
    }
    return
  }
}

// CHECK-LABEL: func.func @repeat_uniform_no_halo_uses_existing_plan
// CHECK-NOT: scf.for
// CHECK: arts.epoch
// CHECK: arts.edt
// CHECK: scf.for

// CHECK-LABEL: func.func @repeat_uniform_halo_plan_keeps_epoch_loop
// CHECK: scf.for
// CHECK: arts.epoch
// CHECK-SAME: planHaloShape = [1]
// CHECK: arts.edt
