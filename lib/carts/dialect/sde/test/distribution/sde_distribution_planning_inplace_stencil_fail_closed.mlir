// Multi-worker row-major unit-radius in-place stencils become an SDE wavefront.
// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-planning \
// RUN:   | %FileCheck %s --check-prefix=MULTI \
// RUN:     --implicit-check-not=wavefront_2d \
// RUN:     --implicit-check-not=arrayLayout \
// RUN:     --implicit-check-not=commVolumeBytes \
// RUN:     --implicit-check-not=inPlaceSharedState

// Single-worker lowering remains serial and undistributed.
// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_1t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-planning \
// RUN:   | %FileCheck %s --check-prefix=SERIAL

// MULTI-LABEL: func.func @seidel_in_place
// MULTI: scf.for
// MULTI: sde.su_distribute <owner_compute>
// MULTI: sde.su_iterate
// MULTI: sde.cu_region <parallel>
// MULTI: scf.if
// MULTI: accessMaxOffsets = [1, 1]
// MULTI-SAME: accessMinOffsets = [-1, -1]
// MULTI-SAME: iterationTopology = #sde.iteration_topology<owner_strip>
// MULTI-SAME: logicalWorkerSlice = [1, 4096]
// MULTI-SAME: ownerDims = [0, 1]
// MULTI-SAME: pattern = #sde.pattern<stencil_tiling_nd>
// MULTI-SAME: physicalBlockShape = [1, 4096]
// MULTI-SAME: physicalHaloShape = [1]
// MULTI-SAME: physicalOwnerDims = [0]
// MULTI-SAME: spatialDims = [0, 1]
// MULTI-SAME: writeFootprint = [1, 1]

// SERIAL-LABEL: func.func @seidel_in_place
// SERIAL: inPlaceSharedState
// SERIAL-NOT: wavefront_2d
// SERIAL-NOT: sde.su_distribute

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @seidel_in_place(%A: memref<4096x4096xf64>) {
    %c1 = arith.constant 1 : index
    %cub = arith.constant 4095 : index
    %nine = arith.constant 9.000000e+00 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%cub) step (%c1) {
          scf.for %j = %c1 to %cub step %c1 {
            %im1 = arith.subi %i, %c1 : index
            %ip1 = arith.addi %i, %c1 : index
            %jm1 = arith.subi %j, %c1 : index
            %jp1 = arith.addi %j, %c1 : index
            %a0 = memref.load %A[%im1, %jm1] : memref<4096x4096xf64>
            %a1 = memref.load %A[%im1, %j] : memref<4096x4096xf64>
            %a2 = memref.load %A[%im1, %jp1] : memref<4096x4096xf64>
            %a3 = memref.load %A[%i, %jm1] : memref<4096x4096xf64>
            %a4 = memref.load %A[%i, %j] : memref<4096x4096xf64>
            %a5 = memref.load %A[%i, %jp1] : memref<4096x4096xf64>
            %a6 = memref.load %A[%ip1, %jm1] : memref<4096x4096xf64>
            %a7 = memref.load %A[%ip1, %j] : memref<4096x4096xf64>
            %a8 = memref.load %A[%ip1, %jp1] : memref<4096x4096xf64>
            %s0 = arith.addf %a0, %a1 : f64
            %s1 = arith.addf %s0, %a2 : f64
            %s2 = arith.addf %s1, %a3 : f64
            %s3 = arith.addf %s2, %a4 : f64
            %s4 = arith.addf %s3, %a5 : f64
            %s5 = arith.addf %s4, %a6 : f64
            %s6 = arith.addf %s5, %a7 : f64
            %s7 = arith.addf %s6, %a8 : f64
            %res = arith.divf %s7, %nine : f64
            memref.store %res, %A[%i, %j] : memref<4096x4096xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
