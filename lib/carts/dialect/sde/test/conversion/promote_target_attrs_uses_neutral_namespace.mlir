// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline sde-input-normalization | %FileCheck %s

// CHECK: module attributes
// CHECK-SAME: "carts.target-cpu" = "znver4"
// CHECK-SAME: "carts.target-features" = "+avx2"
// CHECK-NOT: arts.target-cpu
// CHECK-NOT: arts.target-features

module attributes {
  dlti.dl_spec = #dlti.dl_spec<
    #dlti.dl_entry<i64, dense<64> : vector<2xi64>>,
    #dlti.dl_entry<i32, dense<32> : vector<2xi64>>,
    #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>,
    #dlti.dl_entry<"dlti.endianness", "little">,
    #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i64:64-n32:64-S128",
  llvm.target_triple = "x86_64-unknown-linux-gnu",
  "polygeist.target-cpu" = "znver4",
  "polygeist.target-features" = "+avx2"
} {
  func.func @main() {
    return
  }
}
