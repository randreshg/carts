// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=epochs | %FileCheck %s

// Test that ownership proofs cannot be established for a kernel with
// non-affine/indirect access, so the persistent region gate does NOT fire.
// An EDT that uses a function call (opaque side-effect) prevents the
// ownership proof analysis from proving memory access patterns.

// CHECK-NOT: arts.persistent_region
// CHECK: arts.epoch

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @indirect_access(memref<64xf64>, index) -> ()

  func.func @test_persistent_proof_failure() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c20 = arith.constant 20 : index
    %c0_i32 = arith.constant 0 : i32
    %buf = memref.alloca() : memref<64xf64>
    scf.for %t = %c0 to %c20 step %c1 {
      %e = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @indirect_access(%buf, %t) : (memref<64xf64>, index) -> ()
          arts.yield
        }
        arts.yield
      } : i64
    }
    return
  }
}
