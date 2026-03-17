// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline=pre-lowering | %FileCheck %s

// Conv3d-like repeated-epoch amortization:
// - Detect repetition loop around a single epoch + kernel timer tail
// - Hoist epoch out of the repetition loop
// - Push repetition into the outlined EDT body
//
// CHECK-LABEL: func.func private @__arts_edt_
// CHECK: %c4 = arith.constant 4 : index
// CHECK: scf.for %{{.*}} = %c0 to %c4 step %c1 {
//
// CHECK-LABEL: func.func @main
// CHECK: %[[EPOCH:.+]] = arts.create_epoch : i64
// CHECK-NOT: arts.create_epoch
// CHECK: arts.edt_create(
// CHECK: arts.wait_on_epoch %[[EPOCH]] : i64
// CHECK: scf.for %{{.*}} = %c0 to %c4 step %c1 {
// CHECK: func.call @carts_kernel_timer_accum() : () -> ()

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func private @carts_kernel_timer_accum()

  func.func @main() -> f32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %a = memref.alloc() : memref<8xf32>
    %b = memref.alloc() : memref<8xf32>

    scf.for %t = %c0 to %c4 step %c1 {
      omp.parallel {
        omp.wsloop for (%i) : index = (%c1) to (%c8) step (%c1) {
          %x = memref.load %a[%i] : memref<8xf32>
          memref.store %x, %b[%i] : memref<8xf32>
          omp.yield
        }
        omp.terminator
      }
      func.call @carts_kernel_timer_accum() : () -> ()
    }

    %r = memref.load %b[%c1] : memref<8xf32>
    memref.dealloc %a : memref<8xf32>
    memref.dealloc %b : memref<8xf32>
    return %r : f32
  }
}
