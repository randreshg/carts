// RUN: not %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --start-from openmp-to-arts --pipeline openmp-to-arts 2>&1 | %FileCheck %s

// Verify that VerifySdeLowered rejects IR containing residual SDE ops.
// A surviving arts_sde.cu_atomic with a non-add reduction kind cannot be
// lowered by ConvertSdeToArts (only add is supported), so it survives
// and VerifySdeLowered should catch it.

// CHECK: SDE operation 'arts_sde.cu_atomic' survived past SDE-to-ARTS conversion

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main() {
    %addr = memref.alloca() : memref<f64>
    %val = arith.constant 1.0 : f64
    arts_sde.cu_atomic <mul>(%addr, %val : memref<f64>, f64)
    return
  }
}
