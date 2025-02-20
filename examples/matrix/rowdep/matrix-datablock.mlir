-----------------------------------------
IdentifyDatablocksPass STARTED
-----------------------------------------

[identify-datablocks] Analyzing candidate datablocks in function: compute

[identify-datablocks] EDT region
arts.edt dependencies(%12) : (memref<1x1xf64>) {
  %13 = arith.sitofp %11 : i32 to f64
  %14 = arith.addf %13, %8 : f64
  scf.for %arg1 = %c0 to %c100 step %c1 {
    memref.store %14, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
  }
  arts.yield
}

- Datablock
  Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
  Access Type: write-only
  Pinned Indices:
    - <block argument> of type 'index' at index: 0

[identify-datablocks] EDT region
arts.edt dependencies(%9, %10) : (memref<1x1xf64>, memref<1x1xf64>) {
  scf.for %arg0 = %c0 to %c100 step %c1 {
    %11 = memref.load %alloca_0[%c0, %arg0] : memref<100x100xf64>
    memref.store %11, %alloca[%c0, %arg0] : memref<100x100xf64>
  }
  arts.yield
}

- Datablock
  Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
  Access Type: read-only
  Pinned Indices:
    - %c0 = arith.constant 0 : index

- Datablock
  Memref: %alloca = memref.alloca() : memref<100x100xf64>
  Access Type: write-only
  Pinned Indices:
    - %c0 = arith.constant 0 : index

[identify-datablocks] EDT region
arts.edt dependencies(%12, %15, %16) : (memref<1x1xf64>, memref<1x1xf64>, memref<1x1xf64>) {
  scf.for %arg1 = %c0 to %c100 step %c1 {
    %17 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
    %18 = memref.load %alloca_0[%14, %arg1] : memref<100x100xf64>
    %19 = arith.addf %17, %18 : f64
    memref.store %19, %alloca[%arg0, %arg1] : memref<100x100xf64>
  }
  arts.yield
}

- Datablock
  Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
  Access Type: read-only
  Pinned Indices:
    - <block argument> of type 'index' at index: 0

- Datablock
  Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
  Access Type: read-only
  Pinned Indices:
    - %14 = arith.index_cast %13 : i32 to index

- Datablock
  Memref: %alloca = memref.alloca() : memref<100x100xf64>
  Access Type: write-only
  Pinned Indices:
    - <block argument> of type 'index' at index: 0

[identify-datablocks] EDT region
arts.edt {
  %8 = arith.sitofp %1 : i32 to f64
  scf.for %arg0 = %c0 to %c100 step %c1 {
    %11 = arith.index_cast %arg0 : index to i32
    %12 = arts.datablock "out" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
    arts.edt dependencies(%12) : (memref<1x1xf64>) {
      %13 = arith.sitofp %11 : i32 to f64
      %14 = arith.addf %13, %8 : f64
      scf.for %arg1 = %c0 to %c100 step %c1 {
        memref.store %14, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
      }
      arts.yield
    }
  }
  %9 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%c0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
  %10 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%c0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
  arts.edt dependencies(%9, %10) : (memref<1x1xf64>, memref<1x1xf64>) {
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %11 = memref.load %alloca_0[%c0, %arg0] : memref<100x100xf64>
      memref.store %11, %alloca[%c0, %arg0] : memref<100x100xf64>
    }
    arts.yield
  }
  scf.for %arg0 = %c1 to %c100 step %c1 {
    %11 = arith.index_cast %arg0 : index to i32
    %12 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
    %13 = arith.addi %11, %c-1_i32 : i32
    %14 = arith.index_cast %13 : i32 to index
    %15 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%14, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
    %16 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%arg0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
    arts.edt dependencies(%12, %15, %16) : (memref<1x1xf64>, memref<1x1xf64>, memref<1x1xf64>) {
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %17 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
        %18 = memref.load %alloca_0[%14, %arg1] : memref<100x100xf64>
        %19 = arith.addf %17, %18 : f64
        memref.store %19, %alloca[%arg0, %arg1] : memref<100x100xf64>
      }
      arts.yield
    }
  }
  arts.yield
}

- Datablock
  Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
  Access Type: read-write
  Pinned Indices: none


- Datablock
  Memref: %alloca = memref.alloca() : memref<100x100xf64>
  Access Type: write-only
  Pinned Indices: none


[identify-datablocks] Analyzing candidate datablocks in function: rand
-----------------------------------------
IdentifyDatablocksPass FINISHED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100x100xf64>
    %alloca_0 = memref.alloca() : memref<100x100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    arts.edt {
      %8 = arith.sitofp %1 : i32 to f64
      scf.for %arg0 = %c0 to %c100 step %c1 {
        %11 = arith.index_cast %arg0 : index to i32
        %12 = arts.datablock "out" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
        arts.edt dependencies(%12) : (memref<1x1xf64>) {
          %13 = arith.sitofp %11 : i32 to f64
          %14 = arith.addf %13, %8 : f64
          scf.for %arg1 = %c0 to %c100 step %c1 {
            memref.store %14, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
          }
          arts.yield
        }
      }
      %9 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%c0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
      %10 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%c0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
      arts.edt dependencies(%9, %10) : (memref<1x1xf64>, memref<1x1xf64>) {
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %11 = memref.load %alloca_0[%c0, %arg0] : memref<100x100xf64>
          memref.store %11, %alloca[%c0, %arg0] : memref<100x100xf64>
        }
        arts.yield
      }
      scf.for %arg0 = %c1 to %c100 step %c1 {
        %11 = arith.index_cast %arg0 : index to i32
        %12 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
        %13 = arith.addi %11, %c-1_i32 : i32
        %14 = arith.index_cast %13 : i32 to index
        %15 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%14, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
        %16 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%arg0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
        arts.edt dependencies(%12, %15, %16) : (memref<1x1xf64>, memref<1x1xf64>, memref<1x1xf64>) {
          scf.for %arg1 = %c0 to %c100 step %c1 {
            %17 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
            %18 = memref.load %alloca_0[%14, %arg1] : memref<100x100xf64>
            %19 = arith.addf %17, %18 : f64
            memref.store %19, %alloca[%arg0, %arg1] : memref<100x100xf64>
          }
          arts.yield
        }
      }
      arts.yield
    }
    %2 = llvm.mlir.addressof @str0 : !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %4 = llvm.mlir.addressof @str1 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %8 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %10 = arith.index_cast %arg1 : index to i32
        %11 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
        %12 = llvm.call @printf(%3, %8, %10, %11) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %9 = llvm.call @printf(%5) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %6 = llvm.mlir.addressof @str2 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %8 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %10 = arith.index_cast %arg1 : index to i32
        %11 = memref.load %alloca[%arg0, %arg1] : memref<100x100xf64>
        %12 = llvm.call @printf(%7, %8, %10, %11) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %9 = llvm.call @printf(%5) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

