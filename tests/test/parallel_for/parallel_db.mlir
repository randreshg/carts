module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx15.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
  llvm.mlir.global internal constant @str1("c[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Results:\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c4_i64 = arith.constant 4 : i64
    %c2 = arith.constant 2 : index
    %c2_i32 = arith.constant 2 : i32
    %0 = memref.load %arg1[%c1] : memref<?xmemref<?xi8>>
    %1 = call @atoi(%0) : (memref<?xi8>) -> i32
    %2 = arith.extsi %1 : i32 to i64
    %3 = arith.muli %2, %c4_i64 : i64
    %4 = arith.index_cast %3 : i64 to index
    %5 = arith.divui %4, %c4 : index
    %alloc = memref.alloc(%5) : memref<?xi32>
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <pinned>] route(%c0_i32 : i32) (%alloc : memref<?xi32>) sizes[%5] elementType(i32) payloadSizes[] : (memref<?xi64>, memref<?xi32>)
    %6 = arith.index_cast %1 : i32 to index
    %guid_0, %ptr_1 = arts.db_acquire[<out>] (%guid, %ptr) offsets[%c0] sizes[%5] : (memref<?xi64>, memref<?xi32>) -> (memref<?xi64>, memref<?xi32>)
    arts.edt <parallel> route(%c0_i32) (%ptr_1) : memref<?xi32> {
    ^bb0(%arg2: memref<?xi32>):
      arts.for for(%c0) to(%6) step(%c2) {{
      ^bb0(%arg3: index):
        %10 = arith.index_cast %arg3 : index to i32
        %11 = arith.muli %10, %c2_i32 : i32
        %12 = arith.addi %10, %11 : i32
        memref.store %12, %arg2[%arg3] : memref<?xi32>
      }}
      arts.db_release(%arg2) : memref<?xi32>
    }
    %7 = llvm.mlir.addressof @str0 : !llvm.ptr
    %8 = llvm.getelementptr %7[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i8>
    %9 = llvm.call @printf(%8) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    scf.for %arg2 = %c0 to %6 step %c1 {
      %10 = arith.index_cast %arg2 : index to i32
      %11 = llvm.mlir.addressof @str1 : !llvm.ptr
      %12 = llvm.getelementptr %11[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
      %13 = memref.load %ptr[%arg2] : memref<?xi32>
      %14 = llvm.call @printf(%12, %10, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    }
    arts.db_free(%ptr) : memref<?xi32>
    arts.db_free(%guid) : memref<?xi64>
    return %c0_i32 : i32
  }
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
