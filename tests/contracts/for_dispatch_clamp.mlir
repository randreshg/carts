// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: arts.epoch
// CHECK: arts.edt <task>
// CHECK-NOT: scf.for %{{.*}} = %c0 to %c8 step %c1

#loc = loc(unknown)
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx16.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c2 = arith.constant 2 : index loc(#loc)
    %c2_i32 = arith.constant 2 : i32 loc(#loc)
    %a = memref.alloc() : memref<2xi32>
    %b = memref.alloc() : memref<2xi32>
    %c = memref.alloc() : memref<2xi32>

    affine.for %i = 0 to 2 {
      %iv = arith.index_cast %i : index to i32
      memref.store %iv, %a[%i] : memref<2xi32>
      %twice = arith.muli %iv, %c2_i32 : i32
      memref.store %twice, %b[%i] : memref<2xi32>
    }

    omp.parallel {
      omp.wsloop for (%i) : index = (%c0) to (%c2) step (%c1) {
        %lhs = memref.load %a[%i] : memref<2xi32>
        %rhs = memref.load %b[%i] : memref<2xi32>
        %sum = arith.addi %lhs, %rhs : i32
        memref.store %sum, %c[%i] : memref<2xi32>
        omp.yield loc(#loc)
      } loc(#loc)
      omp.terminator loc(#loc)
    } loc(#loc)

    call @consume(%c) : (memref<2xi32>) -> ()
    %first = memref.load %c[%c0] : memref<2xi32>
    memref.dealloc %a : memref<2xi32>
    memref.dealloc %b : memref<2xi32>
    memref.dealloc %c : memref<2xi32>
    return %first : i32
  } loc(#loc)
  func.func private @consume(memref<2xi32>) attributes {llvm.linkage = #llvm.linkage<external>}
}
