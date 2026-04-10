// RUN: %carts-compile %s --O3 --arts-config %S/../../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[{{.*}}<coarse>, <stencil>]
// CHECK: arts.epoch attributes {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK: arts.db_acquire[<in>] ({{.*}}) partitioning(<block>
// CHECK: arts.db_acquire[<inout>] ({{.*}}) partitioning(<block>

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func @main() -> f64 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c15 = arith.constant 15 : index
    %input = memref.alloc() : memref<16xf64>
    %output = memref.alloc() : memref<16xf64>

    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c15) step (%c1) {
        %left = arith.subi %i, %c1 : index
        %right = arith.addi %i, %c1 : index
        %a = memref.load %input[%left] : memref<16xf64>
        %b = memref.load %input[%i] : memref<16xf64>
        %c = memref.load %input[%right] : memref<16xf64>
        %ab = arith.addf %a, %b : f64
        %sum = arith.addf %ab, %c : f64
        memref.store %sum, %output[%i] : memref<16xf64>
        omp.yield
      }
      }
      omp.terminator
    }

    %result = memref.load %output[%c1] : memref<16xf64>
    memref.dealloc %input : memref<16xf64>
    memref.dealloc %output : memref<16xf64>
    return %result : f64
  }
}
