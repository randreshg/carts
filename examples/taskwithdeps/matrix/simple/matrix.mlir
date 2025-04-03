module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str10("\0AVerification: FAILED\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str9("\0AVerification: PASSED\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("\0AMatrix B:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("%6.2f \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Matrix A:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task 2: Computing B[%d][%d] = %.2f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task 1: Computing B[0][%d] = %.2f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Task 0: Initializing A[%d][%d] = %.2f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Random number: %d\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s <N>\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c11_i32 = arith.constant 11 : i32
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %0 = arith.cmpi slt, %arg0, %c2_i32 : i32
    scf.if %0 {
      %25 = llvm.mlir.addressof @stderr : !llvm.ptr
      %26 = llvm.load %25 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %27 = "polygeist.memref2pointer"(%26) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %28 = llvm.mlir.addressof @str0 : !llvm.ptr
      %29 = llvm.getelementptr %28[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %30 = affine.load %arg1[0] : memref<?xmemref<?xi8>>
      %31 = "polygeist.memref2pointer"(%30) : (memref<?xi8>) -> !llvm.ptr
      %32 = llvm.call @fprintf(%27, %29, %31) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
      func.call @exit(%c1_i32) : (i32) -> ()
    }
    %1 = affine.load %arg1[1] : memref<?xmemref<?xi8>>
    %2 = call @atoi(%1) : (memref<?xi8>) -> i32
    %3 = arith.index_cast %2 : i32 to index
    %alloca = memref.alloca(%3, %3) : memref<?x?xf64>
    %alloca_0 = memref.alloca(%3, %3) : memref<?x?xf64>
    %4 = llvm.mlir.zero : !llvm.ptr
    %5 = "polygeist.pointer2memref"(%4) : (!llvm.ptr) -> memref<?xi64>
    %6 = call @time(%5) : (memref<?xi64>) -> i64
    %7 = arith.trunci %6 : i64 to i32
    call @srand(%7) : (i32) -> ()
    %8 = call @rand() : () -> i32
    %9 = arith.remsi %8, %c11_i32 : i32
    %10 = llvm.mlir.addressof @str1 : !llvm.ptr
    %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<19 x i8>
    %12 = llvm.call @printf(%11, %9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    omp.parallel   {
      omp.barrier
      %alloca_1 = memref.alloca() : memref<f64>
      %alloca_2 = memref.alloca() : memref<f64>
      %alloca_3 = memref.alloca() : memref<f64>
      %alloca_4 = memref.alloca() : memref<f64>
      %alloca_5 = memref.alloca() : memref<f64>
      %alloca_6 = memref.alloca() : memref<f64>
      omp.master {
        %25 = arith.index_cast %2 : i32 to index
        scf.for %arg2 = %c0 to %25 step %c1 {
          %28 = arith.index_cast %arg2 : index to i32
          %29 = arith.index_cast %2 : i32 to index
          scf.for %arg3 = %c0 to %29 step %c1 {
            %30 = arith.index_cast %arg3 : index to i32
            %31 = memref.load %alloca[%arg2, %arg3] : memref<?x?xf64>
            affine.store %31, %alloca_6[] : memref<f64>
            omp.task   depend(taskdependout -> %alloca_6 : memref<f64>) {
              %32 = arith.sitofp %28 : i32 to f64
              %33 = arith.sitofp %9 : i32 to f64
              %34 = arith.addf %32, %33 : f64
              memref.store %34, %alloca[%arg2, %arg3] : memref<?x?xf64>
              %35 = llvm.mlir.addressof @str2 : !llvm.ptr
              %36 = llvm.getelementptr %35[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<39 x i8>
              %37 = llvm.call @printf(%36, %28, %30, %34) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
              omp.terminator
            }
          }
        }
        %26 = arith.index_cast %2 : i32 to index
        scf.for %arg2 = %c0 to %26 step %c1 {
          %28 = arith.index_cast %arg2 : index to i32
          %29 = memref.load %alloca[%c0, %arg2] : memref<?x?xf64>
          affine.store %29, %alloca_4[] : memref<f64>
          %30 = memref.load %alloca_0[%c0, %arg2] : memref<?x?xf64>
          affine.store %30, %alloca_5[] : memref<f64>
          omp.task   depend(taskdependin -> %alloca_4 : memref<f64>, taskdependout -> %alloca_5 : memref<f64>) {
            memref.store %29, %alloca_0[%c0, %arg2] : memref<?x?xf64>
            %31 = llvm.mlir.addressof @str3 : !llvm.ptr
            %32 = llvm.getelementptr %31[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
            %33 = llvm.call @printf(%32, %28, %29) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
            omp.terminator
          }
        }
        %27 = arith.index_cast %2 : i32 to index
        scf.for %arg2 = %c1 to %27 step %c1 {
          %28 = arith.index_cast %arg2 : index to i32
          %29 = arith.index_cast %2 : i32 to index
          scf.for %arg3 = %c0 to %29 step %c1 {
            %30 = arith.index_cast %arg3 : index to i32
            %31 = memref.load %alloca[%arg2, %arg3] : memref<?x?xf64>
            affine.store %31, %alloca_1[] : memref<f64>
            %32 = arith.addi %28, %c-1_i32 : i32
            %33 = arith.index_cast %32 : i32 to index
            %34 = memref.load %alloca[%33, %arg3] : memref<?x?xf64>
            affine.store %34, %alloca_2[] : memref<f64>
            %35 = memref.load %alloca_0[%arg2, %arg3] : memref<?x?xf64>
            affine.store %35, %alloca_3[] : memref<f64>
            omp.task   depend(taskdependin -> %alloca_1 : memref<f64>, taskdependin -> %alloca_2 : memref<f64>, taskdependout -> %alloca_3 : memref<f64>) {
              %36 = arith.addf %31, %34 : f64
              memref.store %36, %alloca_0[%arg2, %arg3] : memref<?x?xf64>
              %37 = llvm.mlir.addressof @str4 : !llvm.ptr
              %38 = llvm.getelementptr %37[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<36 x i8>
              %39 = llvm.call @printf(%38, %28, %30, %36) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
              omp.terminator
            }
          }
        }
        omp.terminator
      }
      omp.barrier
      omp.terminator
    }
    %13 = llvm.mlir.addressof @str5 : !llvm.ptr
    %14 = llvm.getelementptr %13[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %15 = llvm.call @printf(%14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %16 = arith.index_cast %2 : i32 to index
    scf.for %arg2 = %c0 to %16 step %c1 {
      %25 = arith.index_cast %2 : i32 to index
      scf.for %arg3 = %c0 to %25 step %c1 {
        %29 = llvm.mlir.addressof @str6 : !llvm.ptr
        %30 = llvm.getelementptr %29[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
        %31 = memref.load %alloca[%arg2, %arg3] : memref<?x?xf64>
        %32 = llvm.call @printf(%30, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %26 = llvm.mlir.addressof @str7 : !llvm.ptr
      %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
      %28 = llvm.call @printf(%27) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %17 = llvm.mlir.addressof @str8 : !llvm.ptr
    %18 = llvm.getelementptr %17[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    %19 = llvm.call @printf(%18) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %20 = arith.index_cast %2 : i32 to index
    scf.for %arg2 = %c0 to %20 step %c1 {
      %25 = arith.index_cast %2 : i32 to index
      scf.for %arg3 = %c0 to %25 step %c1 {
        %29 = llvm.mlir.addressof @str6 : !llvm.ptr
        %30 = llvm.getelementptr %29[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
        %31 = memref.load %alloca_0[%arg2, %arg3] : memref<?x?xf64>
        %32 = llvm.call @printf(%30, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %26 = llvm.mlir.addressof @str7 : !llvm.ptr
      %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
      %28 = llvm.call @printf(%27) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %21:3 = scf.while (%arg2 = %c0_i32, %arg3 = %c1_i32) : (i32, i32) -> (i32, i32, i32) {
      %25 = arith.cmpi slt, %arg2, %2 : i32
      %26:3 = scf.if %25 -> (i32, i32, i32) {
        %27 = arith.index_cast %arg2 : i32 to index
        %28 = memref.load %alloca_0[%c0, %27] : memref<?x?xf64>
        %29 = memref.load %alloca[%c0, %27] : memref<?x?xf64>
        %30 = arith.cmpf une, %28, %29 : f64
        %31 = arith.select %30, %c0_i32, %arg3 : i32
        %32 = arith.addi %arg2, %c1_i32 : i32
        %33 = llvm.mlir.undef : i32
        scf.yield %32, %31, %33 : i32, i32, i32
      } else {
        scf.yield %arg2, %arg3, %arg3 : i32, i32, i32
      }
      scf.condition(%25) %26#0, %26#1, %26#2 : i32, i32, i32
    } do {
    ^bb0(%arg2: i32, %arg3: i32, %arg4: i32):
      scf.yield %arg2, %arg3 : i32, i32
    }
    %22 = arith.index_cast %2 : i32 to index
    %23 = scf.for %arg2 = %c1 to %22 step %c1 iter_args(%arg3 = %21#2) -> (i32) {
      %25 = arith.index_cast %arg2 : index to i32
      %26 = arith.index_cast %2 : i32 to index
      %27 = scf.for %arg4 = %c0 to %26 step %c1 iter_args(%arg5 = %arg3) -> (i32) {
        %28 = memref.load %alloca_0[%arg2, %arg4] : memref<?x?xf64>
        %29 = memref.load %alloca[%arg2, %arg4] : memref<?x?xf64>
        %30 = arith.addi %25, %c-1_i32 : i32
        %31 = arith.index_cast %30 : i32 to index
        %32 = memref.load %alloca[%31, %arg4] : memref<?x?xf64>
        %33 = arith.addf %29, %32 : f64
        %34 = arith.cmpf une, %28, %33 : f64
        %35 = arith.select %34, %c0_i32, %arg5 : i32
        scf.yield %35 : i32
      }
      scf.yield %27 : i32
    }
    %24 = arith.cmpi ne, %23, %c0_i32 : i32
    scf.if %24 {
      %25 = llvm.mlir.addressof @str9 : !llvm.ptr
      %26 = llvm.getelementptr %25[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
      %27 = llvm.call @printf(%26) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    } else {
      %25 = llvm.mlir.addressof @str10 : !llvm.ptr
      %26 = llvm.getelementptr %25[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
      %27 = llvm.call @printf(%26) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %c0_i32 : i32
  }
  func.func private @exit(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
