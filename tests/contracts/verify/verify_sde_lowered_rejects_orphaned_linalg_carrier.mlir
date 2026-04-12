// RUN: not %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --start-from openmp-to-arts --pipeline openmp-to-arts 2>&1 | %FileCheck %s

// Verify that VerifySdeLowered rejects transient linalg carriers that survive
// inside an arts.for even when no residual arts_sde ops remain.

// CHECK: transient carrier 'linalg.copy' survived past SDE-to-ARTS conversion

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<16xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    arts.for(%c0) to(%c16) step(%c1) {
    ^bb0(%iv: index):
      %scratch = memref.alloca() : memref<16xf64>
      linalg.copy ins(%A : memref<16xf64>) outs(%scratch : memref<16xf64>)
      arts.yield
    }
    return
  }
}
