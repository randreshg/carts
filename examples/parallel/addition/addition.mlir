module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("Error at index %d: expected %d, got %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Parallel addition finished.\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Performing parallel array addition...\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %true = arith.constant true
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %c50_i32 = arith.constant 50 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = llvm.mlir.undef : i32
    %1 = llvm.mlir.addressof @str0 : !llvm.ptr
    %2 = llvm.getelementptr %1[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<39 x i8>
    %3 = llvm.call @printf(%2) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %4 = llvm.mlir.addressof @str1 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
    %6 = llvm.call @printf(%5) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %7:3 = scf.while (%arg0 = %c0_i32, %arg1 = %true, %arg2 = %0, %arg3 = %true) : (i32, i1, i32, i1) -> (i1, i32, i32) {
      %9 = arith.cmpi slt, %arg0, %c50_i32 : i32
      %10 = arith.andi %9, %arg3 : i1
      scf.condition(%10) %arg1, %arg2, %arg0 : i1, i32, i32
    } do {
    ^bb0(%arg0: i1, %arg1: i32, %arg2: i32):
      %9 = arith.muli %arg2, %c2_i32 : i32
      %10 = arith.addi %arg2, %9 : i32
      %11 = arith.muli %arg2, %c2_i32 : i32
      %12 = arith.addi %arg2, %11 : i32
      %13 = arith.cmpi ne, %10, %12 : i32
      %14 = arith.cmpi eq, %10, %12 : i32
      %15 = arith.andi %14, %arg0 : i1
      %16 = arith.select %13, %c1_i32, %arg1 : i32
      %17 = arith.cmpi eq, %10, %12 : i32
      scf.if %13 {
        %19 = llvm.mlir.addressof @str2 : !llvm.ptr
        %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<40 x i8>
        %21 = arith.addi %arg2, %11 : i32
        %22 = llvm.call @printf(%20, %arg2, %21, %10) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
      }
      %18 = scf.if %17 -> (i32) {
        %19 = arith.addi %arg2, %c1_i32 : i32
        scf.yield %19 : i32
      } else {
        scf.yield %arg2 : i32
      }
      scf.yield %18, %15, %16, %17 : i32, i1, i32, i1
    }
    %8 = arith.select %7#0, %c0_i32, %7#1 : i32
    return %8 : i32
  }
}
