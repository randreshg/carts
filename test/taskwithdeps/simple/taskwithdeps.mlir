module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx15.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
  llvm.mlir.global internal constant @str7("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("A: %d, B: %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Parallel region DONE\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task 3: Final value of b=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task 2: Reading a=%d and updating b\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Task 1: Initializing a with value %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Main thread: Creating tasks\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c5_i32 = arith.constant 5 : i32
    %c100_i32 = arith.constant 100 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<i32>
    %0 = llvm.mlir.undef : i32
    affine.store %0, %alloca[] : memref<i32>
    %alloca_0 = memref.alloca() : memref<i32>
    affine.store %0, %alloca_0[] : memref<i32>
    %1 = llvm.mlir.addressof @str0 : !llvm.ptr
    %2 = llvm.getelementptr %1[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
    %3 = llvm.call @printf(%2) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    affine.store %c0_i32, %alloca_0[] : memref<i32>
    affine.store %c0_i32, %alloca[] : memref<i32>
    %4 = llvm.mlir.zero : !llvm.ptr
    %5 = "polygeist.pointer2memref"(%4) : (!llvm.ptr) -> memref<?xi64>
    %6 = call @time(%5) : (memref<?xi64>) -> i64
    %7 = arith.trunci %6 : i64 to i32
    call @srand(%7) : (i32) -> ()
    omp.parallel   {
      omp.barrier
      omp.master {
        %19 = llvm.mlir.addressof @str1 : !llvm.ptr
        %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
        %21 = llvm.call @printf(%20) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
        omp.task   {
          %25 = func.call @rand() : () -> i32
          %26 = arith.remsi %25, %c100_i32 : i32
          affine.store %26, %alloca_0[] : memref<i32>
          %27 = llvm.mlir.addressof @str2 : !llvm.ptr
          %28 = llvm.getelementptr %27[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
          %29 = llvm.call @printf(%28, %26) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
          omp.terminator
        }
        %22 = affine.load %alloca_0[] : memref<i32>
        %alloca_1 = memref.alloca() : memref<i32>
        affine.store %22, %alloca_1[] : memref<i32>
        %23 = affine.load %alloca[] : memref<i32>
        %alloca_2 = memref.alloca() : memref<i32>
        affine.store %23, %alloca_2[] : memref<i32>
        omp.task   depend(taskdependin -> %alloca_1 : memref<i32>, taskdependinout -> %alloca_2 : memref<i32>) {
          %25 = llvm.mlir.addressof @str3 : !llvm.ptr
          %26 = llvm.getelementptr %25[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
          %27 = llvm.call @printf(%26, %22) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
          %28 = arith.addi %22, %c5_i32 : i32
          affine.store %28, %alloca[] : memref<i32>
          omp.terminator
        }
        %24 = affine.load %alloca[] : memref<i32>
        %alloca_3 = memref.alloca() : memref<i32>
        affine.store %24, %alloca_3[] : memref<i32>
        omp.task   depend(taskdependin -> %alloca_3 : memref<i32>) {
          %25 = llvm.mlir.addressof @str4 : !llvm.ptr
          %26 = llvm.getelementptr %25[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
          %27 = llvm.call @printf(%26, %24) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
          omp.terminator
        }
        omp.terminator
      }
      omp.barrier
      omp.terminator
    }
    %8 = llvm.mlir.addressof @str5 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
    %10 = llvm.call @printf(%9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %11 = llvm.mlir.addressof @str6 : !llvm.ptr
    %12 = llvm.getelementptr %11[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<14 x i8>
    %13 = affine.load %alloca_0[] : memref<i32>
    %14 = affine.load %alloca[] : memref<i32>
    %15 = llvm.call @printf(%12, %13, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    %16 = llvm.mlir.addressof @str7 : !llvm.ptr
    %17 = llvm.getelementptr %16[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
    %18 = llvm.call @printf(%17) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    return %c0_i32 : i32
  }
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
