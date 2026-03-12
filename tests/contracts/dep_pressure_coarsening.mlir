// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency | %FileCheck %s

// CHECK: arts.edt <parallel>
// CHECK: arts.for(%c0) to(%c32) step(%c1)
// CHECK: arts.partition_hint = {blockSize = 16 : i64, mode = 1 : i8}

#loc = loc(unknown)
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx16.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c32 = arith.constant 32 : index loc(#loc)
    %a0 = memref.alloc() : memref<32xi32>
    %a1 = memref.alloc() : memref<32xi32>
    %a2 = memref.alloc() : memref<32xi32>
    %a3 = memref.alloc() : memref<32xi32>
    %a4 = memref.alloc() : memref<32xi32>
    %a5 = memref.alloc() : memref<32xi32>
    %a6 = memref.alloc() : memref<32xi32>
    %out = memref.alloc() : memref<32xi32>

    omp.parallel {
      omp.wsloop for (%i) : index = (%c0) to (%c32) step (%c1) {
        %v0 = memref.load %a0[%i] : memref<32xi32>
        %v1 = memref.load %a1[%i] : memref<32xi32>
        %v2 = memref.load %a2[%i] : memref<32xi32>
        %v3 = memref.load %a3[%i] : memref<32xi32>
        %v4 = memref.load %a4[%i] : memref<32xi32>
        %v5 = memref.load %a5[%i] : memref<32xi32>
        %v6 = memref.load %a6[%i] : memref<32xi32>
        %s0 = arith.addi %v0, %v1 : i32
        %s1 = arith.addi %v2, %v3 : i32
        %s2 = arith.addi %v4, %v5 : i32
        %s3 = arith.addi %s0, %s1 : i32
        %s4 = arith.addi %s2, %v6 : i32
        %sum = arith.addi %s3, %s4 : i32
        memref.store %sum, %out[%i] : memref<32xi32>
        omp.yield loc(#loc)
      } loc(#loc)
      omp.terminator loc(#loc)
    } loc(#loc)

    %first = memref.load %out[%c0] : memref<32xi32>
    memref.dealloc %a0 : memref<32xi32>
    memref.dealloc %a1 : memref<32xi32>
    memref.dealloc %a2 : memref<32xi32>
    memref.dealloc %a3 : memref<32xi32>
    memref.dealloc %a4 : memref<32xi32>
    memref.dealloc %a5 : memref<32xi32>
    memref.dealloc %a6 : memref<32xi32>
    memref.dealloc %out : memref<32xi32>
    return %first : i32
  } loc(#loc)
}
