// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Verify elementwise classification is stamped and the carrier-authoritative
// linalg.generic body is present after StructuredSummaries.  After
// ConvertSdeToArts the carrier is lowered to scf.for + memref ops with the
// correct dep/distribution attributes on arts.for.

// CHECK-LABEL: // -----// IR Dump After StructuredSummaries (structured-summaries) //----- //
// CHECK: arts_sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: linalg.generic
// CHECK: arith.mulf
// CHECK: linalg.yield

// CHECK-LABEL: // -----// IR Dump After ConvertSdeToArts (convert-sde-to-arts) //----- //
// CHECK: arts.for(%c0) to(%c128) step(%c16)
// CHECK: scf.for
// CHECK: memref.load
// CHECK: arith.mulf
// CHECK: memref.store
// CHECK: depPattern = #arts.dep_pattern<uniform>

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<128xf64>, %B: memref<128xf64>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop {
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
