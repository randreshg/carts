// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: %[[B_GUID:.*]], %[[B_PTR:.*]] = arts.db_acquire[<in>] ({{.*}}) partitioning(<coarse>)
// CHECK: arts.epoch attributes {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>
// CHECK: %[[A_GUID:.*]], %[[A_PTR:.*]] = arts.db_acquire[<in>] ({{.*}}) partitioning(<block>
// CHECK: %[[C_GUID:.*]], %[[C_PTR:.*]] = arts.db_acquire[<inout>] ({{.*}}) partitioning(<block>
// CHECK: arts.edt <task> <intranode> route(%c0_i32) (%[[A_PTR]], %[[B_PTR]], %[[C_PTR]])

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func @main() -> f32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %cst = arith.constant 0.000000e+00 : f32
    %A = memref.alloc() : memref<16x16xf32>
    %B = memref.alloc() : memref<16x16xf32>
    %C = memref.alloc() : memref<16x16xf32>

    omp.parallel {
      omp.wsloop for (%i) : index = (%c0) to (%c16) step (%c1) {
        scf.for %j = %c0 to %c16 step %c1 {
          %sum = scf.for %k = %c0 to %c16 step %c1 iter_args(%acc = %cst) -> (f32) {
            %a = memref.load %A[%i, %k] : memref<16x16xf32>
            %b = memref.load %B[%k, %j] : memref<16x16xf32>
            %mul = arith.mulf %a, %b : f32
            %next = arith.addf %acc, %mul : f32
            scf.yield %next : f32
          }
          memref.store %sum, %C[%i, %j] : memref<16x16xf32>
        }
        omp.yield
      }
      omp.terminator
    }

    %result = memref.load %C[%c0, %c0] : memref<16x16xf32>
    memref.dealloc %C : memref<16x16xf32>
    memref.dealloc %B : memref<16x16xf32>
    memref.dealloc %A : memref<16x16xf32>
    return %result : f32
  }
}
