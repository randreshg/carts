// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_64t.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=SDE
// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_64t.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=ARTS

// Verify the ownership boundary for loop schedule/chunk decisions:
// SdeChunkOptimization chooses or preserves the SDE-visible schedule/chunk,
// ConvertSdeToArts forwards that exact decision onto arts.for, and the
// conversion boundary does not leave behind SDE ops or transient carrier IR.

// SDE-LABEL: // -----// IR Dump After SdeChunkOptimization (sde-chunk-optimization) //----- //
// SDE: func.func @main
// SDE: %c4 = arith.constant 4 : index
// SDE: arts_sde.su_iterate(%c0) to(%c128) step(%c1) schedule(<dynamic>, %c4)

// ARTS-LABEL: // -----// IR Dump After ConvertSdeToArts (convert-sde-to-arts) //----- //
// ARTS: func.func @main
// ARTS: arts.edt <parallel> <intranode>
// ARTS: arts.for(%c0) to(%c128) step(%c1) schedule(<dynamic>, %c4)
// ARTS-NOT: arts_sde.
// ARTS-NOT: bufferization.to_tensor
// ARTS-NOT: tensor.empty
// ARTS-NOT: linalg.generic

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<128xf64>, %B: memref<128xf64>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop schedule(dynamic = %c4 : index, nonmonotonic) {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %v = memref.load %A[%i] : memref<128xf64>
          %r = arith.mulf %v, %cst : f64
          memref.store %r, %B[%i] : memref<128xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
