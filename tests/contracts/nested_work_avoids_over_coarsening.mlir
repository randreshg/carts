// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency | %FileCheck %s

// CHECK: arts.edt <parallel>
// CHECK: arts.for(%c0) to(%c32) step(%c1)
// CHECK-NOT: arts.partition_hint

#loc = loc(unknown)
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx16.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c32 = arith.constant 32 : index loc(#loc)
    %c64 = arith.constant 64 : index loc(#loc)
    %buf = memref.alloc() : memref<32x64x64xi32>

    omp.parallel {
      omp.wsloop for (%i) : index = (%c0) to (%c32) step (%c1) {
        affine.for %j = 0 to 64 {
          affine.for %k = 0 to 64 {
            %iv = arith.index_cast %k : index to i32
            memref.store %iv, %buf[%i, %j, %k] : memref<32x64x64xi32>
          }
        }
        omp.yield loc(#loc)
      } loc(#loc)
      omp.terminator loc(#loc)
    } loc(#loc)

    %v = memref.load %buf[%c0, %c0, %c0] : memref<32x64x64xi32>
    memref.dealloc %buf : memref<32x64x64xi32>
    return %v : i32
  } loc(#loc)
}
