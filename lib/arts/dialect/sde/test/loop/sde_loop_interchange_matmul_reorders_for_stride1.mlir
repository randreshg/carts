// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=BEFORE
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=AFTER

// Verify that LoopInterchange preserves a matmul carrier. In
// carrier-authoritative mode the j-k→k-j reorder is implicit in the
// linalg.generic indexing maps. Both before and after interchange the carrier
// uses (d0,d2)→A, (d2,d1)→B, (d0,d1)→C maps with a reduction on d2.

// BEFORE-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //
// BEFORE: arts_sde.su_iterate
// BEFORE-SAME: classification(<matmul>)
// BEFORE: arts_sde.mu_memref_to_tensor %arg0
// BEFORE: arts_sde.mu_memref_to_tensor %arg1
// BEFORE: arts_sde.mu_memref_to_tensor %arg2
// BEFORE: linalg.generic
// BEFORE-SAME: iterator_types = ["parallel", "parallel", "reduction"]
// BEFORE: arith.mulf
// BEFORE: arith.addf
// BEFORE: linalg.yield

// AFTER-LABEL: // -----// IR Dump After LoopInterchange (loop-interchange) //----- //
// AFTER: arts_sde.su_iterate
// AFTER-SAME: classification(<matmul>)
// AFTER: arts_sde.mu_memref_to_tensor %arg0
// AFTER: arts_sde.mu_memref_to_tensor %arg1
// AFTER: arts_sde.mu_memref_to_tensor %arg2
// AFTER: linalg.generic
// AFTER-SAME: iterator_types = ["parallel", "parallel", "reduction"]
// AFTER: arith.mulf
// AFTER: arith.addf
// AFTER: linalg.yield

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<32x32xf32>, %B: memref<32x32xf32>, %C: memref<32x32xf32>) {
    %c0 = arith.constant 0 : index
    %c32 = arith.constant 32 : index
    %c1 = arith.constant 1 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c32) step (%c1) {
          scf.for %j = %c0 to %c32 step %c1 {
            scf.for %k = %c0 to %c32 step %c1 {
              %a = memref.load %A[%i, %k] : memref<32x32xf32>
              %b = memref.load %B[%k, %j] : memref<32x32xf32>
              %c = memref.load %C[%i, %j] : memref<32x32xf32>
              %prod = arith.mulf %a, %b : f32
              %sum = arith.addf %c, %prod : f32
              memref.store %sum, %C[%i, %j] : memref<32x32xf32>
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
