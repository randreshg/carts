// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// Contract: consecutive arts.for regions inside the same parallel EDT must
// stay in source order after ForLowering. This protects multi-phase kernels
// like 2mm/3mm, where later loop epochs must not be reinserted ahead of
// earlier phases.

// CHECK-LABEL: func.func @main
// CHECK: arts.epoch
// CHECK: memref.load %{{.*}}[%{{.*}}] : memref<?xf64>
// CHECK: memref.load %{{.*}}[%{{.*}}] : memref<?xf64>
// CHECK: arith.addf
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<?xf64>
// CHECK: arts.epoch
// CHECK: memref.load %{{.*}}[%{{.*}}] : memref<?xf64>
// CHECK: memref.load %{{.*}}[%{{.*}}] : memref<?xf64>
// CHECK: arith.mulf
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<?xf64>
// CHECK: call @consume(

#loc = loc(unknown)
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx16.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
  func.func @main() -> i32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %lhs = memref.alloc() : memref<8xf64>
    %rhs = memref.alloc() : memref<8xf64>
    %mid = memref.alloc() : memref<8xf64>
    %out = memref.alloc() : memref<8xf64>

    omp.parallel {
      omp.wsloop for (%i) : index = (%c0) to (%c8) step (%c1) {
        %a = memref.load %lhs[%i] : memref<8xf64>
        %b = memref.load %rhs[%i] : memref<8xf64>
        %sum = arith.addf %a, %b : f64
        memref.store %sum, %mid[%i] : memref<8xf64>
        omp.yield
      }
      omp.wsloop for (%i) : index = (%c0) to (%c8) step (%c1) {
        %m = memref.load %mid[%i] : memref<8xf64>
        %r = memref.load %rhs[%i] : memref<8xf64>
        %prod = arith.mulf %m, %r : f64
        memref.store %prod, %out[%i] : memref<8xf64>
        omp.yield
      }
      omp.terminator
    }

    call @consume(%out) : (memref<8xf64>) -> ()
    %v = memref.load %out[%c0] : memref<8xf64>
    %n = arith.fptosi %v : f64 to i32
    memref.dealloc %lhs : memref<8xf64>
    memref.dealloc %rhs : memref<8xf64>
    memref.dealloc %mid : memref<8xf64>
    memref.dealloc %out : memref<8xf64>
    return %n : i32
  }

  func.func private @consume(memref<8xf64>)
}
