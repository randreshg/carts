// RUN: %carts-compile %s --O3 --arts-config %arts_config \
// RUN:   --start-from sde-planning --pipeline sde-planning -o - 2>&1 \
// RUN:   | %FileCheck %s \
// RUN:     --implicit-check-not=reduce_scatter_like \
// RUN:     --implicit-check-not=sde.mu_alloc \
// RUN:     --implicit-check-not="classification(<reduction>)" \
// RUN:     --implicit-check-not=block_contraction

// Scratch used only by one worksharing loop stays inside that su_iterate, so it
// remains worker-private and does not become a shared MU or redistribution edge.

// CHECK-LABEL: func.func @scratch_elementwise
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: sde.cu_region <parallel>
// CHECK: memref.alloca() : memref<8xf32>

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @scratch_elementwise(%out: memref<4194304xf32>, %in: memref<4194304xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c4194304 = arith.constant 4194304 : index
    %cst = arith.constant 0.000000e+00 : f32
    omp.parallel {
      %buf = memref.alloca() : memref<8xf32>
      omp.wsloop schedule(static) {
        omp.loop_nest (%i) : index = (%c0) to (%c4194304) step (%c1) {
          %x = memref.load %in[%i] : memref<4194304xf32>
          scf.for %j = %c0 to %c8 step %c1 {
            %v = arith.mulf %x, %x : f32
            memref.store %v, %buf[%j] : memref<8xf32>
          }
          %acc = scf.for %j = %c0 to %c8 step %c1 iter_args(%a = %cst) -> (f32) {
            %b = memref.load %buf[%j] : memref<8xf32>
            %n = arith.addf %a, %b : f32
            scf.yield %n : f32
          }
          memref.store %acc, %out[%i] : memref<4194304xf32>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
