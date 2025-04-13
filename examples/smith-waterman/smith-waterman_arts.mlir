
-----------------------------------------
DatablockPass STARTED
-----------------------------------------
Converting datablocks to parameters - Analyzing 10 datablocks
- Converting datablock to parameter: %86 = arts.datablock "in" ptr[%1 : memref<9xi8>], indices[%76], sizes[], type[i8], typeSize[%c1] {ptrDb, singleSize} -> memref<i8>
- Converting datablock to parameter: %89 = arts.datablock "in" ptr[%0 : memref<8xi8>], indices[%78], sizes[], type[i8], typeSize[%c1] {ptrDb, singleSize} -> memref<i8>
Analyzing edt deps - Size: 6
Removing datablock dependency: %87 = arts.datablock "in" ptr[%1 : memref<9xi8>], indices[%76], sizes[], type[i8], typeSize[%c1] {ptrDb, singleSize} -> memref<i8>
Removing datablock dependency: %90 = arts.datablock "in" ptr[%0 : memref<8xi8>], indices[%78], sizes[], type[i8], typeSize[%c1] {ptrDb, singleSize} -> memref<i8>
Removing datablocks 1
No graph for function: strlen
-----------------------------------------
DatablockPass FINISHED
-----------------------------------------
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
    %c4 = arith.constant 4 : index
    %c9 = arith.constant 9 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c7 = arith.constant 7 : index
    %c6 = arith.constant 6 : index
    %c5 = arith.constant 5 : index
    %c3 = arith.constant 3 : index
    %c2 = arith.constant 2 : index
    %c0 = arith.constant 0 : index
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c-2_i32 = arith.constant -2 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c2_i32 = arith.constant 2 : i32
    %alloca = memref.alloca() : memref<8xi8>
    %0 = arts.datablock "in" ptr[%alloca : memref<8xi8>], indices[], sizes[], type[memref<8xi8>], typeSize[%c8] {singleSize} -> memref<8xi8>
    %alloca_0 = memref.alloca() : memref<9xi8>
    %1 = arts.datablock "in" ptr[%alloca_0 : memref<9xi8>], indices[], sizes[], type[memref<9xi8>], typeSize[%c9] {singleSize} -> memref<9xi8>
    %2 = llvm.mlir.addressof @str0 : !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %4 = llvm.load %3 : !llvm.ptr -> i8
    memref.store %4, %1[%c0] : memref<9xi8>
    %5 = llvm.getelementptr %2[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %6 = llvm.load %5 : !llvm.ptr -> i8
    memref.store %6, %1[%c1] : memref<9xi8>
    %7 = llvm.getelementptr %2[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %8 = llvm.load %7 : !llvm.ptr -> i8
    memref.store %8, %1[%c2] : memref<9xi8>
    %9 = llvm.getelementptr %2[0, 3] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %10 = llvm.load %9 : !llvm.ptr -> i8
    memref.store %10, %1[%c3] : memref<9xi8>
    %11 = llvm.getelementptr %2[0, 4] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %12 = llvm.load %11 : !llvm.ptr -> i8
    memref.store %12, %1[%c4] : memref<9xi8>
    %13 = llvm.getelementptr %2[0, 5] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %14 = llvm.load %13 : !llvm.ptr -> i8
    memref.store %14, %1[%c5] : memref<9xi8>
    %15 = llvm.getelementptr %2[0, 6] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %16 = llvm.load %15 : !llvm.ptr -> i8
    memref.store %16, %1[%c6] : memref<9xi8>
    %17 = llvm.getelementptr %2[0, 7] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %18 = llvm.load %17 : !llvm.ptr -> i8
    memref.store %18, %1[%c7] : memref<9xi8>
    %19 = llvm.getelementptr %2[0, 8] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %20 = llvm.load %19 : !llvm.ptr -> i8
    memref.store %20, %1[%c8] : memref<9xi8>
    %21 = llvm.mlir.addressof @str1 : !llvm.ptr
    %22 = llvm.getelementptr %21[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %23 = llvm.load %22 : !llvm.ptr -> i8
    memref.store %23, %0[%c0] : memref<8xi8>
    %24 = llvm.getelementptr %21[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %25 = llvm.load %24 : !llvm.ptr -> i8
    memref.store %25, %0[%c1] : memref<8xi8>
    %26 = llvm.getelementptr %21[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %27 = llvm.load %26 : !llvm.ptr -> i8
    memref.store %27, %0[%c2] : memref<8xi8>
    %28 = llvm.getelementptr %21[0, 3] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %29 = llvm.load %28 : !llvm.ptr -> i8
    memref.store %29, %0[%c3] : memref<8xi8>
    %30 = llvm.getelementptr %21[0, 4] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %31 = llvm.load %30 : !llvm.ptr -> i8
    memref.store %31, %0[%c4] : memref<8xi8>
    %32 = llvm.getelementptr %21[0, 5] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %33 = llvm.load %32 : !llvm.ptr -> i8
    memref.store %33, %0[%c5] : memref<8xi8>
    %34 = llvm.getelementptr %21[0, 6] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %35 = llvm.load %34 : !llvm.ptr -> i8
    memref.store %35, %0[%c6] : memref<8xi8>
    %36 = llvm.getelementptr %21[0, 7] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %37 = llvm.load %36 : !llvm.ptr -> i8
    memref.store %37, %0[%c7] : memref<8xi8>
    %38 = "polygeist.memref2pointer"(%1) : (memref<9xi8>) -> !llvm.ptr
    %39 = call @strlen(%38) : (!llvm.ptr) -> i64
    %40 = arith.trunci %39 : i64 to i32
    %41 = "polygeist.memref2pointer"(%0) : (memref<8xi8>) -> !llvm.ptr
    %42 = call @strlen(%41) : (!llvm.ptr) -> i64
    %43 = arith.trunci %42 : i64 to i32
    %44 = arith.addi %40, %c1_i32 : i32
    %45 = arith.index_cast %44 : i32 to index
    %46 = arith.addi %43, %c1_i32 : i32
    %47 = arith.index_cast %46 : i32 to index
    %alloca_1 = memref.alloca(%45, %47) : memref<?x?xi32>
    %48 = arts.datablock "inout" ptr[%alloca_1 : memref<?x?xi32>], indices[], sizes[%45, %47], type[i32], typeSize[%c4] -> memref<?x?xi32>
    %49 = arith.addi %40, %c1_i32 : i32
    %50 = arith.index_cast %49 : i32 to index
    %51 = arith.addi %43, %c1_i32 : i32
    %52 = arith.index_cast %51 : i32 to index
    %alloca_2 = memref.alloca(%50, %52) : memref<?x?xi32>
    %53 = arith.addi %40, %c1_i32 : i32
    %54 = arith.index_cast %53 : i32 to index
    scf.for %arg0 = %c0 to %54 step %c1 {
      %63 = arith.addi %43, %c1_i32 : i32
      %64 = arith.index_cast %63 : i32 to index
      scf.for %arg1 = %c0 to %64 step %c1 {
        memref.store %c0_i32, %48[%arg0, %arg1] : memref<?x?xi32>
        memref.store %c0_i32, %alloca_2[%arg0, %arg1] : memref<?x?xi32>
      }
    }
    %55 = arith.addi %40, %c1_i32 : i32
    %56 = arith.index_cast %55 : i32 to index
    arts.edt dependencies(%0, %1, %48) : (memref<8xi8>, memref<9xi8>, memref<?x?xi32>) attributes {sync} {
      %63 = llvm.mlir.addressof @str2 : !llvm.ptr
      %64 = llvm.getelementptr %63[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i8>
      %65 = "polygeist.memref2pointer"(%1) : (memref<9xi8>) -> !llvm.ptr
      %66 = llvm.call @printf(%64, %65) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
      %67 = llvm.mlir.addressof @str3 : !llvm.ptr
      %68 = llvm.getelementptr %67[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i8>
      %69 = "polygeist.memref2pointer"(%0) : (memref<8xi8>) -> !llvm.ptr
      %70 = llvm.call @printf(%68, %69) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
      scf.for %arg0 = %c1 to %56 step %c1 {
        %71 = arith.index_cast %arg0 : index to i32
        %72 = arith.addi %43, %c1_i32 : i32
        %73 = arith.index_cast %72 : i32 to index
        scf.for %arg1 = %c1 to %73 step %c1 {
          %74 = arith.index_cast %arg1 : index to i32
          %75 = arith.addi %71, %c-1_i32 : i32
          %76 = arith.index_cast %75 : i32 to index
          %77 = arith.addi %74, %c-1_i32 : i32
          %78 = arith.index_cast %77 : i32 to index
          %79 = arith.addi %71, %c-1_i32 : i32
          %80 = arith.index_cast %79 : i32 to index
          %81 = arith.addi %74, %c-1_i32 : i32
          %82 = arith.index_cast %81 : i32 to index
          %83 = arts.datablock "in" ptr[%48 : memref<?x?xi32>], indices[%76, %78], sizes[], type[i32], typeSize[%c4] {hasGuid, ptrDb, singleSize} -> memref<i32>
          %84 = arts.datablock "in" ptr[%48 : memref<?x?xi32>], indices[%arg0, %82], sizes[], type[i32], typeSize[%c4] {hasGuid, ptrDb, singleSize} -> memref<i32>
          %85 = arts.datablock "inout" ptr[%48 : memref<?x?xi32>], indices[%arg0, %arg1], sizes[], type[i32], typeSize[%c4] {hasGuid, ptrDb, singleSize} -> memref<i32>
          %86 = memref.load %1[%76] : memref<9xi8>
          %87 = arts.datablock "in" ptr[%48 : memref<?x?xi32>], indices[%80, %arg1], sizes[], type[i32], typeSize[%c4] {hasGuid, ptrDb, singleSize} -> memref<i32>
          %88 = memref.load %0[%78] : memref<8xi8>
          arts.edt dependencies(%83, %84, %85, %87) : (memref<i32>, memref<i32>, memref<i32>, memref<i32>) attributes {task} {
            %89 = memref.load %83[] : memref<i32>
            %90 = arith.cmpi eq, %86, %88 : i8
            %91 = arith.select %90, %c2_i32, %c-1_i32 : i32
            %92 = arith.addi %89, %91 : i32
            %93 = memref.load %87[] : memref<i32>
            %94 = arith.addi %93, %c-2_i32 : i32
            %95 = memref.load %84[] : memref<i32>
            %96 = arith.addi %95, %c-2_i32 : i32
            %97 = arith.cmpi sgt, %92, %94 : i32
            %98 = scf.if %97 -> (i32) {
              %101 = arith.cmpi sgt, %92, %96 : i32
              %102 = arith.select %101, %92, %96 : i32
              scf.yield %102 : i32
            } else {
              %101 = arith.cmpi sgt, %94, %96 : i32
              %102 = arith.select %101, %94, %96 : i32
              scf.yield %102 : i32
            }
            memref.store %98, %85[] : memref<i32>
            %99 = memref.load %85[] : memref<i32>
            %100 = arith.cmpi slt, %99, %c0_i32 : i32
            scf.if %100 {
              memref.store %c0_i32, %85[] : memref<i32>
            }
            arts.yield
          }
        }
      }
      arts.yield
    }
    %57 = arith.addi %40, %c1_i32 : i32
    %58 = arith.index_cast %57 : i32 to index
    scf.for %arg0 = %c1 to %58 step %c1 {
      %63 = arith.index_cast %arg0 : index to i32
      %64 = arith.addi %43, %c1_i32 : i32
      %65 = arith.index_cast %64 : i32 to index
      scf.for %arg1 = %c1 to %65 step %c1 {
        %66 = arith.index_cast %arg1 : index to i32
        %67 = arith.addi %63, %c-1_i32 : i32
        %68 = arith.index_cast %67 : i32 to index
        %69 = arith.addi %66, %c-1_i32 : i32
        %70 = arith.index_cast %69 : i32 to index
        %71 = memref.load %alloca_2[%68, %70] : memref<?x?xi32>
        %72 = memref.load %1[%68] : memref<9xi8>
        %73 = memref.load %0[%70] : memref<8xi8>
        %74 = arith.cmpi eq, %72, %73 : i8
        %75 = arith.select %74, %c2_i32, %c-1_i32 : i32
        %76 = arith.addi %71, %75 : i32
        %77 = memref.load %alloca_2[%68, %arg1] : memref<?x?xi32>
        %78 = arith.addi %77, %c-2_i32 : i32
        %79 = memref.load %alloca_2[%arg0, %70] : memref<?x?xi32>
        %80 = arith.addi %79, %c-2_i32 : i32
        %81 = arith.cmpi sgt, %76, %78 : i32
        %82 = scf.if %81 -> (i32) {
          %84 = arith.cmpi sgt, %76, %80 : i32
          %85 = arith.select %84, %76, %80 : i32
          scf.yield %85 : i32
        } else {
          %84 = arith.cmpi sgt, %78, %80 : i32
          %85 = arith.select %84, %78, %80 : i32
          scf.yield %85 : i32
        }
        memref.store %82, %alloca_2[%arg0, %arg1] : memref<?x?xi32>
        %83 = arith.cmpi slt, %82, %c0_i32 : i32
        scf.if %83 {
          memref.store %c0_i32, %alloca_2[%arg0, %arg1] : memref<?x?xi32>
        }
      }
    }
    %59 = arith.addi %40, %c1_i32 : i32
    %60 = arith.index_cast %59 : i32 to index
    %61 = scf.for %arg0 = %c0 to %60 step %c1 iter_args(%arg1 = %c1_i32) -> (i32) {
      %63 = arith.index_cast %arg0 : index to i32
      %64 = arith.addi %43, %c1_i32 : i32
      %65 = arith.index_cast %64 : i32 to index
      %66 = scf.for %arg2 = %c0 to %65 step %c1 iter_args(%arg3 = %arg1) -> (i32) {
        %67 = arith.index_cast %arg2 : index to i32
        %68 = memref.load %48[%arg0, %arg2] : memref<?x?xi32>
        %69 = memref.load %alloca_2[%arg0, %arg2] : memref<?x?xi32>
        %70 = arith.cmpi ne, %68, %69 : i32
        %71 = arith.select %70, %c0_i32, %arg3 : i32
        scf.if %70 {
          %72 = llvm.mlir.addressof @str4 : !llvm.ptr
          %73 = llvm.getelementptr %72[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<50 x i8>
          %74 = llvm.call @printf(%73, %63, %67, %68, %69) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
        }
        scf.yield %71 : i32
      }
      scf.yield %66 : i32
    }
    %62 = arith.cmpi ne, %61, %c0_i32 : i32
    scf.if %62 {
      %63 = llvm.mlir.addressof @str5 : !llvm.ptr
      %64 = llvm.getelementptr %63[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<16 x i8>
      %65 = llvm.call @printf(%64) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    } else {
      %63 = llvm.mlir.addressof @str6 : !llvm.ptr
      %64 = llvm.getelementptr %63[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
      %65 = llvm.call @printf(%64) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %c0_i32 : i32
  }
  func.func private @strlen(!llvm.ptr) -> i64
}
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsRT(i32, memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsShutdown() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsPersistentEventIncrementLatch(i64, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsPersistentEventDecrementLatch(i64, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsAddDependenceToPersistentEvent(i64, i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsPersistentEventCreate(i32, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsWaitOnHandle(i64) -> i1 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, llvm.readnone}
  llvm.mlir.global internal constant @str6("Results DO NOT match!\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Results match!\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Mismatch at (%d, %d): Parallel=%d, Sequential=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("seq2: %s\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("seq1: %s\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str1("TATGCGC\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("AGTACGCA\00") {addr_space = 0 : i32}
  func.func private @strlen(!llvm.ptr) -> i64
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    return
  }
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c1_i32 = arith.constant 1 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c4 = arith.constant 4 : index
    %c2_i32 = arith.constant 2 : i32
    %c-2_i32 = arith.constant -2 : i32
    %c0_i32 = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %c1_0 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1_0] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %c2 = arith.constant 2 : index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.trunci %4 : i64 to i32
    %c3 = arith.constant 3 : index
    %6 = memref.load %arg1[%c3] : memref<?xi64>
    %7 = arith.index_cast %6 : i64 to index
    %c4_1 = arith.constant 4 : index
    %8 = memref.load %arg1[%c4_1] : memref<?xi64>
    %9 = arith.index_cast %8 : i64 to index
    %c5 = arith.constant 5 : index
    %10 = memref.load %arg1[%c5] : memref<?xi64>
    %11 = arith.index_cast %10 : i64 to index
    %c0_2 = arith.constant 0 : index
    %c1_3 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0_2, %alloca[] : memref<index>
    %12 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, ptr)>}> : () -> index
    %13 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %14 = memref.load %alloca[] : memref<index>
    %15 = arith.muli %14, %12 : index
    %16 = arith.index_cast %15 : index to i32
    %17 = llvm.getelementptr %13[%16] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32_4 = arith.constant 0 : i32
    %18 = llvm.getelementptr %17[%c0_i32_4, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %19 = llvm.load %18 : !llvm.ptr -> i64
    %c0_i32_5 = arith.constant 0 : i32
    %c2_i32_6 = arith.constant 2 : i32
    %20 = llvm.getelementptr %17[%c0_i32_5, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %21 = llvm.load %20 : !llvm.ptr -> memref<?xi8>
    %22 = arith.addi %14, %c1_3 : index
    memref.store %22, %alloca[] : memref<index>
    %23 = memref.load %alloca[] : memref<index>
    %24 = arith.muli %23, %12 : index
    %25 = arith.index_cast %24 : index to i32
    %26 = llvm.getelementptr %13[%25] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32_7 = arith.constant 0 : i32
    %27 = llvm.getelementptr %26[%c0_i32_7, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %28 = llvm.load %27 : !llvm.ptr -> i64
    %c0_i32_8 = arith.constant 0 : i32
    %c2_i32_9 = arith.constant 2 : i32
    %29 = llvm.getelementptr %26[%c0_i32_8, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %30 = llvm.load %29 : !llvm.ptr -> memref<?xi8>
    %31 = arith.addi %23, %c1_3 : index
    memref.store %31, %alloca[] : memref<index>
    %alloca_10 = memref.alloca(%9, %11) : memref<?x?xi64>
    %alloca_11 = memref.alloca(%9, %11) : memref<?x?xmemref<?xi8>>
    %c0_12 = arith.constant 0 : index
    scf.for %arg4 = %c0_12 to %9 step %c1_3 {
      %c0_16 = arith.constant 0 : index
      scf.for %arg5 = %c0_16 to %11 step %c1_3 {
        %45 = memref.load %alloca[] : memref<index>
        %46 = arith.muli %45, %12 : index
        %47 = arith.index_cast %46 : index to i32
        %48 = llvm.getelementptr %13[%47] : (!llvm.ptr, i32) -> !llvm.ptr, i8
        %c0_i32_17 = arith.constant 0 : i32
        %49 = llvm.getelementptr %48[%c0_i32_17, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
        %50 = llvm.load %49 : !llvm.ptr -> i64
        %c0_i32_18 = arith.constant 0 : i32
        %c2_i32_19 = arith.constant 2 : i32
        %51 = llvm.getelementptr %48[%c0_i32_18, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
        %52 = llvm.load %51 : !llvm.ptr -> memref<?xi8>
        memref.store %50, %alloca_10[%arg4, %arg5] : memref<?x?xi64>
        memref.store %52, %alloca_11[%arg4, %arg5] : memref<?x?xmemref<?xi8>>
        %53 = arith.addi %45, %c1_3 : index
        memref.store %53, %alloca[] : memref<index>
      }
    }
    %32 = "polygeist.memref2pointer"(%30) : (memref<?xi8>) -> !llvm.ptr
    %33 = llvm.load %32 : !llvm.ptr -> memref<9xi8>
    %34 = "polygeist.memref2pointer"(%21) : (memref<?xi8>) -> !llvm.ptr
    %35 = llvm.load %34 : !llvm.ptr -> memref<8xi8>
    %36 = call @artsGetCurrentNode() : () -> i32
    %alloca_13 = memref.alloca(%9, %11) : memref<?x?xi64>
    %c0_14 = arith.constant 0 : index
    %c1_15 = arith.constant 1 : index
    scf.for %arg4 = %c0_14 to %9 step %c1_15 {
      %c0_16 = arith.constant 0 : index
      %c1_17 = arith.constant 1 : index
      scf.for %arg5 = %c0_16 to %11 step %c1_17 {
        %c0_i32_18 = arith.constant 0 : i32
        %c0_i64 = arith.constant 0 : i64
        %45 = func.call @artsPersistentEventCreate(%36, %c0_i32_18, %c0_i64) : (i32, i32, i64) -> i64
        memref.store %45, %alloca_13[%arg4, %arg5] : memref<?x?xi64>
      }
    }
    %37 = llvm.mlir.addressof @str2 : !llvm.ptr
    %38 = llvm.getelementptr %37[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i8>
    %39 = "polygeist.memref2pointer"(%33) : (memref<9xi8>) -> !llvm.ptr
    %40 = llvm.call @printf(%38, %39) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
    %41 = llvm.mlir.addressof @str3 : !llvm.ptr
    %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i8>
    %43 = "polygeist.memref2pointer"(%35) : (memref<8xi8>) -> !llvm.ptr
    %44 = llvm.call @printf(%42, %43) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
    scf.for %arg4 = %c1 to %7 step %c1 {
      %45 = arith.index_cast %arg4 : index to i32
      %46 = arith.addi %5, %c1_i32 : i32
      %47 = arith.index_cast %46 : i32 to index
      scf.for %arg5 = %c1 to %47 step %c1 {
        %48 = arith.index_cast %arg5 : index to i32
        %49 = arith.addi %45, %c-1_i32 : i32
        %50 = arith.index_cast %49 : i32 to index
        %51 = arith.addi %48, %c-1_i32 : i32
        %52 = arith.index_cast %51 : i32 to index
        %53 = arith.addi %45, %c-1_i32 : i32
        %54 = arith.index_cast %53 : i32 to index
        %55 = arith.addi %48, %c-1_i32 : i32
        %56 = arith.index_cast %55 : i32 to index
        %57 = memref.load %33[%50] : memref<9xi8>
        %58 = memref.load %35[%52] : memref<8xi8>
        %59 = func.call @artsGetCurrentEpochGuid() : () -> i64
        %60 = func.call @artsGetCurrentNode() : () -> i32
        %c0_16 = arith.constant 0 : index
        %alloca_17 = memref.alloca() : memref<index>
        memref.store %c0_16, %alloca_17[] : memref<index>
        %alloca_18 = memref.alloca() : memref<index>
        memref.store %c0_16, %alloca_18[] : memref<index>
        %61 = memref.load %alloca_18[] : memref<index>
        %c1_19 = arith.constant 1 : index
        %62 = memref.load %alloca_18[] : memref<index>
        %63 = arith.addi %62, %c1_19 : index
        memref.store %63, %alloca_18[] : memref<index>
        %64 = memref.load %alloca_18[] : memref<index>
        %c1_20 = arith.constant 1 : index
        %65 = memref.load %alloca_18[] : memref<index>
        %66 = arith.addi %65, %c1_20 : index
        memref.store %66, %alloca_18[] : memref<index>
        %67 = memref.load %alloca_18[] : memref<index>
        %c1_21 = arith.constant 1 : index
        %68 = memref.load %alloca_18[] : memref<index>
        %69 = arith.addi %68, %c1_21 : index
        memref.store %69, %alloca_18[] : memref<index>
        %70 = memref.load %alloca_17[] : memref<index>
        %71 = arith.addi %70, %c1_21 : index
        memref.store %71, %alloca_17[] : memref<index>
        %72 = memref.load %alloca_18[] : memref<index>
        %c1_22 = arith.constant 1 : index
        %73 = memref.load %alloca_18[] : memref<index>
        %74 = arith.addi %73, %c1_22 : index
        memref.store %74, %alloca_18[] : memref<index>
        %75 = memref.load %alloca_18[] : memref<index>
        %76 = arith.index_cast %75 : index to i32
        %alloca_23 = memref.alloca() : memref<index>
        %c2_24 = arith.constant 2 : index
        memref.store %c2_24, %alloca_23[] : memref<index>
        %77 = memref.load %alloca_23[] : memref<index>
        %78 = memref.load %alloca_17[] : memref<index>
        %79 = arith.addi %77, %78 : index
        memref.store %79, %alloca_23[] : memref<index>
        %80 = memref.load %alloca_23[] : memref<index>
        %81 = arith.index_cast %80 : index to i32
        %alloca_25 = memref.alloca(%80) : memref<?xi64>
        %c0_26 = arith.constant 0 : index
        %82 = arith.extsi %57 : i8 to i64
        memref.store %82, %alloca_25[%c0_26] : memref<?xi64>
        %c1_27 = arith.constant 1 : index
        %83 = arith.extsi %58 : i8 to i64
        memref.store %83, %alloca_25[%c1_27] : memref<?xi64>
        %alloca_28 = memref.alloca() : memref<index>
        %c2_29 = arith.constant 2 : index
        memref.store %c2_29, %alloca_28[] : memref<index>
        %84 = memref.load %alloca_13[%arg4, %arg5] : memref<?x?xi64>
        %85 = memref.load %alloca_28[] : memref<index>
        memref.store %84, %alloca_25[%85] : memref<?xi64>
        %c1_30 = arith.constant 1 : index
        %86 = arith.addi %85, %c1_30 : index
        memref.store %86, %alloca_28[] : memref<index>
        %87 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
        %88 = "polygeist.pointer2memref"(%87) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
        %89 = func.call @artsEdtCreateWithEpoch(%88, %60, %81, %alloca_25, %76, %59) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
        %90 = memref.load %alloca_13[%50, %52] : memref<?x?xi64>
        %91 = memref.load %alloca_10[%50, %52] : memref<?x?xi64>
        %92 = arith.index_cast %61 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%90, %89, %92, %91) : (i64, i64, i32, i64) -> ()
        %93 = memref.load %alloca_13[%arg4, %56] : memref<?x?xi64>
        %94 = memref.load %alloca_10[%arg4, %56] : memref<?x?xi64>
        %95 = arith.index_cast %64 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%93, %89, %95, %94) : (i64, i64, i32, i64) -> ()
        %96 = memref.load %alloca_13[%arg4, %arg5] : memref<?x?xi64>
        %97 = memref.load %alloca_10[%arg4, %arg5] : memref<?x?xi64>
        %98 = arith.index_cast %67 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%96, %89, %98, %97) : (i64, i64, i32, i64) -> ()
        %99 = memref.load %alloca_13[%54, %arg5] : memref<?x?xi64>
        %100 = memref.load %alloca_10[%54, %arg5] : memref<?x?xi64>
        %101 = arith.index_cast %72 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%99, %89, %101, %100) : (i64, i64, i32, i64) -> ()
        %102 = memref.load %alloca_13[%arg4, %arg5] : memref<?x?xi64>
        %103 = memref.load %alloca_10[%arg4, %arg5] : memref<?x?xi64>
        func.call @artsPersistentEventIncrementLatch(%102, %103) : (i64, i64) -> ()
      }
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c2_i32 = arith.constant 2 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c-2_i32 = arith.constant -2 : i32
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i8
    %c1 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.trunci %2 : i64 to i8
    %c0_0 = arith.constant 0 : index
    %c1_1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0_0, %alloca[] : memref<index>
    %4 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, ptr)>}> : () -> index
    %5 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %6 = memref.load %alloca[] : memref<index>
    %7 = arith.muli %6, %4 : index
    %8 = arith.index_cast %7 : index to i32
    %9 = llvm.getelementptr %5[%8] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32_2 = arith.constant 0 : i32
    %10 = llvm.getelementptr %9[%c0_i32_2, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %11 = llvm.load %10 : !llvm.ptr -> i64
    %c0_i32_3 = arith.constant 0 : i32
    %c2_i32_4 = arith.constant 2 : i32
    %12 = llvm.getelementptr %9[%c0_i32_3, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %13 = llvm.load %12 : !llvm.ptr -> memref<?xi8>
    %14 = arith.addi %6, %c1_1 : index
    memref.store %14, %alloca[] : memref<index>
    %15 = memref.load %alloca[] : memref<index>
    %16 = arith.muli %15, %4 : index
    %17 = arith.index_cast %16 : index to i32
    %18 = llvm.getelementptr %5[%17] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32_5 = arith.constant 0 : i32
    %19 = llvm.getelementptr %18[%c0_i32_5, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %20 = llvm.load %19 : !llvm.ptr -> i64
    %c0_i32_6 = arith.constant 0 : i32
    %c2_i32_7 = arith.constant 2 : i32
    %21 = llvm.getelementptr %18[%c0_i32_6, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %22 = llvm.load %21 : !llvm.ptr -> memref<?xi8>
    %23 = arith.addi %15, %c1_1 : index
    memref.store %23, %alloca[] : memref<index>
    %24 = memref.load %alloca[] : memref<index>
    %25 = arith.muli %24, %4 : index
    %26 = arith.index_cast %25 : index to i32
    %27 = llvm.getelementptr %5[%26] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32_8 = arith.constant 0 : i32
    %28 = llvm.getelementptr %27[%c0_i32_8, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %29 = llvm.load %28 : !llvm.ptr -> i64
    %c0_i32_9 = arith.constant 0 : i32
    %c2_i32_10 = arith.constant 2 : i32
    %30 = llvm.getelementptr %27[%c0_i32_9, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %31 = llvm.load %30 : !llvm.ptr -> memref<?xi8>
    %32 = arith.addi %24, %c1_1 : index
    memref.store %32, %alloca[] : memref<index>
    %33 = memref.load %alloca[] : memref<index>
    %34 = arith.muli %33, %4 : index
    %35 = arith.index_cast %34 : index to i32
    %36 = llvm.getelementptr %5[%35] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32_11 = arith.constant 0 : i32
    %37 = llvm.getelementptr %36[%c0_i32_11, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %38 = llvm.load %37 : !llvm.ptr -> i64
    %c0_i32_12 = arith.constant 0 : i32
    %c2_i32_13 = arith.constant 2 : i32
    %39 = llvm.getelementptr %36[%c0_i32_12, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %40 = llvm.load %39 : !llvm.ptr -> memref<?xi8>
    %41 = arith.addi %33, %c1_1 : index
    memref.store %41, %alloca[] : memref<index>
    %42 = "polygeist.memref2pointer"(%13) : (memref<?xi8>) -> !llvm.ptr
    %43 = llvm.load %42 : !llvm.ptr -> !llvm.ptr
    %44 = llvm.load %43 : !llvm.ptr -> i32
    %45 = arith.cmpi eq, %1, %3 : i8
    %46 = arith.select %45, %c2_i32, %c-1_i32 : i32
    %47 = arith.addi %44, %46 : i32
    %48 = "polygeist.memref2pointer"(%40) : (memref<?xi8>) -> !llvm.ptr
    %49 = llvm.load %48 : !llvm.ptr -> !llvm.ptr
    %50 = llvm.load %49 : !llvm.ptr -> i32
    %51 = arith.addi %50, %c-2_i32 : i32
    %52 = "polygeist.memref2pointer"(%22) : (memref<?xi8>) -> !llvm.ptr
    %53 = llvm.load %52 : !llvm.ptr -> !llvm.ptr
    %54 = llvm.load %53 : !llvm.ptr -> i32
    %55 = arith.addi %54, %c-2_i32 : i32
    %56 = arith.cmpi sgt, %47, %51 : i32
    %57 = scf.if %56 -> (i32) {
      %70 = arith.cmpi sgt, %47, %55 : i32
      %71 = arith.select %70, %47, %55 : i32
      scf.yield %71 : i32
    } else {
      %70 = arith.cmpi sgt, %51, %55 : i32
      %71 = arith.select %70, %51, %55 : i32
      scf.yield %71 : i32
    }
    %58 = "polygeist.memref2pointer"(%31) : (memref<?xi8>) -> !llvm.ptr
    %59 = llvm.load %58 : !llvm.ptr -> !llvm.ptr
    llvm.store %57, %59 : i32, !llvm.ptr
    %60 = "polygeist.memref2pointer"(%31) : (memref<?xi8>) -> !llvm.ptr
    %61 = llvm.load %60 : !llvm.ptr -> !llvm.ptr
    %62 = llvm.load %61 : !llvm.ptr -> i32
    %63 = arith.cmpi slt, %62, %c0_i32 : i32
    scf.if %63 {
      %70 = "polygeist.memref2pointer"(%31) : (memref<?xi8>) -> !llvm.ptr
      %71 = llvm.load %70 : !llvm.ptr -> !llvm.ptr
      llvm.store %c0_i32, %71 : i32, !llvm.ptr
    }
    %alloca_14 = memref.alloca() : memref<index>
    %c2 = arith.constant 2 : index
    memref.store %c2, %alloca_14[] : memref<index>
    %64 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    %65 = memref.load %alloca_14[] : memref<index>
    %66 = arith.index_cast %65 : index to i32
    %67 = llvm.getelementptr %64[%66] : (!llvm.ptr, i32) -> !llvm.ptr, i64
    %68 = llvm.load %67 : !llvm.ptr -> i64
    call @artsPersistentEventDecrementLatch(%68, %29) : (i64, i64) -> ()
    %c1_15 = arith.constant 1 : index
    %69 = arith.addi %65, %c1_15 : index
    memref.store %69, %alloca_14[] : memref<index>
    return
  }
  func.func @mainBody() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4 = arith.constant 4 : index
    %c9 = arith.constant 9 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c7 = arith.constant 7 : index
    %c6 = arith.constant 6 : index
    %c5 = arith.constant 5 : index
    %c3 = arith.constant 3 : index
    %c2 = arith.constant 2 : index
    %c0 = arith.constant 0 : index
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c-2_i32 = arith.constant -2 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c2_i32 = arith.constant 2 : i32
    %0 = call @artsGetCurrentNode() : () -> i32
    %alloca = memref.alloca() : memref<i64>
    %c0_i32_0 = arith.constant 0 : i32
    %c0_i64 = arith.constant 0 : i64
    %1 = call @artsPersistentEventCreate(%0, %c0_i32_0, %c0_i64) : (i32, i32, i64) -> i64
    memref.store %1, %alloca[] : memref<i64>
    %2 = call @artsGetCurrentNode() : () -> i32
    %3 = arith.index_cast %c8 : index to i64
    %c7_i32 = arith.constant 7 : i32
    %4 = call @artsReserveGuidRoute(%c7_i32, %2) : (i32, i32) -> i64
    %5 = call @artsDbCreateWithGuid(%4, %3) : (i64, i64) -> memref<?xi8>
    %6 = "polygeist.memref2pointer"(%5) : (memref<?xi8>) -> !llvm.ptr
    %7 = llvm.load %6 : !llvm.ptr -> memref<8xi8>
    %8 = call @artsGetCurrentNode() : () -> i32
    %alloca_1 = memref.alloca() : memref<i64>
    %c0_i32_2 = arith.constant 0 : i32
    %c0_i64_3 = arith.constant 0 : i64
    %9 = call @artsPersistentEventCreate(%8, %c0_i32_2, %c0_i64_3) : (i32, i32, i64) -> i64
    memref.store %9, %alloca_1[] : memref<i64>
    %10 = call @artsGetCurrentNode() : () -> i32
    %11 = arith.index_cast %c9 : index to i64
    %c7_i32_4 = arith.constant 7 : i32
    %12 = call @artsReserveGuidRoute(%c7_i32_4, %10) : (i32, i32) -> i64
    %13 = call @artsDbCreateWithGuid(%12, %11) : (i64, i64) -> memref<?xi8>
    %14 = "polygeist.memref2pointer"(%13) : (memref<?xi8>) -> !llvm.ptr
    %15 = llvm.load %14 : !llvm.ptr -> memref<9xi8>
    %16 = llvm.mlir.addressof @str0 : !llvm.ptr
    %17 = llvm.getelementptr %16[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %18 = llvm.load %17 : !llvm.ptr -> i8
    memref.store %18, %15[%c0] : memref<9xi8>
    %19 = llvm.getelementptr %16[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %20 = llvm.load %19 : !llvm.ptr -> i8
    memref.store %20, %15[%c1] : memref<9xi8>
    %21 = llvm.getelementptr %16[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %22 = llvm.load %21 : !llvm.ptr -> i8
    memref.store %22, %15[%c2] : memref<9xi8>
    %23 = llvm.getelementptr %16[0, 3] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %24 = llvm.load %23 : !llvm.ptr -> i8
    memref.store %24, %15[%c3] : memref<9xi8>
    %25 = llvm.getelementptr %16[0, 4] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %26 = llvm.load %25 : !llvm.ptr -> i8
    memref.store %26, %15[%c4] : memref<9xi8>
    %27 = llvm.getelementptr %16[0, 5] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %28 = llvm.load %27 : !llvm.ptr -> i8
    memref.store %28, %15[%c5] : memref<9xi8>
    %29 = llvm.getelementptr %16[0, 6] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %30 = llvm.load %29 : !llvm.ptr -> i8
    memref.store %30, %15[%c6] : memref<9xi8>
    %31 = llvm.getelementptr %16[0, 7] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %32 = llvm.load %31 : !llvm.ptr -> i8
    memref.store %32, %15[%c7] : memref<9xi8>
    %33 = llvm.getelementptr %16[0, 8] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %34 = llvm.load %33 : !llvm.ptr -> i8
    memref.store %34, %15[%c8] : memref<9xi8>
    %35 = llvm.mlir.addressof @str1 : !llvm.ptr
    %36 = llvm.getelementptr %35[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %37 = llvm.load %36 : !llvm.ptr -> i8
    memref.store %37, %7[%c0] : memref<8xi8>
    %38 = llvm.getelementptr %35[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %39 = llvm.load %38 : !llvm.ptr -> i8
    memref.store %39, %7[%c1] : memref<8xi8>
    %40 = llvm.getelementptr %35[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %41 = llvm.load %40 : !llvm.ptr -> i8
    memref.store %41, %7[%c2] : memref<8xi8>
    %42 = llvm.getelementptr %35[0, 3] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %43 = llvm.load %42 : !llvm.ptr -> i8
    memref.store %43, %7[%c3] : memref<8xi8>
    %44 = llvm.getelementptr %35[0, 4] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %45 = llvm.load %44 : !llvm.ptr -> i8
    memref.store %45, %7[%c4] : memref<8xi8>
    %46 = llvm.getelementptr %35[0, 5] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %47 = llvm.load %46 : !llvm.ptr -> i8
    memref.store %47, %7[%c5] : memref<8xi8>
    %48 = llvm.getelementptr %35[0, 6] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %49 = llvm.load %48 : !llvm.ptr -> i8
    memref.store %49, %7[%c6] : memref<8xi8>
    %50 = llvm.getelementptr %35[0, 7] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>
    %51 = llvm.load %50 : !llvm.ptr -> i8
    memref.store %51, %7[%c7] : memref<8xi8>
    %52 = "polygeist.memref2pointer"(%15) : (memref<9xi8>) -> !llvm.ptr
    %53 = call @strlen(%52) : (!llvm.ptr) -> i64
    %54 = arith.trunci %53 : i64 to i32
    %55 = "polygeist.memref2pointer"(%7) : (memref<8xi8>) -> !llvm.ptr
    %56 = call @strlen(%55) : (!llvm.ptr) -> i64
    %57 = arith.trunci %56 : i64 to i32
    %58 = arith.addi %54, %c1_i32 : i32
    %59 = arith.index_cast %58 : i32 to index
    %60 = arith.addi %57, %c1_i32 : i32
    %61 = arith.index_cast %60 : i32 to index
    %62 = call @artsGetCurrentNode() : () -> i32
    %alloca_5 = memref.alloca(%59, %61) : memref<?x?xi64>
    %c0_6 = arith.constant 0 : index
    %c1_7 = arith.constant 1 : index
    scf.for %arg0 = %c0_6 to %59 step %c1_7 {
      %c0_38 = arith.constant 0 : index
      %c1_39 = arith.constant 1 : index
      scf.for %arg1 = %c0_38 to %61 step %c1_39 {
        %c0_i32_40 = arith.constant 0 : i32
        %c0_i64_41 = arith.constant 0 : i64
        %118 = func.call @artsPersistentEventCreate(%62, %c0_i32_40, %c0_i64_41) : (i32, i32, i64) -> i64
        memref.store %118, %alloca_5[%arg0, %arg1] : memref<?x?xi64>
      }
    }
    %63 = call @artsGetCurrentNode() : () -> i32
    %64 = arith.index_cast %c4 : index to i64
    %c9_i32 = arith.constant 9 : i32
    %alloca_8 = memref.alloca(%59, %61) : memref<?x?xi64>
    %alloca_9 = memref.alloca(%59, %61) : memref<?x?xmemref<?xi8>>
    %c0_10 = arith.constant 0 : index
    %c1_11 = arith.constant 1 : index
    scf.for %arg0 = %c0_10 to %59 step %c1_11 {
      %c0_38 = arith.constant 0 : index
      %c1_39 = arith.constant 1 : index
      scf.for %arg1 = %c0_38 to %61 step %c1_39 {
        %118 = func.call @artsReserveGuidRoute(%c9_i32, %63) : (i32, i32) -> i64
        %119 = func.call @artsDbCreateWithGuid(%118, %64) : (i64, i64) -> memref<?xi8>
        memref.store %118, %alloca_8[%arg0, %arg1] : memref<?x?xi64>
        memref.store %119, %alloca_9[%arg0, %arg1] : memref<?x?xmemref<?xi8>>
      }
    }
    %65 = arith.addi %54, %c1_i32 : i32
    %66 = arith.index_cast %65 : i32 to index
    %67 = arith.addi %57, %c1_i32 : i32
    %68 = arith.index_cast %67 : i32 to index
    %alloca_12 = memref.alloca(%66, %68) : memref<?x?xi32>
    %69 = arith.addi %54, %c1_i32 : i32
    %70 = arith.index_cast %69 : i32 to index
    scf.for %arg0 = %c0 to %70 step %c1 {
      %118 = arith.addi %57, %c1_i32 : i32
      %119 = arith.index_cast %118 : i32 to index
      scf.for %arg1 = %c0 to %119 step %c1 {
        %120 = polygeist.subindex %alloca_9[%arg0] () : memref<?x?xmemref<?xi8>> -> memref<?xmemref<?xi8>>
        %121 = polygeist.subindex %120[%arg1] () : memref<?xmemref<?xi8>> -> memref<memref<?xi8>>
        %122 = "polygeist.memref2pointer"(%121) : (memref<memref<?xi8>>) -> !llvm.ptr
        %123 = llvm.load %122 : !llvm.ptr -> !llvm.ptr
        llvm.store %c0_i32, %123 : i32, !llvm.ptr
        memref.store %c0_i32, %alloca_12[%arg0, %arg1] : memref<?x?xi32>
      }
    }
    %71 = arith.addi %54, %c1_i32 : i32
    %72 = arith.index_cast %71 : i32 to index
    %73 = call @artsGetCurrentNode() : () -> i32
    %c0_13 = arith.constant 0 : index
    %alloca_14 = memref.alloca() : memref<index>
    memref.store %c0_13, %alloca_14[] : memref<index>
    %alloca_15 = memref.alloca() : memref<index>
    memref.store %c0_13, %alloca_15[] : memref<index>
    %74 = memref.load %alloca_15[] : memref<index>
    %75 = arith.index_cast %74 : index to i32
    %alloca_16 = memref.alloca() : memref<index>
    %c0_17 = arith.constant 0 : index
    memref.store %c0_17, %alloca_16[] : memref<index>
    %76 = memref.load %alloca_16[] : memref<index>
    %77 = arith.index_cast %76 : index to i32
    %alloca_18 = memref.alloca(%76) : memref<?xi64>
    %c1_i32_19 = arith.constant 1 : i32
    %78 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %79 = "polygeist.pointer2memref"(%78) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %80 = call @artsEdtCreate(%79, %73, %77, %alloca_18, %c1_i32_19) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %c0_i32_20 = arith.constant 0 : i32
    %81 = call @artsInitializeAndStartEpoch(%80, %c0_i32_20) : (i64, i32) -> i64
    %82 = call @artsGetCurrentNode() : () -> i32
    %c0_21 = arith.constant 0 : index
    %alloca_22 = memref.alloca() : memref<index>
    memref.store %c0_21, %alloca_22[] : memref<index>
    %alloca_23 = memref.alloca() : memref<index>
    memref.store %c0_21, %alloca_23[] : memref<index>
    %83 = memref.load %alloca_23[] : memref<index>
    %c1_24 = arith.constant 1 : index
    %84 = memref.load %alloca_23[] : memref<index>
    %85 = arith.addi %84, %c1_24 : index
    memref.store %85, %alloca_23[] : memref<index>
    %86 = memref.load %alloca_23[] : memref<index>
    %c1_25 = arith.constant 1 : index
    %87 = memref.load %alloca_23[] : memref<index>
    %88 = arith.addi %87, %c1_25 : index
    memref.store %88, %alloca_23[] : memref<index>
    %89 = memref.load %alloca_23[] : memref<index>
    %c1_26 = arith.constant 1 : index
    %90 = arith.muli %c1_26, %59 : index
    %91 = arith.muli %90, %61 : index
    %92 = memref.load %alloca_23[] : memref<index>
    %93 = arith.addi %92, %91 : index
    memref.store %93, %alloca_23[] : memref<index>
    %94 = memref.load %alloca_23[] : memref<index>
    %95 = arith.index_cast %94 : index to i32
    %alloca_27 = memref.alloca() : memref<index>
    %c6_28 = arith.constant 6 : index
    memref.store %c6_28, %alloca_27[] : memref<index>
    %96 = memref.load %alloca_27[] : memref<index>
    %97 = arith.index_cast %96 : index to i32
    %alloca_29 = memref.alloca(%96) : memref<?xi64>
    %c0_30 = arith.constant 0 : index
    %98 = arith.index_cast %59 : index to i64
    memref.store %98, %alloca_29[%c0_30] : memref<?xi64>
    %c1_31 = arith.constant 1 : index
    %99 = arith.index_cast %61 : index to i64
    memref.store %99, %alloca_29[%c1_31] : memref<?xi64>
    %c2_32 = arith.constant 2 : index
    %100 = arith.extsi %57 : i32 to i64
    memref.store %100, %alloca_29[%c2_32] : memref<?xi64>
    %c3_33 = arith.constant 3 : index
    %101 = arith.index_cast %72 : index to i64
    memref.store %101, %alloca_29[%c3_33] : memref<?xi64>
    %c4_34 = arith.constant 4 : index
    %102 = arith.index_cast %59 : index to i64
    memref.store %102, %alloca_29[%c4_34] : memref<?xi64>
    %c5_35 = arith.constant 5 : index
    %103 = arith.index_cast %61 : index to i64
    memref.store %103, %alloca_29[%c5_35] : memref<?xi64>
    %104 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %105 = "polygeist.pointer2memref"(%104) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %106 = call @artsEdtCreateWithEpoch(%105, %82, %97, %alloca_29, %95, %81) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %107 = memref.load %alloca[] : memref<i64>
    %108 = arith.index_cast %83 : index to i32
    call @artsAddDependenceToPersistentEvent(%107, %106, %108, %4) : (i64, i64, i32, i64) -> ()
    %109 = memref.load %alloca_1[] : memref<i64>
    %110 = arith.index_cast %86 : index to i32
    call @artsAddDependenceToPersistentEvent(%109, %106, %110, %12) : (i64, i64, i32, i64) -> ()
    %alloc = memref.alloc() : memref<index>
    memref.store %89, %alloc[] : memref<index>
    %c0_36 = arith.constant 0 : index
    %c1_37 = arith.constant 1 : index
    scf.for %arg0 = %c0_36 to %59 step %c1_37 {
      %c0_38 = arith.constant 0 : index
      %c1_39 = arith.constant 1 : index
      scf.for %arg1 = %c0_38 to %61 step %c1_39 {
        %118 = memref.load %alloca_5[%arg0, %arg1] : memref<?x?xi64>
        %119 = memref.load %alloc[] : memref<index>
        %120 = memref.load %alloca_8[%arg0, %arg1] : memref<?x?xi64>
        %121 = arith.index_cast %119 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%118, %106, %121, %120) : (i64, i64, i32, i64) -> ()
        %c1_40 = arith.constant 1 : index
        %122 = arith.addi %119, %c1_40 : index
        memref.store %122, %alloc[] : memref<index>
      }
    }
    %111 = call @artsWaitOnHandle(%81) : (i64) -> i1
    %112 = arith.addi %54, %c1_i32 : i32
    %113 = arith.index_cast %112 : i32 to index
    scf.for %arg0 = %c1 to %113 step %c1 {
      %118 = arith.index_cast %arg0 : index to i32
      %119 = arith.addi %57, %c1_i32 : i32
      %120 = arith.index_cast %119 : i32 to index
      scf.for %arg1 = %c1 to %120 step %c1 {
        %121 = arith.index_cast %arg1 : index to i32
        %122 = arith.addi %118, %c-1_i32 : i32
        %123 = arith.index_cast %122 : i32 to index
        %124 = arith.addi %121, %c-1_i32 : i32
        %125 = arith.index_cast %124 : i32 to index
        %126 = memref.load %alloca_12[%123, %125] : memref<?x?xi32>
        %127 = memref.load %15[%123] : memref<9xi8>
        %128 = memref.load %7[%125] : memref<8xi8>
        %129 = arith.cmpi eq, %127, %128 : i8
        %130 = arith.select %129, %c2_i32, %c-1_i32 : i32
        %131 = arith.addi %126, %130 : i32
        %132 = memref.load %alloca_12[%123, %arg1] : memref<?x?xi32>
        %133 = arith.addi %132, %c-2_i32 : i32
        %134 = memref.load %alloca_12[%arg0, %125] : memref<?x?xi32>
        %135 = arith.addi %134, %c-2_i32 : i32
        %136 = arith.cmpi sgt, %131, %133 : i32
        %137 = scf.if %136 -> (i32) {
          %139 = arith.cmpi sgt, %131, %135 : i32
          %140 = arith.select %139, %131, %135 : i32
          scf.yield %140 : i32
        } else {
          %139 = arith.cmpi sgt, %133, %135 : i32
          %140 = arith.select %139, %133, %135 : i32
          scf.yield %140 : i32
        }
        memref.store %137, %alloca_12[%arg0, %arg1] : memref<?x?xi32>
        %138 = arith.cmpi slt, %137, %c0_i32 : i32
        scf.if %138 {
          memref.store %c0_i32, %alloca_12[%arg0, %arg1] : memref<?x?xi32>
        }
      }
    }
    %114 = arith.addi %54, %c1_i32 : i32
    %115 = arith.index_cast %114 : i32 to index
    %116 = scf.for %arg0 = %c0 to %115 step %c1 iter_args(%arg1 = %c1_i32) -> (i32) {
      %118 = arith.index_cast %arg0 : index to i32
      %119 = arith.addi %57, %c1_i32 : i32
      %120 = arith.index_cast %119 : i32 to index
      %121 = scf.for %arg2 = %c0 to %120 step %c1 iter_args(%arg3 = %arg1) -> (i32) {
        %122 = arith.index_cast %arg2 : index to i32
        %123 = polygeist.subindex %alloca_9[%arg0] () : memref<?x?xmemref<?xi8>> -> memref<?xmemref<?xi8>>
        %124 = polygeist.subindex %123[%arg2] () : memref<?xmemref<?xi8>> -> memref<memref<?xi8>>
        %125 = "polygeist.memref2pointer"(%124) : (memref<memref<?xi8>>) -> !llvm.ptr
        %126 = llvm.load %125 : !llvm.ptr -> !llvm.ptr
        %127 = llvm.load %126 : !llvm.ptr -> i32
        %128 = memref.load %alloca_12[%arg0, %arg2] : memref<?x?xi32>
        %129 = arith.cmpi ne, %127, %128 : i32
        %130 = arith.select %129, %c0_i32, %arg3 : i32
        scf.if %129 {
          %131 = llvm.mlir.addressof @str4 : !llvm.ptr
          %132 = llvm.getelementptr %131[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<50 x i8>
          %133 = llvm.call @printf(%132, %118, %122, %127, %128) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
        }
        scf.yield %130 : i32
      }
      scf.yield %121 : i32
    }
    %117 = arith.cmpi ne, %116, %c0_i32 : i32
    scf.if %117 {
      %118 = llvm.mlir.addressof @str5 : !llvm.ptr
      %119 = llvm.getelementptr %118[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<16 x i8>
      %120 = llvm.call @printf(%119) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    } else {
      %118 = llvm.mlir.addressof @str6 : !llvm.ptr
      %119 = llvm.getelementptr %118[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
      %120 = llvm.call @printf(%119) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %c0_i32 : i32
  }
  func.func @initPerWorker(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) {
    return
  }
  func.func @initPerNode(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) {
    %0 = arith.index_cast %arg0 : i32 to index
    %c1 = arith.constant 1 : index
    %1 = arith.cmpi uge, %0, %c1 : index
    cf.cond_br %1, ^bb1, ^bb2(%arg1, %arg2 : i32, memref<?xmemref<?xi8>>)
  ^bb1:  // pred: ^bb0
    return
  ^bb2(%2: i32, %3: memref<?xmemref<?xi8>>):  // pred: ^bb0
    %4 = call @mainBody() : () -> i32
    call @artsShutdown() : () -> ()
    return
  }
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 {
    %0 = call @artsRT(%arg0, %arg1) : (i32, memref<?xmemref<?xi8>>) -> i32
    %c0_i32 = arith.constant 0 : i32
    return %c0_i32 : i32
  }
}

