// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir \
// RUN:   --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=BASE
// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir \
// RUN:   --min-distributed-tile-bytes=4194304 \
// RUN:   --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=COARSE

// Direct-row owner-strip plans use the same SDE tile-byte floor as owner-tile
// plans. The floor may coarsen only excess inter-locality task waves; it keeps
// at least one task per logical worker so blocked DB ownership still exposes
// CDAG concurrency.

// BASE-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// BASE: func.func @direct_row_matmul
// BASE: iterationTopology = #sde.iteration_topology<owner_strip>
// BASE-SAME: logicalWorkerSlice = [32, 1024]
// BASE-SAME: physicalBlockShape = [32, 1024]
// BASE-SAME: physicalOwnerDims = [0]

// COARSE-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// COARSE: func.func @direct_row_matmul
// COARSE: iterationTopology = #sde.iteration_topology<owner_strip>
// COARSE-SAME: logicalWorkerSlice = [64, 1024]
// COARSE-SAME: physicalBlockShape = [64, 1024]
// COARSE-SAME: physicalOwnerDims = [0]

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @direct_row_matmul(%A: memref<1024x1024xf64>, %B: memref<1024x1024xf64>, %C: memref<1024x1024xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c1024 = arith.constant 1024 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c1024) step (%c1) {
          scf.for %j = %c0 to %c1024 step %c1 {
            %zero = arith.constant 0.000000e+00 : f64
            memref.store %zero, %C[%i, %j] : memref<1024x1024xf64>
            scf.for %k = %c0 to %c1024 step %c1 {
              %a = memref.load %A[%i, %k] : memref<1024x1024xf64>
              %b = memref.load %B[%k, %j] : memref<1024x1024xf64>
              %old = memref.load %C[%i, %j] : memref<1024x1024xf64>
              %prod = arith.mulf %a, %b : f64
              %next = arith.addf %old, %prod : f64
              memref.store %next, %C[%i, %j] : memref<1024x1024xf64>
            }
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
