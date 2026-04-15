// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=LINALG
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR

// Verify the SDE carrier-authoritative flow for a matmul loop:
// RaiseToTensor is a no-op (scalar body still present, no carriers),
// RaiseToLinalg stamps the matmul classification and creates the
// carrier-authoritative linalg.generic with mu_memref_to_tensor boundary ops
// for the read-modify-write destination (scalar body erased).

// LINALG-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //
// LINALG: func.func @main
// LINALG: arts_sde.su_iterate (%c0) to (%c32) step (%c1) classification(<matmul>) {
// LINALG: %[[A:.+]] = arts_sde.mu_memref_to_tensor %arg0 : memref<32x32xf32> -> tensor<32x32xf32>
// LINALG: %[[B:.+]] = arts_sde.mu_memref_to_tensor %arg1 : memref<32x32xf32> -> tensor<32x32xf32>
// LINALG: arts_sde.mu_memref_to_tensor %arg2 : memref<32x32xf32> -> tensor<32x32xf32>
// LINALG: %[[C2:.+]] = arts_sde.mu_memref_to_tensor %arg2 : memref<32x32xf32> -> tensor<32x32xf32>
// LINALG: linalg.generic
// LINALG-SAME: ins(%[[A]], %[[B]],
// LINALG-SAME: outs(%[[C2]] : tensor<32x32xf32>)

// TENSOR-LABEL: // -----// IR Dump After RaiseToTensor (raise-to-tensor) //----- //
// TENSOR: func.func @main
// TENSOR: arts_sde.su_iterate (%c0) to (%c32) step (%c1) {
// TENSOR: scf.for
// TENSOR: scf.for
// TENSOR: memref.load %arg0
// TENSOR: memref.load %arg1
// TENSOR: memref.load %arg2
// TENSOR: memref.store %{{.+}}, %arg2

// TENSOR-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //

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
