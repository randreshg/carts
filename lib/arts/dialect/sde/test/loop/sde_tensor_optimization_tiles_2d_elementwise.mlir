// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=OPT

// Verify that TensorOpt tiles a true 2D su_iterate produced from
// a multi-dimensional omp.loop_nest. The pass distributes workers across both
// dimensions (ceil(W^(1/N)) per dim) and creates two nested scf.for tile loops
// inside the rewritten su_iterate.

// OPT-LABEL: // -----// IR Dump After TensorOpt (tensor-opt) //----- //
// OPT: arts_sde.cu_region <parallel> {
// OPT: arts_sde.su_iterate
// OPT-SAME: to (%c128, %c256)
// OPT: scf.for %[[I:.+]] = %arg2
// OPT: scf.for %[[J:.+]] = %arg3
// OPT: memref.load %arg0[%[[I]], %[[J]]] : memref<128x256xf64>
// OPT: arith.mulf
// OPT: memref.store {{.+}}, %arg1[%[[I]], %[[J]]] : memref<128x256xf64>
// OPT-NOT: bufferization.to_tensor
// OPT-NOT: tensor.empty
// OPT-NOT: linalg.generic

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<128x256xf64>, %B: memref<128x256xf64>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c256 = arith.constant 256 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i, %j) : index = (%c0, %c0) to (%c128, %c256) step (%c1, %c1) {
          %v = memref.load %A[%i, %j] : memref<128x256xf64>
          %r = arith.mulf %v, %cst : f64
          memref.store %r, %B[%i, %j] : memref<128x256xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
