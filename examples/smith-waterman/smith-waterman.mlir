module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str6("Results DO NOT match!\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Results match!\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Mismatch at (%d, %d): Parallel=%d, Sequential=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("seq2: %s\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("seq1: %s\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str1("TATGCGC\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("AGTACGCA\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c0_i32 = arith.constant 0 : i32
    %c-2_i32 = arith.constant -2 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c2_i32 = arith.constant 2 : i32
    %alloca = memref.alloca() : memref<8xi8>
    %alloca_0 = memref.alloca() : memref<9xi8>
    %0 = llvm.mlir.addressof @str0 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %2 = llvm.load %1 : !llvm.ptr -> i8
    affine.store %2, %alloca_0[0] : memref<9xi8>
    %3 = llvm.getelementptr %0[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %4 = llvm.load %3 : !llvm.ptr -> i8
    affine.store %4, %alloca_0[1] : memref<9xi8>
    %5 = llvm.getelementptr %0[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %6 = llvm.load %5 : !llvm.ptr -> i8
    affine.store %6, %alloca_0[2] : memref<9xi8>
    %7 = llvm.getelementptr %0[0, 3] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %8 = llvm.load %7 : !llvm.ptr -> i8
    affine.store %8, %alloca_0[3] : memref<9xi8>
    %9 = llvm.getelementptr %0[0, 4] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %10 = llvm.load %9 : !llvm.ptr -> i8
    affine.store %10, %alloca_0[4] : memref<9xi8>
    %11 = llvm.getelementptr %0[0, 5] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %12 = llvm.load %11 : !llvm.ptr -> i8
    affine.store %12, %alloca_0[5] : memref<9xi8>
    %13 = llvm.getelementptr %0[0, 6] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %14 = llvm.load %13 : !llvm.ptr -> i8
    affine.store %14, %alloca_0[6] : memref<9xi8>
    %15 = llvm.getelementptr %0[0, 7] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %16 = llvm.load %15 : !llvm.ptr -> i8
    affine.store %16, %alloca_0[7] : memref<9xi8>
    %17 = llvm.getelementptr %0[0, 8] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %18 = llvm.load %17 : !llvm.ptr -> i8
    affine.store %18, %alloca_0[8] : memref<9xi8>
    %19 = llvm.mlir.addressof @str1 : !llvm.ptr
    %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %21 = llvm.load %20 : !llvm.ptr -> i8
    affine.store %21, %alloca[0] : memref<8xi8>
    %22 = llvm.getelementptr %19[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %23 = llvm.load %22 : !llvm.ptr -> i8
    affine.store %23, %alloca[1] : memref<8xi8>
    %24 = llvm.getelementptr %19[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %25 = llvm.load %24 : !llvm.ptr -> i8
    affine.store %25, %alloca[2] : memref<8xi8>
    %26 = llvm.getelementptr %19[0, 3] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %27 = llvm.load %26 : !llvm.ptr -> i8
    affine.store %27, %alloca[3] : memref<8xi8>
    %28 = llvm.getelementptr %19[0, 4] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %29 = llvm.load %28 : !llvm.ptr -> i8
    affine.store %29, %alloca[4] : memref<8xi8>
    %30 = llvm.getelementptr %19[0, 5] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %31 = llvm.load %30 : !llvm.ptr -> i8
    affine.store %31, %alloca[5] : memref<8xi8>
    %32 = llvm.getelementptr %19[0, 6] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %33 = llvm.load %32 : !llvm.ptr -> i8
    affine.store %33, %alloca[6] : memref<8xi8>
    %34 = llvm.getelementptr %19[0, 7] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %35 = llvm.load %34 : !llvm.ptr -> i8
    affine.store %35, %alloca[7] : memref<8xi8>
    %36 = "polygeist.memref2pointer"(%alloca_0) : (memref<9xi8>) -> !llvm.ptr
    %37 = call @strlen(%36) : (!llvm.ptr) -> i64
    %38 = arith.trunci %37 : i64 to i32
    %39 = "polygeist.memref2pointer"(%alloca) : (memref<8xi8>) -> !llvm.ptr
    %40 = call @strlen(%39) : (!llvm.ptr) -> i64
    %41 = arith.trunci %40 : i64 to i32
    %42 = arith.addi %38, %c1_i32 : i32
    %43 = arith.index_cast %42 : i32 to index
    %44 = arith.addi %41, %c1_i32 : i32
    %45 = arith.index_cast %44 : i32 to index
    %alloca_1 = memref.alloca(%43, %45) : memref<?x?xi32>
    %46 = arith.addi %38, %c1_i32 : i32
    %47 = arith.index_cast %46 : i32 to index
    %48 = arith.addi %41, %c1_i32 : i32
    %49 = arith.index_cast %48 : i32 to index
    %alloca_2 = memref.alloca(%47, %49) : memref<?x?xi32>
    %50 = arith.addi %38, %c1_i32 : i32
    %51 = arith.index_cast %50 : i32 to index
    scf.for %arg0 = %c0 to %51 step %c1 {
      %58 = arith.addi %41, %c1_i32 : i32
      %59 = arith.index_cast %58 : i32 to index
      scf.for %arg1 = %c0 to %59 step %c1 {
        memref.store %c0_i32, %alloca_1[%arg0, %arg1] : memref<?x?xi32>
        memref.store %c0_i32, %alloca_2[%arg0, %arg1] : memref<?x?xi32>
      }
    }
    omp.parallel   {
      omp.barrier
      omp.master {
        %58 = llvm.mlir.addressof @str2 : !llvm.ptr
        %59 = llvm.getelementptr %58[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i8>
        %60 = "polygeist.memref2pointer"(%alloca_0) : (memref<9xi8>) -> !llvm.ptr
        %61 = llvm.call @printf(%59, %60) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
        %62 = llvm.mlir.addressof @str3 : !llvm.ptr
        %63 = llvm.getelementptr %62[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i8>
        %64 = "polygeist.memref2pointer"(%alloca) : (memref<8xi8>) -> !llvm.ptr
        %65 = llvm.call @printf(%63, %64) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
        %66 = arith.addi %38, %c1_i32 : i32
        %67 = arith.index_cast %66 : i32 to index
        scf.for %arg0 = %c1 to %67 step %c1 {
          %68 = arith.index_cast %arg0 : index to i32
          %69 = arith.addi %41, %c1_i32 : i32
          %70 = arith.index_cast %69 : i32 to index
          scf.for %arg1 = %c1 to %70 step %c1 {
            %71 = arith.index_cast %arg1 : index to i32
            omp.task   {
              %72 = arith.addi %68, %c-1_i32 : i32
              %73 = arith.index_cast %72 : i32 to index
              %74 = arith.addi %71, %c-1_i32 : i32
              %75 = arith.index_cast %74 : i32 to index
              %76 = memref.load %alloca_1[%73, %75] : memref<?x?xi32>
              %77 = memref.load %alloca_0[%73] : memref<9xi8>
              %78 = memref.load %alloca[%75] : memref<8xi8>
              %79 = arith.cmpi eq, %77, %78 : i8
              %80 = arith.select %79, %c2_i32, %c-1_i32 : i32
              %81 = arith.addi %76, %80 : i32
              %82 = arith.addi %68, %c-1_i32 : i32
              %83 = arith.index_cast %82 : i32 to index
              %84 = memref.load %alloca_1[%83, %arg1] : memref<?x?xi32>
              %85 = arith.addi %84, %c-2_i32 : i32
              %86 = arith.addi %71, %c-1_i32 : i32
              %87 = arith.index_cast %86 : i32 to index
              %88 = memref.load %alloca_1[%arg0, %87] : memref<?x?xi32>
              %89 = arith.addi %88, %c-2_i32 : i32
              %90 = arith.cmpi sgt, %81, %85 : i32
              %91 = scf.if %90 -> (i32) {
                %94 = arith.cmpi sgt, %81, %89 : i32
                %95 = arith.select %94, %81, %89 : i32
                scf.yield %95 : i32
              } else {
                %94 = arith.cmpi sgt, %85, %89 : i32
                %95 = arith.select %94, %85, %89 : i32
                scf.yield %95 : i32
              }
              memref.store %91, %alloca_1[%arg0, %arg1] : memref<?x?xi32>
              %92 = memref.load %alloca_1[%arg0, %arg1] : memref<?x?xi32>
              %93 = arith.cmpi slt, %92, %c0_i32 : i32
              scf.if %93 {
                memref.store %c0_i32, %alloca_1[%arg0, %arg1] : memref<?x?xi32>
              }
              omp.terminator
            }
          }
        }
        omp.terminator
      }
      omp.barrier
      omp.terminator
    }
    %52 = arith.addi %38, %c1_i32 : i32
    %53 = arith.index_cast %52 : i32 to index
    scf.for %arg0 = %c1 to %53 step %c1 {
      %58 = arith.index_cast %arg0 : index to i32
      %59 = arith.addi %41, %c1_i32 : i32
      %60 = arith.index_cast %59 : i32 to index
      scf.for %arg1 = %c1 to %60 step %c1 {
        %61 = arith.index_cast %arg1 : index to i32
        %62 = arith.addi %58, %c-1_i32 : i32
        %63 = arith.index_cast %62 : i32 to index
        %64 = arith.addi %61, %c-1_i32 : i32
        %65 = arith.index_cast %64 : i32 to index
        %66 = memref.load %alloca_2[%63, %65] : memref<?x?xi32>
        %67 = memref.load %alloca_0[%63] : memref<9xi8>
        %68 = memref.load %alloca[%65] : memref<8xi8>
        %69 = arith.cmpi eq, %67, %68 : i8
        %70 = arith.select %69, %c2_i32, %c-1_i32 : i32
        %71 = arith.addi %66, %70 : i32
        %72 = memref.load %alloca_2[%63, %arg1] : memref<?x?xi32>
        %73 = arith.addi %72, %c-2_i32 : i32
        %74 = memref.load %alloca_2[%arg0, %65] : memref<?x?xi32>
        %75 = arith.addi %74, %c-2_i32 : i32
        %76 = arith.cmpi sgt, %71, %73 : i32
        %77 = scf.if %76 -> (i32) {
          %79 = arith.cmpi sgt, %71, %75 : i32
          %80 = arith.select %79, %71, %75 : i32
          scf.yield %80 : i32
        } else {
          %79 = arith.cmpi sgt, %73, %75 : i32
          %80 = arith.select %79, %73, %75 : i32
          scf.yield %80 : i32
        }
        memref.store %77, %alloca_2[%arg0, %arg1] : memref<?x?xi32>
        %78 = arith.cmpi slt, %77, %c0_i32 : i32
        scf.if %78 {
          memref.store %c0_i32, %alloca_2[%arg0, %arg1] : memref<?x?xi32>
        }
      }
    }
    %54 = arith.addi %38, %c1_i32 : i32
    %55 = arith.index_cast %54 : i32 to index
    %56 = scf.for %arg0 = %c0 to %55 step %c1 iter_args(%arg1 = %c1_i32) -> (i32) {
      %58 = arith.index_cast %arg0 : index to i32
      %59 = arith.addi %41, %c1_i32 : i32
      %60 = arith.index_cast %59 : i32 to index
      %61 = scf.for %arg2 = %c0 to %60 step %c1 iter_args(%arg3 = %arg1) -> (i32) {
        %62 = arith.index_cast %arg2 : index to i32
        %63 = memref.load %alloca_1[%arg0, %arg2] : memref<?x?xi32>
        %64 = memref.load %alloca_2[%arg0, %arg2] : memref<?x?xi32>
        %65 = arith.cmpi ne, %63, %64 : i32
        %66 = arith.select %65, %c0_i32, %arg3 : i32
        scf.if %65 {
          %67 = llvm.mlir.addressof @str4 : !llvm.ptr
          %68 = llvm.getelementptr %67[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<50 x i8>
          %69 = llvm.call @printf(%68, %58, %62, %63, %64) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
        }
        scf.yield %66 : i32
      }
      scf.yield %61 : i32
    }
    %57 = arith.cmpi ne, %56, %c0_i32 : i32
    scf.if %57 {
      %58 = llvm.mlir.addressof @str5 : !llvm.ptr
      %59 = llvm.getelementptr %58[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<16 x i8>
      %60 = llvm.call @printf(%59) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    } else {
      %58 = llvm.mlir.addressof @str6 : !llvm.ptr
      %59 = llvm.getelementptr %58[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
      %60 = llvm.call @printf(%59) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %c0_i32 : i32
  }
  func.func private @strlen(!llvm.ptr) -> i64
}
