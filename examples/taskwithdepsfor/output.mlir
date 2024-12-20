module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @compute(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %0 = arith.index_cast %arg0 : i32 to index
    scf.for %arg3 = %c0 to %0 step %c1 {
      %1 = arith.index_cast %arg3 : index to i32
      func.call @arts_task_0(%1, %arg1, %arg2) : (i32, memref<?xf64>, memref<?xf64>) -> ()
      func.call @arts_task_1(%1, %arg1, %arg2) : (i32, memref<?xf64>, memref<?xf64>) -> ()
    }
    return
  }
  func.func @arts_parallel_0(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %0 = arith.index_cast %arg0 : i32 to index
    scf.for %arg3 = %c0 to %0 step %c1 {
      %1 = arith.index_cast %arg3 : index to i32
      func.call @arts_task_0(%1, %arg1, %arg2) : (i32, memref<?xf64>, memref<?xf64>) -> ()
      func.call @arts_task_1(%1, %arg1, %arg2) : (i32, memref<?xf64>, memref<?xf64>) -> ()
    }
    return
  }
  func.func @arts_single_0(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %0 = arith.index_cast %arg0 : i32 to index
    scf.for %arg3 = %c0 to %0 step %c1 {
      %1 = arith.index_cast %arg3 : index to i32
      func.call @arts_task_0(%1, %arg1, %arg2) : (i32, memref<?xf64>, memref<?xf64>) -> ()
      func.call @arts_task_1(%1, %arg1, %arg2) : (i32, memref<?xf64>, memref<?xf64>) -> ()
    }
    return
  }
  func.func @arts_task_0(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %0 = arith.index_cast %arg0 : i32 to index
    %1 = arith.sitofp %arg0 : i32 to f64
    affine.store %1, %arg1[symbol(%0)] : memref<?xf64>
    %2 = arith.muli %0, %c8 : index
    %3 = arith.index_cast %2 : index to i64
    %4 = "polygeist.memref2pointer"(%arg1) : (memref<?xf64>) -> !llvm.ptr
    %5 = llvm.getelementptr %4[%3] : (!llvm.ptr, i64) -> !llvm.ptr, i8
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi8>
    call @arts_signal(%6) : (memref<?xi8>) -> ()
    return
  }
  func.func @arts_task_1(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c8 = arith.constant 8 : index
    %0 = arith.index_cast %arg0 : i32 to index
    %1 = arith.muli %0, %c8 : index
    %2 = arith.index_cast %1 : index to i64
    %3 = "polygeist.memref2pointer"(%arg1) : (memref<?xf64>) -> !llvm.ptr
    %4 = llvm.getelementptr %3[%2] : (!llvm.ptr, i64) -> !llvm.ptr, i8
    %5 = "polygeist.pointer2memref"(%4) : (!llvm.ptr) -> memref<?xi8>
    call @arts_acquire(%5) : (memref<?xi8>) -> ()
    %6 = arith.addi %arg0, %c-1_i32 : i32
    %7 = arith.index_cast %6 : i32 to index
    %8 = arith.muli %7, %c8 : index
    %9 = arith.index_cast %8 : index to i64
    %10 = llvm.getelementptr %3[%9] : (!llvm.ptr, i64) -> !llvm.ptr, i8
    %11 = "polygeist.pointer2memref"(%10) : (!llvm.ptr) -> memref<?xi8>
    call @arts_acquire(%11) : (memref<?xi8>) -> ()
    %12 = affine.load %arg1[symbol(%0)] : memref<?xf64>
    %13 = arith.cmpi sgt, %arg0, %c0_i32 : i32
    %14 = scf.if %13 -> (f64) {
      %19 = affine.load %arg1[symbol(%0) - 1] : memref<?xf64>
      scf.yield %19 : f64
    } else {
      scf.yield %cst : f64
    }
    %15 = arith.addf %12, %14 : f64
    affine.store %15, %arg2[symbol(%0)] : memref<?xf64>
    %16 = "polygeist.memref2pointer"(%arg2) : (memref<?xf64>) -> !llvm.ptr
    %17 = llvm.getelementptr %16[%2] : (!llvm.ptr, i64) -> !llvm.ptr, i8
    %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr) -> memref<?xi8>
    call @arts_signal(%18) : (memref<?xi8>) -> ()
    return
  }
  func.func private @arts_signal(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @arts_acquire(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
}
