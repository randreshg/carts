// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline concurrency-opt | %FileCheck %s

// CHECK-NOT: arts.db_alloc[<inout>, <stack>, <write>, <coarse>, <uniform>]
// CHECK: arts.edt <task>
// CHECK-SAME: (%[[OUT_PTR:.*]]) : memref<?xmemref<?xf64>>
// CHECK: ^bb0(%[[OUT_ARG:.*]]: memref<?xmemref<?xf64>>):
// CHECK: %[[LOCAL:.*]] = memref.alloca()
// CHECK: %[[OUT_REF:.*]] = arts.db_ref %[[OUT_ARG]][%{{.*}}] : memref<?xmemref<?xf64>> -> memref<?xf64>
// CHECK: scf.for
// CHECK: memref.store %{{.*}}, %[[LOCAL]][] : memref<f64>
// CHECK: %[[VALUE:.*]] = memref.load %[[LOCAL]][] : memref<f64>
// CHECK: memref.store %[[VALUE]], %[[OUT_REF]][%{{.*}}] : memref<?xf64>

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func @main() -> f64 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %tmp = memref.alloca() : memref<1xf64>
    %out = memref.alloc() : memref<16xf64>
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c16) step (%c1) {
        %iv_i32 = arith.index_cast %i : index to i32
        %iv_f64 = arith.sitofp %iv_i32 : i32 to f64
        memref.store %iv_f64, %tmp[%c0] : memref<1xf64>
        %value = memref.load %tmp[%c0] : memref<1xf64>
        memref.store %value, %out[%i] : memref<16xf64>
        omp.yield
      }
      }
      omp.terminator
    }
    %result = memref.load %out[%c1] : memref<16xf64>
    memref.dealloc %out : memref<16xf64>
    return %result : f64
  }
}
