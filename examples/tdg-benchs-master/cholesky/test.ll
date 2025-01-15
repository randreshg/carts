module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  memref.global "private" @matrix : memref<4194304xf64> = uninitialized
  memref.global "private" @original_matrix : memref<4194304xf64> = uninitialized
  llvm.mlir.global internal constant @str7("ts = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("UNSUCCESSFUL\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("sucessful\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("n/a\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("num_iter must be positive\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("NB = %d, DIM = %d, NB must be smaller than DIM\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Usage: <block number> <num_iterations> \0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("%g ms passed\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  memref.global @NB : memref<1xi32> = dense<32>
  memref.global "private" @"omp_gemm@static@DMONE@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_gemm@static@DMONE" : memref<1xf64> = uninitialized
  memref.global "private" @"omp_gemm@static@DONE@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_gemm@static@DONE" : memref<1xf64> = uninitialized
  memref.global "private" @"omp_gemm@static@NT@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_gemm@static@NT" : memref<1xi8> = uninitialized
  memref.global "private" @"omp_gemm@static@TR@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_gemm@static@TR" : memref<1xi8> = uninitialized
  memref.global "private" @"omp_syrk@static@DMONE@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_syrk@static@DMONE" : memref<1xf64> = uninitialized
  memref.global "private" @"omp_syrk@static@DONE@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_syrk@static@DONE" : memref<1xf64> = uninitialized
  memref.global "private" @"omp_syrk@static@NT@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_syrk@static@NT" : memref<1xi8> = uninitialized
  memref.global "private" @"omp_syrk@static@LO@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_syrk@static@LO" : memref<1xi8> = uninitialized
  memref.global "private" @"omp_trsm@static@DONE@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_trsm@static@DONE" : memref<1xf64> = uninitialized
  memref.global "private" @"omp_trsm@static@RI@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_trsm@static@RI" : memref<1xi8> = uninitialized
  memref.global "private" @"omp_trsm@static@NU@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_trsm@static@NU" : memref<1xi8> = uninitialized
  memref.global "private" @"omp_trsm@static@TR@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_trsm@static@TR" : memref<1xi8> = uninitialized
  memref.global "private" @"omp_trsm@static@LO@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_trsm@static@LO" : memref<1xi8> = uninitialized
  memref.global "private" @"omp_potrf@static@L@init" : memref<1xi1> = dense<true>
  memref.global "private" @"omp_potrf@static@L" : memref<1xi8> = uninitialized
  func.func @omp_potrf(%arg0: memref<?xf64>, %arg1: i32, %arg2: i32, %arg3: memref<?x?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %false = arith.constant false
    %c76_i8 = arith.constant 76 : i8
    %0 = memref.get_global @"omp_potrf@static@L" : memref<1xi8>
    %1 = memref.get_global @"omp_potrf@static@L@init" : memref<1xi1>
    %2 = affine.load %1[0] : memref<1xi1>
    scf.if %2 {
      affine.store %false, %1[0] : memref<1xi1>
      affine.store %c76_i8, %0[0] : memref<1xi8>
    }
    return
  }
  func.func @omp_trsm(%arg0: memref<?xf64>, %arg1: memref<?xf64>, %arg2: i32, %arg3: i32, %arg4: memref<?x?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 1.000000e+00 : f64
    %c82_i8 = arith.constant 82 : i8
    %c78_i8 = arith.constant 78 : i8
    %c84_i8 = arith.constant 84 : i8
    %false = arith.constant false
    %c76_i8 = arith.constant 76 : i8
    %0 = memref.get_global @"omp_trsm@static@DONE" : memref<1xf64>
    %1 = memref.get_global @"omp_trsm@static@RI" : memref<1xi8>
    %2 = memref.get_global @"omp_trsm@static@NU" : memref<1xi8>
    %3 = memref.get_global @"omp_trsm@static@TR" : memref<1xi8>
    %4 = memref.get_global @"omp_trsm@static@LO" : memref<1xi8>
    %5 = memref.get_global @"omp_trsm@static@LO@init" : memref<1xi1>
    %6 = affine.load %5[0] : memref<1xi1>
    scf.if %6 {
      affine.store %false, %5[0] : memref<1xi1>
      affine.store %c76_i8, %4[0] : memref<1xi8>
    }
    %7 = memref.get_global @"omp_trsm@static@TR@init" : memref<1xi1>
    %8 = affine.load %7[0] : memref<1xi1>
    scf.if %8 {
      affine.store %false, %7[0] : memref<1xi1>
      affine.store %c84_i8, %3[0] : memref<1xi8>
    }
    %9 = memref.get_global @"omp_trsm@static@NU@init" : memref<1xi1>
    %10 = affine.load %9[0] : memref<1xi1>
    scf.if %10 {
      affine.store %false, %9[0] : memref<1xi1>
      affine.store %c78_i8, %2[0] : memref<1xi8>
    }
    %11 = memref.get_global @"omp_trsm@static@RI@init" : memref<1xi1>
    %12 = affine.load %11[0] : memref<1xi1>
    scf.if %12 {
      affine.store %false, %11[0] : memref<1xi1>
      affine.store %c82_i8, %1[0] : memref<1xi8>
    }
    %13 = memref.get_global @"omp_trsm@static@DONE@init" : memref<1xi1>
    %14 = affine.load %13[0] : memref<1xi1>
    scf.if %14 {
      affine.store %false, %13[0] : memref<1xi1>
      affine.store %cst, %0[0] : memref<1xf64>
    }
    return
  }
  func.func @omp_syrk(%arg0: memref<?xf64>, %arg1: memref<?xf64>, %arg2: i32, %arg3: i32, %arg4: memref<?x?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant -1.000000e+00 : f64
    %cst_0 = arith.constant 1.000000e+00 : f64
    %c78_i8 = arith.constant 78 : i8
    %false = arith.constant false
    %c76_i8 = arith.constant 76 : i8
    %0 = memref.get_global @"omp_syrk@static@DMONE" : memref<1xf64>
    %1 = memref.get_global @"omp_syrk@static@DONE" : memref<1xf64>
    %2 = memref.get_global @"omp_syrk@static@NT" : memref<1xi8>
    %3 = memref.get_global @"omp_syrk@static@LO" : memref<1xi8>
    %4 = memref.get_global @"omp_syrk@static@LO@init" : memref<1xi1>
    %5 = affine.load %4[0] : memref<1xi1>
    scf.if %5 {
      affine.store %false, %4[0] : memref<1xi1>
      affine.store %c76_i8, %3[0] : memref<1xi8>
    }
    %6 = memref.get_global @"omp_syrk@static@NT@init" : memref<1xi1>
    %7 = affine.load %6[0] : memref<1xi1>
    scf.if %7 {
      affine.store %false, %6[0] : memref<1xi1>
      affine.store %c78_i8, %2[0] : memref<1xi8>
    }
    %8 = memref.get_global @"omp_syrk@static@DONE@init" : memref<1xi1>
    %9 = affine.load %8[0] : memref<1xi1>
    scf.if %9 {
      affine.store %false, %8[0] : memref<1xi1>
      affine.store %cst_0, %1[0] : memref<1xf64>
    }
    %10 = memref.get_global @"omp_syrk@static@DMONE@init" : memref<1xi1>
    %11 = affine.load %10[0] : memref<1xi1>
    scf.if %11 {
      affine.store %false, %10[0] : memref<1xi1>
      affine.store %cst, %0[0] : memref<1xf64>
    }
    return
  }
  func.func @omp_gemm(%arg0: memref<?xf64>, %arg1: memref<?xf64>, %arg2: memref<?xf64>, %arg3: i32, %arg4: i32, %arg5: memref<?x?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant -1.000000e+00 : f64
    %cst_0 = arith.constant 1.000000e+00 : f64
    %c78_i8 = arith.constant 78 : i8
    %false = arith.constant false
    %c84_i8 = arith.constant 84 : i8
    %0 = memref.get_global @"omp_gemm@static@DMONE" : memref<1xf64>
    %1 = memref.get_global @"omp_gemm@static@DONE" : memref<1xf64>
    %2 = memref.get_global @"omp_gemm@static@NT" : memref<1xi8>
    %3 = memref.get_global @"omp_gemm@static@TR" : memref<1xi8>
    %4 = memref.get_global @"omp_gemm@static@TR@init" : memref<1xi1>
    %5 = affine.load %4[0] : memref<1xi1>
    scf.if %5 {
      affine.store %false, %4[0] : memref<1xi1>
      affine.store %c84_i8, %3[0] : memref<1xi8>
    }
    %6 = memref.get_global @"omp_gemm@static@NT@init" : memref<1xi1>
    %7 = affine.load %6[0] : memref<1xi1>
    scf.if %7 {
      affine.store %false, %6[0] : memref<1xi1>
      affine.store %c78_i8, %2[0] : memref<1xi8>
    }
    %8 = memref.get_global @"omp_gemm@static@DONE@init" : memref<1xi1>
    %9 = affine.load %8[0] : memref<1xi1>
    scf.if %9 {
      affine.store %false, %8[0] : memref<1xi1>
      affine.store %cst_0, %1[0] : memref<1xf64>
    }
    %10 = memref.get_global @"omp_gemm@static@DMONE@init" : memref<1xi1>
    %11 = affine.load %10[0] : memref<1xi1>
    scf.if %11 {
      affine.store %false, %10[0] : memref<1xi1>
      affine.store %cst, %0[0] : memref<1xf64>
    }
    return
  }
  func.func @cholesky_blocked(%arg0: i32, %arg1: memref<?x?xmemref<?xf64>>, %arg2: i32) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 0.000000e+00 : f64
    omp.parallel   {
      omp.barrier
      %alloca = memref.alloca() : memref<f64>
      %alloca_0 = memref.alloca() : memref<f64>
      %alloca_1 = memref.alloca() : memref<f64>
      %alloca_2 = memref.alloca() : memref<f64>
      %alloca_3 = memref.alloca() : memref<f64>
      %alloca_4 = memref.alloca() : memref<f64>
      %alloca_5 = memref.alloca() : memref<f64>
      %alloca_6 = memref.alloca() : memref<f64>
      omp.master {
        %alloca_7 = memref.alloca() : memref<1x2xi64>
        %0 = "polygeist.memref2pointer"(%arg1) : (memref<?x?xmemref<?xf64>>) -> !llvm.ptr
        %1 = "polygeist.pointer2memref"(%0) : (!llvm.ptr) -> memref<?x?xf64>
        %2 = arith.index_cast %arg2 : i32 to index
        scf.for %arg3 = %c0 to %2 step %c1 {
          %cast = memref.cast %alloca_7 : memref<1x2xi64> to memref<?x2xi64>
          %6 = func.call @clock_gettime(%c1_i32, %cast) : (i32, memref<?x2xi64>) -> i32
          %7 = memref.get_global @NB : memref<1xi32>
          %8 = scf.while (%arg4 = %c0_i32) : (i32) -> i32 {
            %9 = affine.load %7[0] : memref<1xi32>
            %10 = arith.cmpi slt, %arg4, %9 : i32
            scf.condition(%10) %arg4 : i32
          } do {
          ^bb0(%arg4: i32):
            %9 = arith.index_cast %arg4 : i32 to index
            %10 = memref.load %1[%9, %9] : memref<?x?xf64>
            affine.store %10, %alloca_2[] : memref<f64>
            omp.task   depend(taskdependinout -> %alloca_2 : memref<f64>) {
              %20 = memref.load %arg1[%9, %9] : memref<?x?xmemref<?xf64>>
              func.call @omp_potrf(%20, %arg0, %arg0, %1) : (memref<?xf64>, i32, i32, memref<?x?xf64>) -> ()
              omp.terminator
            }
            %11 = arith.addi %arg4, %c1_i32 : i32
            %12 = arith.index_cast %arg4 : i32 to index
            %13 = scf.while (%arg5 = %11) : (i32) -> i32 {
              %20 = affine.load %7[0] : memref<1xi32>
              %21 = arith.cmpi slt, %arg5, %20 : i32
              scf.condition(%21) %arg5 : i32
            } do {
            ^bb0(%arg5: i32):
              %20 = memref.load %1[%12, %12] : memref<?x?xf64>
              affine.store %20, %alloca_3[] : memref<f64>
              %21 = arith.index_cast %arg5 : i32 to index
              %22 = memref.load %1[%12, %21] : memref<?x?xf64>
              affine.store %22, %alloca_4[] : memref<f64>
              omp.task   depend(taskdependin -> %alloca_3 : memref<f64>, taskdependinout -> %alloca_4 : memref<f64>) {
                %24 = memref.load %arg1[%12, %12] : memref<?x?xmemref<?xf64>>
                %25 = memref.load %arg1[%12, %21] : memref<?x?xmemref<?xf64>>
                func.call @omp_trsm(%24, %25, %arg0, %arg0, %1) : (memref<?xf64>, memref<?xf64>, i32, i32, memref<?x?xf64>) -> ()
                omp.terminator
              }
              %23 = arith.addi %arg5, %c1_i32 : i32
              scf.yield %23 : i32
            }
            %14 = arith.addi %arg4, %c1_i32 : i32
            %15 = arith.addi %arg4, %c1_i32 : i32
            %16 = arith.index_cast %15 : i32 to index
            %17 = arith.index_cast %arg4 : i32 to index
            %18 = scf.while (%arg5 = %14) : (i32) -> i32 {
              %20 = affine.load %7[0] : memref<1xi32>
              %21 = arith.cmpi slt, %arg5, %20 : i32
              scf.condition(%21) %arg5 : i32
            } do {
            ^bb0(%arg5: i32):
              %20 = arith.index_cast %arg5 : i32 to index
              scf.for %arg6 = %16 to %20 step %c1 {
                %25 = arith.subi %arg6, %16 : index
                %26 = arith.index_cast %15 : i32 to index
                %27 = arith.addi %26, %25 : index
                %28 = arith.index_cast %arg4 : i32 to index
                %29 = arith.index_cast %arg5 : i32 to index
                %30 = memref.load %1[%28, %29] : memref<?x?xf64>
                affine.store %30, %alloca[] : memref<f64>
                %31 = memref.load %1[%28, %27] : memref<?x?xf64>
                affine.store %31, %alloca_0[] : memref<f64>
                %32 = memref.load %1[%27, %29] : memref<?x?xf64>
                affine.store %32, %alloca_1[] : memref<f64>
                omp.task   depend(taskdependin -> %alloca : memref<f64>, taskdependin -> %alloca_0 : memref<f64>, taskdependinout -> %alloca_1 : memref<f64>) {
                  %33 = memref.load %arg1[%28, %29] : memref<?x?xmemref<?xf64>>
                  %34 = memref.load %arg1[%28, %27] : memref<?x?xmemref<?xf64>>
                  %35 = memref.load %arg1[%27, %29] : memref<?x?xmemref<?xf64>>
                  func.call @omp_gemm(%33, %34, %35, %arg0, %arg0, %1) : (memref<?xf64>, memref<?xf64>, memref<?xf64>, i32, i32, memref<?x?xf64>) -> ()
                  omp.terminator
                }
              }
              %21 = arith.index_cast %arg5 : i32 to index
              %22 = memref.load %1[%17, %21] : memref<?x?xf64>
              affine.store %22, %alloca_5[] : memref<f64>
              %23 = memref.load %1[%21, %21] : memref<?x?xf64>
              affine.store %23, %alloca_6[] : memref<f64>
              omp.task   depend(taskdependin -> %alloca_5 : memref<f64>, taskdependinout -> %alloca_6 : memref<f64>) {
                %25 = memref.load %arg1[%17, %21] : memref<?x?xmemref<?xf64>>
                %26 = memref.load %arg1[%21, %21] : memref<?x?xmemref<?xf64>>
                func.call @omp_syrk(%25, %26, %arg0, %arg0, %1) : (memref<?xf64>, memref<?xf64>, i32, i32, memref<?x?xf64>) -> ()
                omp.terminator
              }
              %24 = arith.addi %arg5, %c1_i32 : i32
              scf.yield %24 : i32
            }
            %19 = arith.addi %arg4, %c1_i32 : i32
            scf.yield %19 : i32
          }
        }
        %3 = llvm.mlir.addressof @str0 : !llvm.ptr
        %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<14 x i8>
        %5 = llvm.call @printf(%4, %cst) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
        omp.terminator
      }
      omp.barrier
      omp.terminator
    }
    return
  }
  func.func private @clock_gettime(i32, memref<?x2xi64>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4194304 = arith.constant 4194304 : index
    %cst = arith.constant 0.000000e+00 : f64
    %c8 = arith.constant 8 : index
    %c0_i32 = arith.constant 0 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c2048_i32 = arith.constant 2048 : i32
    %c1_i32 = arith.constant 1 : i32
    %c3_i32 = arith.constant 3 : i32
    %0 = arith.cmpi ne, %arg0, %c3_i32 : i32
    scf.if %0 {
      %29 = llvm.mlir.addressof @stderr : !llvm.ptr
      %30 = llvm.load %29 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %31 = "polygeist.memref2pointer"(%30) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %32 = llvm.mlir.addressof @str1 : !llvm.ptr
      %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<41 x i8>
      %34 = llvm.call @fprintf(%31, %33) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
      func.call @exit(%c1_i32) : (i32) -> ()
    }
    %1 = memref.get_global @NB : memref<1xi32>
    %2 = affine.load %1[0] : memref<1xi32>
    %3 = arith.cmpi sgt, %2, %c2048_i32 : i32
    scf.if %3 {
      %29 = llvm.mlir.addressof @stderr : !llvm.ptr
      %30 = llvm.load %29 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %31 = "polygeist.memref2pointer"(%30) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %32 = llvm.mlir.addressof @str2 : !llvm.ptr
      %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<48 x i8>
      %34 = llvm.call @fprintf(%31, %33, %2, %c2048_i32) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, i32, i32) -> i32
      func.call @exit(%c-1_i32) : (i32) -> ()
    }
    %4 = affine.load %arg1[2] : memref<?xmemref<?xi8>>
    %5 = call @atoi(%4) : (memref<?xi8>) -> i32
    %6 = arith.cmpi slt, %5, %c0_i32 : i32
    scf.if %6 {
      %29 = llvm.mlir.addressof @stderr : !llvm.ptr
      %30 = llvm.load %29 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %31 = "polygeist.memref2pointer"(%30) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %32 = llvm.mlir.addressof @str3 : !llvm.ptr
      %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<27 x i8>
      %34 = llvm.call @fprintf(%31, %33) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
      func.call @exit(%c1_i32) : (i32) -> ()
    }
    %7 = memref.get_global @NB : memref<1xi32>
    %8 = affine.load %7[0] : memref<1xi32>
    %9 = arith.divsi %c2048_i32, %8 : i32
    %10 = llvm.mlir.addressof @str7 : !llvm.ptr
    %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %12 = llvm.call @printf(%11, %9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %13 = "polygeist.typeSize"() <{source = memref<?xmemref<?xf64>>}> : () -> index
    %14 = arith.index_cast %13 : index to i64
    %15 = memref.get_global @NB : memref<1xi32>
    %16 = affine.load %15[0] : memref<1xi32>
    %17 = arith.extsi %16 : i32 to i64
    %18 = arith.muli %14, %17 : i64
    %19 = arith.index_cast %18 : i64 to index
    %20 = arith.divui %19, %13 : index
    %alloc = memref.alloc(%20) : memref<?xmemref<?xmemref<?xf64>>>
    %21 = memref.get_global @NB : memref<1xi32>
    %22 = affine.load %21[0] : memref<1xi32>
    %23 = "polygeist.typeSize"() <{source = memref<?xf64>}> : () -> index
    %24 = arith.index_cast %23 : index to i64
    %25 = arith.index_cast %22 : i32 to index
    %26 = scf.for %arg2 = %c0 to %25 step %c1 iter_args(%arg3 = %c0_i32) -> (i32) {
      %29 = arith.index_cast %arg3 : i32 to index
      %30 = arith.extsi %22 : i32 to i64
      %31 = arith.muli %24, %30 : i64
      %32 = arith.index_cast %31 : i64 to index
      %33 = arith.divui %32, %23 : index
      %alloc_0 = memref.alloc(%33) : memref<?xmemref<?xf64>>
      memref.store %alloc_0, %alloc[%29] : memref<?xmemref<?xmemref<?xf64>>>
      %34 = arith.addi %arg3, %c1_i32 : i32
      scf.yield %34 : i32
    }
    %27 = memref.get_global @NB : memref<1xi32>
    %28 = scf.while (%arg2 = %c0_i32) : (i32) -> i32 {
      %29 = affine.load %27[0] : memref<1xi32>
      %30 = arith.cmpi slt, %arg2, %29 : i32
      scf.condition(%30) %arg2 : i32
    } do {
    ^bb0(%arg2: i32):
      %29 = arith.index_cast %arg2 : i32 to index
      %30:2 = scf.while (%arg3 = %c0_i32) : (i32) -> (i32, i32) {
        %32 = affine.load %27[0] : memref<1xi32>
        %33 = arith.cmpi slt, %arg3, %32 : i32
        scf.condition(%33) %32, %arg3 : i32, i32
      } do {
      ^bb0(%arg3: i32, %arg4: i32):
        %32 = memref.load %alloc[%29] : memref<?xmemref<?xmemref<?xf64>>>
        %33 = arith.index_cast %arg4 : i32 to index
        %34 = arith.divsi %c2048_i32, %arg3 : i32
        %35 = arith.muli %34, %34 : i32
        %36 = arith.index_cast %35 : i32 to index
        %37 = arith.muli %36, %c8 : index
        %38 = arith.divui %37, %c8 : index
        %alloc_0 = memref.alloc(%38) : memref<?xf64>
        scf.for %arg5 = %c0 to %38 step %c1 {
          memref.store %cst, %alloc_0[%arg5] : memref<?xf64>
        }
        memref.store %alloc_0, %32[%33] : memref<?xmemref<?xf64>>
        %39 = arith.addi %arg4, %c1_i32 : i32
        scf.yield %39 : i32
      }
      %31 = arith.addi %arg2, %c1_i32 : i32
      scf.yield %31 : i32
    }
    scf.for %arg2 = %c0 to %c4194304 step %c1 {
      %29 = memref.get_global @original_matrix : memref<4194304xf64>
      %30 = memref.get_global @matrix : memref<4194304xf64>
      %31 = memref.load %30[%arg2] : memref<4194304xf64>
      memref.store %31, %29[%arg2] : memref<4194304xf64>
    }
    return %c0_i32 : i32
  }
  func.func private @exit(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
