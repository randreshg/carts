// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=OPT

// Verify that Tiling tiles a matmul with larger dimensions
// (64x64). The pass creates an outer tile on the leading parallel dimension
// while loop interchange (LoopInterchange) has already reordered j-k to k-j.
// After tiling, the su_iterate step changes from 1 to the tile size, and an
// scf.for tile loop wraps the original body.

// OPT-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// OPT: arts_sde.su_iterate
// OPT-SAME: classification(<matmul>)
// OPT: ^bb0(%[[BASE:.+]]: index):
// OPT: scf.for %[[I:.+]] = %[[BASE]]
// OPT: memref.load %arg0[%[[I]], %{{.+}}] : memref<64x64xf32>
// OPT: memref.load %arg1[%{{.+}}, %{{.+}}] : memref<64x64xf32>
// OPT: arith.mulf
// OPT: arith.addf
// OPT: memref.store {{.+}}, %arg2[%[[I]], %{{.+}}] : memref<64x64xf32>
// OPT-NOT: bufferization.to_tensor
// OPT-NOT: linalg.generic

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<64x64xf32>, %B: memref<64x64xf32>, %C: memref<64x64xf32>) {
    %c0 = arith.constant 0 : index
    %c64 = arith.constant 64 : index
    %c1 = arith.constant 1 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c64) step (%c1) {
          scf.for %j = %c0 to %c64 step %c1 {
            scf.for %k = %c0 to %c64 step %c1 {
              %a = memref.load %A[%i, %k] : memref<64x64xf32>
              %b = memref.load %B[%k, %j] : memref<64x64xf32>
              %c = memref.load %C[%i, %j] : memref<64x64xf32>
              %prod = arith.mulf %a, %b : f32
              %sum = arith.addf %c, %prod : f32
              memref.store %sum, %C[%i, %j] : memref<64x64xf32>
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
