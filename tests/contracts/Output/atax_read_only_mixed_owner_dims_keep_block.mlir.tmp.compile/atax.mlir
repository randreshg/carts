#loc5 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":76:5)
#loc34 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":92:3)
#loc42 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":102:3)
#loc51 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":111:3)
#loc58 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":119:3)
#loc70 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":29:13)
#loc72 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":32:3)
#loc78 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":34:3)
#loc81 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":35:5)
#loc90 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":54:13)
#loc98 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":62:5)
#loc108 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":70:5)
module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32} loc(#loc1)
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32} loc(#loc2)
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32} loc(#loc3)
  llvm.mlir.global internal constant @str0("atax\00") {addr_space = 0 : i32} loc(#loc4)
  func.func @main(%arg0: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":76:5), %arg1: memref<?xmemref<?xi8>> loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":76:5)) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr loc(#loc)
    %cst = arith.constant 0.000000e+00 : f64 loc(#loc)
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr loc(#loc)
    %c4000_i32 = arith.constant 4000 : i32 loc(#loc)
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr loc(#loc)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr loc(#loc)
    %true = arith.constant true loc(#loc6)
    %4 = "polygeist.undef"() : () -> i32 loc(#loc7)
    %alloca = memref.alloca() : memref<1xf64> loc(#loc8)
    %5 = "polygeist.undef"() : () -> f64 loc(#loc8)
    affine.store %5, %alloca[0] : memref<1xf64> loc(#loc8)
    %alloca_0 = memref.alloca() : memref<1xmemref<?xf64>> loc(#loc9)
    %alloca_1 = memref.alloca() : memref<1xmemref<?xf64>> loc(#loc9)
    %alloca_2 = memref.alloca() : memref<1xmemref<?xf64>> loc(#loc9)
    %alloca_3 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
    scf.if %true {
      scf.execute_region {
        func.call @carts_benchmarks_start() : () -> () loc(#loc10)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc11)
        func.call @carts_e2e_timer_start(%9) : (memref<?xi8>) -> () loc(#loc11)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc12)
        %10 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc12)
        func.call @carts_phase_timer_start(%9, %10) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc12)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %6 = scf.if %true -> (i32) {
      %9 = scf.execute_region -> i32 {
        %10 = scf.if %true -> (i32) {
          %11 = scf.execute_region -> i32 {
            scf.yield %c4000_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %11 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %10 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %9 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    %7 = arith.index_cast %6 : i32 to index loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %9 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %10 = polygeist.typeSize memref<?xf64> : index loc(#loc13)
            %11 = arith.extsi %6 : i32 to i64 loc(#loc14)
            %12 = arith.index_cast %10 : index to i64 loc(#loc15)
            %13 = arith.muli %11, %12 : i64 loc(#loc16)
            %14 = arith.index_cast %13 : i64 to index loc(#loc13)
            %15 = arith.divui %14, %10 : index loc(#loc13)
            %alloc = memref.alloc(%15) : memref<?xmemref<?xf64>> loc(#loc13)
            affine.store %alloc, %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
            scf.yield %alloc : memref<?xmemref<?xf64>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %9 = scf.execute_region -> memref<?xf64> {
            %10 = polygeist.typeSize f64 : index loc(#loc17)
            %11 = arith.extsi %6 : i32 to i64 loc(#loc18)
            %12 = arith.index_cast %10 : index to i64 loc(#loc19)
            %13 = arith.muli %11, %12 : i64 loc(#loc20)
            %14 = arith.index_cast %13 : i64 to index loc(#loc17)
            %15 = arith.divui %14, %10 : index loc(#loc17)
            %alloc = memref.alloc(%15) : memref<?xf64> loc(#loc17)
            affine.store %alloc, %alloca_2[0] : memref<1xmemref<?xf64>> loc(#loc9)
            scf.yield %alloc : memref<?xf64> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %9 = scf.execute_region -> memref<?xf64> {
            %10 = polygeist.typeSize f64 : index loc(#loc21)
            %11 = arith.extsi %6 : i32 to i64 loc(#loc22)
            %12 = arith.index_cast %10 : index to i64 loc(#loc23)
            %13 = arith.muli %11, %12 : i64 loc(#loc24)
            %14 = arith.index_cast %13 : i64 to index loc(#loc21)
            %15 = arith.divui %14, %10 : index loc(#loc21)
            %alloc = memref.alloc(%15) : memref<?xf64> loc(#loc21)
            affine.store %alloc, %alloca_1[0] : memref<1xmemref<?xf64>> loc(#loc9)
            scf.yield %alloc : memref<?xf64> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %9 = scf.execute_region -> memref<?xf64> {
            %10 = polygeist.typeSize f64 : index loc(#loc25)
            %11 = arith.extsi %6 : i32 to i64 loc(#loc26)
            %12 = arith.index_cast %10 : index to i64 loc(#loc27)
            %13 = arith.muli %11, %12 : i64 loc(#loc28)
            %14 = arith.index_cast %13 : i64 to index loc(#loc25)
            %15 = arith.divui %14, %10 : index loc(#loc25)
            %alloc = memref.alloc(%15) : memref<?xf64> loc(#loc25)
            affine.store %alloc, %alloca_0[0] : memref<1xmemref<?xf64>> loc(#loc9)
            scf.yield %alloc : memref<?xf64> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %9 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc29)
            %10 = polygeist.typeSize f64 : index loc(#loc30)
            %11 = arith.extsi %6 : i32 to i64 loc(#loc31)
            %12 = arith.index_cast %10 : index to i64 loc(#loc32)
            %13 = arith.muli %11, %12 : i64 loc(#loc33)
            %14 = arith.index_cast %13 : i64 to index loc(#loc30)
            %15 = arith.divui %14, %10 : index loc(#loc30)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":92:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %16 = polygeist.subindex %9[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc29)
                  %alloc = memref.alloc(%15) : memref<?xf64> loc(#loc30)
                  affine.store %alloc, %16[0] : memref<?xmemref<?xf64>> loc(#loc35)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc34)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc36)
        %10 = affine.load %alloca_2[0] : memref<1xmemref<?xf64>> loc(#loc37)
        func.call @init_array(%6, %6, %9, %10) : (i32, i32, memref<?xmemref<?xf64>>, memref<?xf64>) -> () loc(#loc38)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc39)
        func.call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> () loc(#loc39)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc40)
        func.call @carts_kernel_timer_start(%9) : (memref<?xi8>) -> () loc(#loc40)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %9 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc41)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":102:3) = 0 to 1 {
              scf.if %true {
                scf.execute_region {
                  %10 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc43)
                  %11 = affine.load %alloca_2[0] : memref<1xmemref<?xf64>> loc(#loc44)
                  %12 = affine.load %alloca_1[0] : memref<1xmemref<?xf64>> loc(#loc45)
                  %13 = affine.load %alloca_0[0] : memref<1xmemref<?xf64>> loc(#loc46)
                  func.call @kernel_atax(%6, %6, %10, %11, %12, %13) : (i32, i32, memref<?xmemref<?xf64>>, memref<?xf64>, memref<?xf64>, memref<?xf64>) -> () loc(#loc47)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  func.call @carts_kernel_timer_accum(%9) : (memref<?xi8>) -> () loc(#loc41)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc42)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc48)
        func.call @carts_kernel_timer_print(%9) : (memref<?xi8>) -> () loc(#loc48)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc49)
        %10 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc49)
        func.call @carts_phase_timer_start(%9, %10) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc49)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.store %cst, %alloca[0] : memref<1xf64> loc(#loc8)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %9 = affine.load %alloca_1[0] : memref<1xmemref<?xf64>> loc(#loc50)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":111:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %10 = polygeist.subindex %9[%arg2] () : memref<?xf64> -> memref<?xf64> loc(#loc50)
                  %11 = affine.load %10[0] : memref<?xf64> loc(#loc50)
                  %12 = affine.load %alloca[0] : memref<1xf64> loc(#loc52)
                  %13 = arith.addf %12, %11 : f64 loc(#loc52)
                  affine.store %13, %alloca[0] : memref<1xf64> loc(#loc52)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc51)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = affine.load %alloca[0] : memref<1xf64> loc(#loc53)
        func.call @carts_bench_checksum_d(%9) : (f64) -> () loc(#loc54)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc55)
        func.call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> () loc(#loc55)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc56)
        %10 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc56)
        func.call @carts_phase_timer_start(%9, %10) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc56)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %9 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc57)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":119:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %10 = polygeist.subindex %9[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc57)
                  %11 = affine.load %10[0] : memref<?xmemref<?xf64>> loc(#loc59)
                  memref.dealloc %11 : memref<?xf64> loc(#loc59)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc58)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc60)
        memref.dealloc %9 : memref<?xmemref<?xf64>> loc(#loc60)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = affine.load %alloca_2[0] : memref<1xmemref<?xf64>> loc(#loc61)
        memref.dealloc %9 : memref<?xf64> loc(#loc61)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = affine.load %alloca_1[0] : memref<1xmemref<?xf64>> loc(#loc62)
        memref.dealloc %9 : memref<?xf64> loc(#loc62)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = affine.load %alloca_0[0] : memref<1xmemref<?xf64>> loc(#loc63)
        memref.dealloc %9 : memref<?xf64> loc(#loc63)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %9 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc64)
        func.call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> () loc(#loc64)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @carts_e2e_timer_stop() : () -> () loc(#loc65)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %8 = scf.if %true -> (i32) {
      %9 = scf.execute_region -> i32 {
        %10 = scf.if %true -> (i32) {
          %11 = scf.execute_region -> i32 {
            scf.yield %c0_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %11 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %10 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %9 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    return %8 : i32 loc(#loc66)
  } loc(#loc5)
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc67)
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc68)
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc69)
  func.func private @init_array(%arg0: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":29:13), %arg1: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":29:13), %arg2: memref<?xmemref<?xf64>> loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":29:13), %arg3: memref<?xf64> loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":29:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %true = arith.constant true loc(#loc71)
    %cst = arith.constant 3.1415926535897931 : f64 loc(#loc)
    %c1_i32 = arith.constant 1 : i32 loc(#loc)
    %0 = arith.index_cast %arg0 : i32 to index loc(#loc70)
    %1 = arith.index_cast %arg1 : i32 to index loc(#loc70)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.for %arg4 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":32:3) = 0 to %1 {
              %2 = arith.index_cast %arg4 : index to i32 loc(#loc72)
              %3 = polygeist.subindex %arg3[%arg4] () : memref<?xf64> -> memref<?xf64> loc(#loc73)
              %4 = arith.sitofp %2 : i32 to f64 loc(#loc74)
              %5 = arith.mulf %4, %cst : f64 loc(#loc75)
              affine.store %5, %3[0] : memref<?xf64> loc(#loc76)
            } loc(#loc72)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %2 = arith.sitofp %arg0 : i32 to f64 loc(#loc77)
            affine.for %arg4 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":34:3) = 0 to %0 {
              %3 = arith.index_cast %arg4 : index to i32 loc(#loc78)
              scf.if %true {
                scf.execute_region {
                  %4 = polygeist.subindex %arg2[%arg4] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc79)
                  %5 = arith.sitofp %3 : i32 to f64 loc(#loc80)
                  affine.for %arg5 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":35:5) = 0 to %1 {
                    %6 = arith.index_cast %arg5 : index to i32 loc(#loc81)
                    %7 = affine.load %4[0] : memref<?xmemref<?xf64>> loc(#loc79)
                    %8 = polygeist.subindex %7[%arg5] () : memref<?xf64> -> memref<?xf64> loc(#loc79)
                    %9 = arith.addi %6, %c1_i32 : i32 loc(#loc82)
                    %10 = arith.sitofp %9 : i32 to f64 loc(#loc83)
                    %11 = arith.mulf %5, %10 : f64 loc(#loc84)
                    %12 = arith.divf %11, %2 : f64 loc(#loc85)
                    affine.store %12, %8[0] : memref<?xf64> loc(#loc86)
                  } loc(#loc81)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc78)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc87)
  } loc(#loc70)
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc88)
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc89)
  func.func private @kernel_atax(%arg0: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":54:13), %arg1: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":54:13), %arg2: memref<?xmemref<?xf64>> loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":54:13), %arg3: memref<?xf64> loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":54:13), %arg4: memref<?xf64> loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":54:13), %arg5: memref<?xf64> loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":54:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %cst = arith.constant 0.000000e+00 : f64 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %true = arith.constant true loc(#loc91)
    %0 = arith.index_cast %arg1 : i32 to index loc(#loc90)
    %1 = arith.index_cast %arg0 : i32 to index loc(#loc90)
    %2 = "polygeist.undef"() : () -> i32 loc(#loc92)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %3 = scf.if %true -> (i32) {
              %5 = scf.execute_region -> i32 {
                scf.yield %arg0 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %5 : i32 loc(#loc)
            } else {
              scf.yield %2 : i32 loc(#loc)
            } loc(#loc)
            %4 = arith.index_cast %3 : i32 to index loc(#loc93)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg6) : index = (%c0) to (%4) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          %5 = polygeist.subindex %arg5[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc94)
                          affine.store %cst, %5[0] : memref<?xf64> loc(#loc95)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %5 = polygeist.subindex %arg5[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc96)
                              %6 = polygeist.subindex %arg2[%arg6] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc97)
                              affine.for %arg7 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":62:5) = 0 to %0 {
                                scf.execute_region {
                                  %7 = affine.load %5[0] : memref<?xf64> loc(#loc99)
                                  %8 = affine.load %6[0] : memref<?xmemref<?xf64>> loc(#loc97)
                                  %9 = polygeist.subindex %8[%arg7] () : memref<?xf64> -> memref<?xf64> loc(#loc97)
                                  %10 = affine.load %9[0] : memref<?xf64> loc(#loc97)
                                  %11 = polygeist.subindex %arg3[%arg7] () : memref<?xf64> -> memref<?xf64> loc(#loc100)
                                  %12 = affine.load %11[0] : memref<?xf64> loc(#loc100)
                                  %13 = arith.mulf %10, %12 : f64 loc(#loc101)
                                  %14 = arith.addf %7, %13 : f64 loc(#loc102)
                                  affine.store %14, %5[0] : memref<?xf64> loc(#loc103)
                                  scf.yield loc(#loc98)
                                } loc(#loc98)
                              } loc(#loc98)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc93)
                    } loc(#loc93)
                    omp.yield loc(#loc93)
                  } loc(#loc93)
                } loc(#loc93)
                scf.yield loc(#loc93)
              } loc(#loc93)
              omp.terminator loc(#loc93)
            } loc(#loc93)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %3 = scf.if %true -> (i32) {
              %5 = scf.execute_region -> i32 {
                scf.yield %arg1 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %5 : i32 loc(#loc)
            } else {
              scf.yield %2 : i32 loc(#loc)
            } loc(#loc)
            %4 = arith.index_cast %3 : i32 to index loc(#loc104)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg6) : index = (%c0) to (%4) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          %5 = polygeist.subindex %arg4[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc105)
                          affine.store %cst, %5[0] : memref<?xf64> loc(#loc106)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %5 = polygeist.subindex %arg4[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc107)
                              affine.for %arg7 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":70:5) = 0 to %1 {
                                scf.execute_region {
                                  %6 = affine.load %5[0] : memref<?xf64> loc(#loc109)
                                  %7 = polygeist.subindex %arg2[%arg7] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc110)
                                  %8 = affine.load %7[0] : memref<?xmemref<?xf64>> loc(#loc110)
                                  %9 = polygeist.subindex %8[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc110)
                                  %10 = affine.load %9[0] : memref<?xf64> loc(#loc110)
                                  %11 = polygeist.subindex %arg5[%arg7] () : memref<?xf64> -> memref<?xf64> loc(#loc111)
                                  %12 = affine.load %11[0] : memref<?xf64> loc(#loc111)
                                  %13 = arith.mulf %10, %12 : f64 loc(#loc112)
                                  %14 = arith.addf %6, %13 : f64 loc(#loc113)
                                  affine.store %14, %5[0] : memref<?xf64> loc(#loc114)
                                  scf.yield loc(#loc108)
                                } loc(#loc108)
                              } loc(#loc108)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc104)
                    } loc(#loc104)
                    omp.yield loc(#loc104)
                  } loc(#loc104)
                } loc(#loc104)
                scf.yield loc(#loc104)
              } loc(#loc104)
              omp.terminator loc(#loc104)
            } loc(#loc104)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc115)
  } loc(#loc90)
  func.func private @carts_kernel_timer_accum(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc116)
  func.func private @carts_kernel_timer_print(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc117)
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc118)
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc119)
} loc(#loc)
#loc = loc(unknown)
#loc1 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:65)
#loc2 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:27)
#loc3 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:65)
#loc4 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":79:25)
#loc6 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":76:1)
#loc7 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":119:8)
#loc8 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":110:3)
#loc9 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.h":49:21)
#loc10 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":54:34)
#loc11 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":79:37)
#loc12 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:41)
#loc13 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":87:19)
#loc14 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":87:40)
#loc15 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":87:45)
#loc16 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":87:43)
#loc17 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":88:18)
#loc18 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":88:38)
#loc19 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":88:43)
#loc20 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":88:41)
#loc21 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":89:18)
#loc22 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":89:38)
#loc23 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":89:43)
#loc24 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":89:41)
#loc25 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":90:20)
#loc26 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":90:40)
#loc27 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":90:45)
#loc28 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":90:43)
#loc29 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":93:5)
#loc30 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":93:12)
#loc31 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":93:32)
#loc32 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":93:37)
#loc33 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":93:35)
#loc35 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":93:10)
#loc36 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":97:22)
#loc37 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":97:25)
#loc38 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":97:3)
#loc39 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":96:36)
#loc40 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":139:40)
#loc41 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":141:40)
#loc43 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":103:25)
#loc44 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":103:28)
#loc45 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":103:31)
#loc46 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":103:34)
#loc47 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":103:5)
#loc48 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":142:40)
#loc49 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:3)
#loc50 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":112:17)
#loc52 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":112:14)
#loc53 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":114:24)
#loc54 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":251:3)
#loc55 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":99:41)
#loc56 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:41)
#loc57 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":120:10)
#loc59 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":120:5)
#loc60 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":122:3)
#loc61 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":123:3)
#loc62 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":124:3)
#loc63 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":125:3)
#loc64 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":101:36)
#loc65 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":80:32)
#loc66 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":131:1)
#loc67 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":50:6)
#loc68 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":74:6)
#loc69 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":90:6)
#loc71 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":29:1)
#loc73 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":33:5)
#loc74 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":33:12)
#loc75 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":33:14)
#loc76 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":33:10)
#loc77 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":36:44)
#loc79 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":36:7)
#loc80 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":36:18)
#loc82 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":36:36)
#loc83 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":36:33)
#loc84 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":36:31)
#loc85 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":36:42)
#loc86 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":36:15)
#loc87 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":37:1)
#loc88 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":93:6)
#loc89 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":128:6)
#loc91 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":54:1)
#loc92 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":68:3)
#loc93 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":59:1)
#loc94 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":61:5)
#loc95 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":61:12)
#loc96 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":63:7)
#loc97 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":63:25)
#loc99 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":63:16)
#loc100 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":63:35)
#loc101 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":63:33)
#loc102 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":63:23)
#loc103 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":63:14)
#loc104 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":67:1)
#loc105 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":69:5)
#loc106 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":69:10)
#loc107 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":71:7)
#loc109 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":71:14)
#loc110 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":71:21)
#loc111 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":71:31)
#loc112 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":71:29)
#loc113 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":71:19)
#loc114 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":71:12)
#loc115 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/atax/atax.c":74:1)
#loc116 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":134:6)
#loc117 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":137:6)
#loc118 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":207:6)
#loc119 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":77:6)
