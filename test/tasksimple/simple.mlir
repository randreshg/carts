module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str4("EDT 3: The final number is %d - %d.\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("EDT 2: The number is %d/%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("EDT 1: The number is %d/%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("EDT 0: The initial number is %d/%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Main EDT: Starting...\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %c10_i32 = arith.constant 10 : i32
    %c1_i32 = arith.constant 1 : i32
    %c100_i32 = arith.constant 100 : i32
    %0 = llvm.mlir.undef : i32
    %alloca = memref.alloca() : memref<i32>
    affine.store %0, %alloca[] : memref<i32>
    %1 = llvm.mlir.zero : !llvm.ptr
    %2 = "polygeist.pointer2memref"(%1) : (!llvm.ptr) -> memref<?xi64>
    %3 = call @time(%2) : (memref<?xi64>) -> i64
    %4 = arith.trunci %3 : i64 to i32
    call @srand(%4) : (i32) -> ()
    %5 = llvm.mlir.addressof @str0 : !llvm.ptr
    %6 = llvm.getelementptr %5[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
    %7 = llvm.call @printf(%6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %8 = call @rand() : () -> i32
    %9 = arith.remsi %8, %c100_i32 : i32
    %10 = arith.addi %9, %c1_i32 : i32
    affine.store %10, %alloca[] : memref<i32>
    %11 = call @rand() : () -> i32
    %12 = arith.remsi %11, %c10_i32 : i32
    %13 = arith.addi %12, %c1_i32 : i32
    %14 = llvm.mlir.addressof @str1 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<36 x i8>
    %16 = llvm.call @printf(%15, %10, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    omp.parallel   {
      omp.barrier
      omp.master {
        %alloca_0 = memref.alloca() : memref<i32>
        affine.store %0, %alloca_0[] : memref<i32>
        %22 = llvm.mlir.addressof @str2 : !llvm.ptr
        %23 = llvm.getelementptr %22[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
        %24 = affine.load %alloca[] : memref<i32>
        %25 = llvm.call @printf(%23, %24, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
        affine.store %13, %alloca_0[] : memref<i32>
        omp.task   {
          %26 = affine.load %alloca[] : memref<i32>
          %27 = arith.addi %26, %c1_i32 : i32
          affine.store %27, %alloca[] : memref<i32>
          %28 = affine.load %alloca_0[] : memref<i32>
          %29 = arith.addi %28, %c1_i32 : i32
          affine.store %29, %alloca_0[] : memref<i32>
          %30 = llvm.mlir.addressof @str3 : !llvm.ptr
          %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
          %32 = llvm.call @printf(%31, %27, %29) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
          omp.terminator
        }
        omp.terminator
      }
      omp.barrier
      omp.terminator
    }
    %17 = affine.load %alloca[] : memref<i32>
    %18 = arith.addi %17, %c1_i32 : i32
    affine.store %18, %alloca[] : memref<i32>
    %19 = llvm.mlir.addressof @str4 : !llvm.ptr
    %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %21 = llvm.call @printf(%20, %18, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    return %c0_i32 : i32
  }
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
