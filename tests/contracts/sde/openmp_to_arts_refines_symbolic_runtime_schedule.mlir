// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Verify that SdeScheduleRefinement can conservatively refine a one-dimensional
// symbolic-trip runtime schedule when the same concrete schedule wins across
// the pass's SDE-level probe trip counts. The refinement must stay
// attribute-only: no chunk operand is introduced before or after
// SdeChunkOptimization.

// CHECK-LABEL: // -----// IR Dump After ConvertOpenMPToSde (convert-openmp-to-sde) //----- //
// CHECK: func.func @main(%[[N:.*]]: index, %[[A:.*]]: memref<?xf64>, %[[B:.*]]: memref<?xf64>)
// CHECK: arts_sde.cu_region <parallel> {
// CHECK: arts_sde.su_iterate(%c0) to(%[[N]]) step(%c1) schedule(<runtime>)

// CHECK-LABEL: // -----// IR Dump After SdeScheduleRefinement (sde-schedule-refinement) //----- //
// CHECK: func.func @main(%[[N:.*]]: index, %[[A:.*]]: memref<?xf64>, %[[B:.*]]: memref<?xf64>)
// CHECK: arts_sde.cu_region <parallel> scope(<local>) {
// CHECK: arts_sde.su_iterate(%c0) to(%[[N]]) step(%c1) schedule(<static>)
// CHECK-NOT: arts_sde.su_iterate{{.*}}schedule(<static>,
// CHECK-NOT: arts.for

// CHECK-LABEL: // -----// IR Dump After SdeChunkOptimization (sde-chunk-optimization) //----- //
// CHECK: func.func @main(%[[N:.*]]: index, %[[A:.*]]: memref<?xf64>, %[[B:.*]]: memref<?xf64>)
// CHECK: arts_sde.su_iterate(%c0) to(%[[N]]) step(%c1) schedule(<static>)
// CHECK-NOT: arts_sde.su_iterate{{.*}}schedule(<static>,

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%N: index, %A: memref<?xf64>, %B: memref<?xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop schedule(runtime) {
        omp.loop_nest (%i) : index = (%c0) to (%N) step (%c1) {
          %v = memref.load %A[%i] : memref<?xf64>
          %r = arith.mulf %v, %cst : f64
          memref.store %r, %B[%i] : memref<?xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
