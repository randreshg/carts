module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str9("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("A[0] = %d, B[0] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Task %d - 2: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task %d - 1: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d - 0: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c5_i32 = arith.constant 5 : i32
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %0 = llvm.mlir.undef : i32
    %1 = arith.cmpi slt, %arg0, %c2_i32 : i32
    %2 = arith.cmpi sge, %arg0, %c2_i32 : i32
    %3 = arith.select %1, %c1_i32, %0 : i32
    scf.if %1 {
      %5 = llvm.mlir.addressof @stderr : !llvm.ptr
      %6 = llvm.load %5 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %7 = "polygeist.memref2pointer"(%6) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %8 = llvm.mlir.addressof @str0 : !llvm.ptr
      %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<13 x i8>
      %10 = affine.load %arg1[0] : memref<?xmemref<?xi8>>
      %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
      %12 = llvm.call @fprintf(%7, %9, %11) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
    }
    %4 = arith.select %2, %c0_i32, %3 : i32
    scf.if %2 {
      %5 = affine.load %arg1[1] : memref<?xmemref<?xi8>>
      %6 = func.call @atoi(%5) : (memref<?xi8>) -> i32
      %7 = arith.index_cast %6 : i32 to index
      %alloca = memref.alloca(%7) : memref<?xi32>
      %alloca_0 = memref.alloca(%7) : memref<?xi32>
      %8 = llvm.mlir.zero : !llvm.ptr
      %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr) -> memref<?xi64>
      %10 = func.call @time(%9) : (memref<?xi64>) -> i64
      %11 = arith.trunci %10 : i64 to i32
      func.call @srand(%11) : (i32) -> ()
      %12 = llvm.mlir.addressof @str1 : !llvm.ptr
      %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
      %14 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %15 = llvm.mlir.addressof @str2 : !llvm.ptr
      %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<42 x i8>
      %17 = llvm.call @printf(%16, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
      omp.parallel   {
        omp.barrier
        %alloca_1 = memref.alloca() : memref<i32>
        %alloca_2 = memref.alloca() : memref<i32>
        %alloca_3 = memref.alloca() : memref<i32>
        %alloca_4 = memref.alloca() : memref<i32>
        %alloca_5 = memref.alloca() : memref<i32>
        %alloca_6 = memref.alloca() : memref<i32>
        omp.master {
          %30 = arith.index_cast %6 : i32 to index
          scf.for %arg2 = %c0 to %30 step %c1 {
            %31 = arith.index_cast %arg2 : index to i32
            %32 = memref.load %alloca[%arg2] : memref<?xi32>
            affine.store %32, %alloca_1[] : memref<i32>
            omp.task   depend(taskdependinout -> %alloca_1 : memref<i32>) {
              memref.store %31, %alloca[%arg2] : memref<?xi32>
              %40 = llvm.mlir.addressof @str3 : !llvm.ptr
              %41 = llvm.getelementptr %40[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
              %42 = llvm.call @printf(%41, %31, %31, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
              omp.terminator
            }
            %33 = memref.load %alloca[%arg2] : memref<?xi32>
            affine.store %33, %alloca_2[] : memref<i32>
            %34 = memref.load %alloca_0[%arg2] : memref<?xi32>
            affine.store %34, %alloca_3[] : memref<i32>
            omp.task   depend(taskdependin -> %alloca_2 : memref<i32>, taskdependinout -> %alloca_3 : memref<i32>) {
              %40 = arith.addi %33, %c5_i32 : i32
              memref.store %40, %alloca_0[%arg2] : memref<?xi32>
              %41 = llvm.mlir.addressof @str4 : !llvm.ptr
              %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
              %43 = llvm.call @printf(%42, %31, %31, %40) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
              omp.terminator
            }
            %35 = arith.addi %31, %c-1_i32 : i32
            %36 = arith.index_cast %35 : i32 to index
            %37 = memref.load %alloca_0[%36] : memref<?xi32>
            affine.store %37, %alloca_4[] : memref<i32>
            %38 = memref.load %alloca[%arg2] : memref<?xi32>
            affine.store %38, %alloca_5[] : memref<i32>
            %39 = memref.load %alloca_0[%arg2] : memref<?xi32>
            affine.store %39, %alloca_6[] : memref<i32>
            omp.task   depend(taskdependin -> %alloca_4 : memref<i32>, taskdependin -> %alloca_5 : memref<i32>, taskdependinout -> %alloca_6 : memref<i32>) {
              %40 = memref.load %alloca_0[%36] : memref<?xi32>
              %41 = arith.addi %38, %40 : i32
              %42 = arith.addi %41, %c5_i32 : i32
              memref.store %42, %alloca_0[%arg2] : memref<?xi32>
              %43 = llvm.mlir.addressof @str5 : !llvm.ptr
              %44 = llvm.getelementptr %43[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
              %45 = llvm.call @printf(%44, %31, %31, %42) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
              omp.terminator
            }
          }
          omp.terminator
        }
        omp.barrier
        omp.terminator
      }
      %18 = llvm.mlir.addressof @str6 : !llvm.ptr
      %19 = llvm.getelementptr %18[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %20 = llvm.call @printf(%19) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %21 = llvm.mlir.addressof @str7 : !llvm.ptr
      %22 = llvm.getelementptr %21[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
      %23 = affine.load %alloca[0] : memref<?xi32>
      %24 = affine.load %alloca_0[0] : memref<?xi32>
      %25 = llvm.call @printf(%22, %23, %24) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      %26 = arith.index_cast %6 : i32 to index
      scf.for %arg2 = %c0 to %26 step %c1 {
        %30 = arith.index_cast %arg2 : index to i32
        %31 = llvm.mlir.addressof @str8 : !llvm.ptr
        %32 = llvm.getelementptr %31[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %33 = memref.load %alloca[%arg2] : memref<?xi32>
        %34 = memref.load %alloca_0[%arg2] : memref<?xi32>
        %35 = llvm.call @printf(%32, %30, %33, %30, %34) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %27 = llvm.mlir.addressof @str9 : !llvm.ptr
      %28 = llvm.getelementptr %27[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %29 = llvm.call @printf(%28) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %4 : i32
  }
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
}
