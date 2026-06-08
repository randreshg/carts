// RUN: not %carts-compile %s --O3 --arts-config %arts_config --start-from sde-planning --pipeline codir-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Same trip counts are not enough to form a repeated timestep. SDE must reject
// the mismatched layout instead of inventing a redistribution target.

// CHECK: sde.redist: redistribution edge is not a cross-owner reduction or halo
// CHECK-SAME: refusing to invent redistribution

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @mismatched_uniform_adjacent(%A: memref<8x8xf64>, %B: memref<8x8xf64>, %Mean: memref<8xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    scf.for %t = %c0 to %c8 step %c1 {
      sde.cu_region <parallel> {
        sde.su_iterate (%c0) to (%c8) step (%c1) {
        ^bb0(%b: index):
          scf.for %c = %c0 to %c8 step %c1 {
            %v = memref.load %A[%b, %c] : memref<8x8xf64>
            memref.store %v, %B[%b, %c] : memref<8x8xf64>
          }
          sde.yield
        }
        sde.yield
      }
      sde.cu_region <parallel> {
        sde.su_iterate (%c0) to (%c8) step (%c1) {
        ^bb0(%channel: index):
          %v = memref.load %B[%c0, %channel] : memref<8x8xf64>
          memref.store %v, %Mean[%channel] : memref<8xf64>
          sde.yield
        }
        sde.yield
      }
    }
    return
  }
}
