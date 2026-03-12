// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at loop-reordering | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel>
// CHECK-COUNT-2: arith.muli %{{.*}}, %{{.*}} : index
// CHECK: arts.for(%c0) to(%{{.*}}) step(%c1)
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK-COUNT-2: scf.for %{{.*}} = %c0 to %c64 step %c1

#loc = loc(unknown)
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx16.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
  func.func @main() -> i32 {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c64 = arith.constant 64 : index

    arts.edt <parallel> <internode> route(%c0_i32) {
      arts.for(%c0) to(%c4) step(%c1) {{
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          scf.for %k = %c0 to %c64 step %c1 {
            scf.for %l = %c0 to %c64 step %c1 {
              scf.for %m = %c0 to %c64 step %c1 {
                func.call @sink(%i, %j, %k, %l, %m) : (index, index, index, index, index) -> ()
              }
            }
          }
        }
        arts.yield
      }}
      arts.yield
    }

    %ret = arith.constant 0 : i32
    return %ret : i32
  }

  func.func private @sink(index, index, index, index, index)
}
