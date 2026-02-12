// RUN: %carts-run %s --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: %[[SGUID:.*]], %[[SPTR:.*]] = arts.db_alloc[<inout>, <heap>, <write>, <stencil>, <stencil>]
// CHECK: arts.db_acquire[<inout>] (%[[SGUID]] : memref<?xi64>, %[[SPTR]] : memref<?xmemref<?x?xf64>>) partitioning(<block>)
// CHECK: arts.db_acquire[<in>] (%[[SGUID]] : memref<?xi64>, %[[SPTR]] : memref<?xmemref<?x?xf64>>) partitioning(<stencil>)

#map = affine_map<()[s0] -> (s0 + 1)>
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  llvm.mlir.global internal constant @str1("e2e.%s: %.9fs\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  memref.global "private" @_carts_e2e_timer_start_ns : memref<1xi64> = dense<0>
  llvm.mlir.global internal constant @str0("jacobi-for\00") {addr_space = 0 : i32}
  memref.global "private" @_carts_e2e_timer_name : memref<1xmemref<?xi8>> = uninitialized
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 0.0039215686274509803 : f64
    %c256_i64 = arith.constant 256 : i64
    %cst_0 = arith.constant 1.000000e-09 : f64
    %cst_1 = arith.constant 0.000000e+00 : f64
    %c10_i32 = arith.constant 10 : i32
    %c0_i32 = arith.constant 0 : i32
    %c256_i32 = arith.constant 256 : i32
    call @carts_benchmarks_start() : () -> ()
    %0 = memref.get_global @_carts_e2e_timer_name : memref<1xmemref<?xi8>>
    %1 = llvm.mlir.addressof @str0 : !llvm.ptr
    %2 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    affine.store %2, %0[0] : memref<1xmemref<?xi8>>
    %3 = memref.get_global @_carts_e2e_timer_start_ns : memref<1xi64>
    %4 = call @carts_e2e_timer_get_time_ns() : () -> i64
    affine.store %4, %3[0] : memref<1xi64>
    %5 = polygeist.typeSize memref<?xf64> : index
    %6 = arith.index_cast %5 : index to i64
    %7 = arith.muli %6, %c256_i64 : i64
    %8 = arith.index_cast %7 : i64 to index
    %9 = arith.divui %8, %5 : index
    %alloc = memref.alloc(%9) : memref<?xmemref<?xf64>>
    %alloc_2 = memref.alloc(%9) : memref<?xmemref<?xf64>>
    %alloc_3 = memref.alloc(%9) : memref<?xmemref<?xf64>>
    affine.for %arg0 = 0 to 256 {
      %alloc_4 = memref.alloc() : memref<256xf64>
      %cast = memref.cast %alloc_4 : memref<256xf64> to memref<?xf64>
      affine.store %cast, %alloc[%arg0] : memref<?xmemref<?xf64>>
      %alloc_5 = memref.alloc() : memref<256xf64>
      %cast_6 = memref.cast %alloc_5 : memref<256xf64> to memref<?xf64>
      affine.store %cast_6, %alloc_2[%arg0] : memref<?xmemref<?xf64>>
      %alloc_7 = memref.alloc() : memref<256xf64>
      %cast_8 = memref.cast %alloc_7 : memref<256xf64> to memref<?xf64>
      affine.store %cast_8, %alloc_3[%arg0] : memref<?xmemref<?xf64>>
    }
    affine.for %arg0 = 0 to 256 {
      affine.for %arg1 = 0 to 256 {
        %21 = affine.load %alloc[%arg0] : memref<?xmemref<?xf64>>
        affine.store %cst_1, %21[%arg1] : memref<?xf64>
        %22 = affine.load %alloc_2[%arg0] : memref<?xmemref<?xf64>>
        affine.store %cst_1, %22[%arg1] : memref<?xf64>
        %23 = affine.load %alloc_3[%arg0] : memref<?xmemref<?xf64>>
        affine.store %cst_1, %23[%arg1] : memref<?xf64>
      }
    }
    call @sweep(%c256_i32, %c256_i32, %cst, %cst, %alloc, %c0_i32, %c10_i32, %alloc_2, %alloc_3, %c10_i32) : (i32, i32, f64, f64, memref<?xmemref<?xf64>>, i32, i32, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, i32) -> ()
    %10 = affine.for %arg0 = 0 to 256 iter_args(%arg1 = %cst_1) -> (f64) {
      %21 = affine.load %alloc_3[%arg0] : memref<?xmemref<?xf64>>
      %22 = affine.for %arg2 = 0 to 256 iter_args(%arg3 = %arg1) -> (f64) {
        %23 = affine.load %21[%arg2] : memref<?xf64>
        %24 = arith.addf %arg3, %23 : f64
        affine.yield %24 : f64
      }
      affine.yield %22 : f64
    }
    call @carts_bench_checksum_d(%10) : (f64) -> ()
    affine.for %arg0 = 0 to 256 {
      %21 = affine.load %alloc[%arg0] : memref<?xmemref<?xf64>>
      memref.dealloc %21 : memref<?xf64>
      %22 = affine.load %alloc_2[%arg0] : memref<?xmemref<?xf64>>
      memref.dealloc %22 : memref<?xf64>
      %23 = affine.load %alloc_3[%arg0] : memref<?xmemref<?xf64>>
      memref.dealloc %23 : memref<?xf64>
    }
    memref.dealloc %alloc : memref<?xmemref<?xf64>>
    memref.dealloc %alloc_2 : memref<?xmemref<?xf64>>
    memref.dealloc %alloc_3 : memref<?xmemref<?xf64>>
    %11 = call @carts_e2e_timer_get_time_ns() : () -> i64
    %12 = affine.load %3[0] : memref<1xi64>
    %13 = arith.subi %11, %12 : i64
    %14 = llvm.mlir.addressof @str1 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
    %16 = affine.load %0[0] : memref<1xmemref<?xi8>>
    %17 = polygeist.memref2pointer %16 : memref<?xi8> to !llvm.ptr
    %18 = arith.sitofp %13 : i64 to f64
    %19 = arith.mulf %18, %cst_0 : f64
    %20 = llvm.call @printf(%15, %17, %19) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, !llvm.ptr, f64) -> i32
    return %c0_i32 : i32
  }
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_get_time_ns() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @sweep(%arg0: i32, %arg1: i32, %arg2: f64, %arg3: f64, %arg4: memref<?xmemref<?xf64>>, %arg5: i32, %arg6: i32, %arg7: memref<?xmemref<?xf64>>, %arg8: memref<?xmemref<?xf64>>, %arg9: i32) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 2.500000e-01 : f64
    %true = arith.constant true
    %c-1_i32 = arith.constant -1 : i32
    %c1 = arith.constant 1 : index
    %c1_i32 = arith.constant 1 : i32
    %0 = arith.index_cast %arg6 : i32 to index
    %1 = arith.index_cast %arg5 : i32 to index
    %2 = arith.index_cast %arg0 : i32 to index
    %3 = arith.index_cast %arg1 : i32 to index
    %4 = arith.addi %arg0, %c-1_i32 : i32
    %5 = arith.addi %arg1, %c-1_i32 : i32
    affine.for %arg10 = #map()[%1] to #map()[%0] {
      omp.parallel   {
        omp.wsloop   for  (%arg11) : index = (%c0) to (%2) step (%c1) {
          affine.for %arg12 = 0 to %3 {
            %6 = memref.load %arg7[%arg11] : memref<?xmemref<?xf64>>
            %7 = memref.load %arg8[%arg11] : memref<?xmemref<?xf64>>
            %8 = affine.load %7[%arg12] : memref<?xf64>
            affine.store %8, %6[%arg12] : memref<?xf64>
          }
          omp.yield
        }
        omp.terminator
      }
      omp.parallel   {
        omp.wsloop   for  (%arg11) : index = (%c0) to (%2) step (%c1) {
          %6 = arith.index_cast %arg11 : index to i32
          %7 = arith.cmpi eq, %6, %c0_i32 : i32
          %8 = arith.cmpi eq, %6, %4 : i32
          %9 = arith.addi %6, %c-1_i32 : i32
          %10 = arith.index_cast %9 : i32 to index
          %11 = arith.addi %6, %c1_i32 : i32
          %12 = arith.index_cast %11 : i32 to index
          affine.for %arg12 = 0 to %3 {
            %13 = arith.index_cast %arg12 : index to i32
            %14 = scf.if %7 -> (i1) {
              scf.yield %true : i1
            } else {
              %19 = arith.cmpi eq, %13, %c0_i32 : i32
              scf.yield %19 : i1
            }
            %15 = arith.xori %14, %true : i1
            %16 = arith.andi %15, %8 : i1
            %17 = arith.ori %14, %16 : i1
            %18 = scf.if %17 -> (i1) {
              scf.yield %true : i1
            } else {
              %19 = arith.cmpi eq, %13, %5 : i32
              scf.yield %19 : i1
            }
            scf.if %18 {
              %19 = memref.load %arg8[%arg11] : memref<?xmemref<?xf64>>
              %20 = memref.load %arg4[%arg11] : memref<?xmemref<?xf64>>
              %21 = affine.load %20[%arg12] : memref<?xf64>
              affine.store %21, %19[%arg12] : memref<?xf64>
            } else {
              %19 = memref.load %arg8[%arg11] : memref<?xmemref<?xf64>>
              %20 = memref.load %arg7[%10] : memref<?xmemref<?xf64>>
              %21 = affine.load %20[%arg12] : memref<?xf64>
              %22 = memref.load %arg7[%arg11] : memref<?xmemref<?xf64>>
              %23 = affine.load %22[%arg12 + 1] : memref<?xf64>
              %24 = arith.addf %21, %23 : f64
              %25 = affine.load %22[%arg12 - 1] : memref<?xf64>
              %26 = arith.addf %24, %25 : f64
              %27 = memref.load %arg7[%12] : memref<?xmemref<?xf64>>
              %28 = affine.load %27[%arg12] : memref<?xf64>
              %29 = arith.addf %26, %28 : f64
              %30 = memref.load %arg4[%arg11] : memref<?xmemref<?xf64>>
              %31 = affine.load %30[%arg12] : memref<?xf64>
              %32 = arith.mulf %31, %arg2 : f64
              %33 = arith.mulf %32, %arg3 : f64
              %34 = arith.addf %29, %33 : f64
              %35 = arith.mulf %34, %cst : f64
              affine.store %35, %19[%arg12] : memref<?xf64>
            }
          }
          omp.yield
        }
        omp.terminator
      }
    }
    return
  }
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>}
}
