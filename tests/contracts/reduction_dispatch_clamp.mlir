// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: arts.epoch
// CHECK: arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xi32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {preserve_dep_mode = #arts.preserve_dep_mode}
// CHECK-NOT: scf.for %{{.*}} = %c0 to %c8 step %c1
// CHECK-NOT: arts.db_alloc[<inout>, <heap>, <write>, <coarse>]
// CHECK-NOT: arts.db_acquire[<out>]
// CHECK: arts.edt <task> <intranode> route(%c0_i32) (%ptr_1) : memref<?xi32>

#loc = loc(unknown)
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx16.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
  omp.reduction.declare @add_i32 : i32 init {
  ^bb0(%arg0: i32 loc(unknown)):
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    omp.yield(%c0_i32 : i32) loc(#loc)
  } combiner {
  ^bb0(%arg0: i32 loc(unknown), %arg1: i32 loc(unknown)):
    %0 = arith.addi %arg0, %arg1 : i32 loc(#loc)
    omp.yield(%0 : i32) loc(#loc)
  } loc(#loc)

  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c2 = arith.constant 2 : index loc(#loc)
    %sum_init = arith.constant 0 : i32 loc(#loc)
    %sum = memref.alloca() : memref<1xi32>
    %sum_cast = memref.cast %sum : memref<1xi32> to memref<?xi32>
    memref.store %sum_init, %sum[%c0] : memref<1xi32>

    omp.parallel {
      omp.wsloop reduction(@add_i32 -> %sum_cast : memref<?xi32>) for (%i) : index = (%c0) to (%c2) step (%c1) {
        %one_i32 = arith.constant 1 : i32 loc(#loc)
        omp.reduction %one_i32, %sum_cast : i32, memref<?xi32> loc(#loc)
        omp.yield loc(#loc)
      } loc(#loc)
      omp.terminator loc(#loc)
    } loc(#loc)

    %result = memref.load %sum[%c0] : memref<1xi32>
    return %result : i32
  } loc(#loc)
}
