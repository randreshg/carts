module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("Task 3: Final value of b=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Task 2: Reading a=%d and updating b\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Task 1: Initializing a\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c5_i32 = arith.constant 5 : i32
    %c10_i32 = arith.constant 10 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<i32>
    %alloca_0 = memref.alloca() : memref<i32>
    affine.store %c0_i32, %alloca_0[] : memref<i32>
    affine.store %c0_i32, %alloca[] : memref<i32>
    omp.parallel   {
      omp.barrier
      omp.master {
        %0 = affine.load %alloca_0[] : memref<i32>
        %alloca_1 = memref.alloca() : memref<i32>
        affine.store %0, %alloca_1[] : memref<i32>
        omp.task   depend(taskdependout -> %alloca_1 : memref<i32>) {
          %4 = llvm.mlir.addressof @str0 : !llvm.ptr
          %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
          %6 = llvm.call @printf(%5) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
          affine.store %c10_i32, %alloca_0[] : memref<i32>
          omp.terminator
        }
        %1 = affine.load %alloca_0[] : memref<i32>
        %alloca_2 = memref.alloca() : memref<i32>
        affine.store %1, %alloca_2[] : memref<i32>
        %2 = affine.load %alloca[] : memref<i32>
        %alloca_3 = memref.alloca() : memref<i32>
        affine.store %2, %alloca_3[] : memref<i32>
        omp.task   depend(taskdependin -> %alloca_2 : memref<i32>, taskdependout -> %alloca_3 : memref<i32>) {
          %4 = llvm.mlir.addressof @str1 : !llvm.ptr
          %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
          %6 = llvm.call @printf(%5, %1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
          %7 = arith.addi %1, %c5_i32 : i32
          affine.store %7, %alloca[] : memref<i32>
          omp.terminator
        }
        %3 = affine.load %alloca[] : memref<i32>
        %alloca_4 = memref.alloca() : memref<i32>
        affine.store %3, %alloca_4[] : memref<i32>
        omp.task   depend(taskdependin -> %alloca_4 : memref<i32>) {
          %4 = llvm.mlir.addressof @str2 : !llvm.ptr
          %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
          %6 = llvm.call @printf(%5, %3) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
          omp.terminator
        }
        omp.terminator
      }
      omp.barrier
      omp.terminator
    }
    return %c0_i32 : i32
  }
}
