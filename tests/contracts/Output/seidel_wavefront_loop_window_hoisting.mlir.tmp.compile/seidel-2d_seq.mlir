#loc5 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":20:5)
#loc22 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":31:3)
#loc26 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":36:3)
#loc28 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":37:5)
#loc40 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":46:3)
#loc48 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":49:7)
#loc73 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":63:3)
#loc80 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":71:3)
#map = affine_map<()[s0] -> (s0 - 1)>
module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32} loc(#loc1)
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32} loc(#loc2)
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32} loc(#loc3)
  llvm.mlir.global internal constant @str0("seidel-2d\00") {addr_space = 0 : i32} loc(#loc4)
  func.func @main(%arg0: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":20:5), %arg1: memref<?xmemref<?xi8>> loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":20:5)) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32 loc(#loc)
    %cst = arith.constant 2.000000e+00 : f64 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr loc(#loc)
    %cst_0 = arith.constant 0.000000e+00 : f64 loc(#loc)
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr loc(#loc)
    %cst_1 = arith.constant 9.000000e+00 : f64 loc(#loc)
    %c1_i32 = arith.constant 1 : i32 loc(#loc)
    %c2_i32 = arith.constant 2 : i32 loc(#loc)
    %c320_i32 = arith.constant 320 : i32 loc(#loc)
    %c9600_i32 = arith.constant 9600 : i32 loc(#loc)
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr loc(#loc)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr loc(#loc)
    %true = arith.constant true loc(#loc6)
    %4 = "polygeist.undef"() : () -> i32 loc(#loc7)
    %alloca = memref.alloca() : memref<1xf64> loc(#loc8)
    %5 = "polygeist.undef"() : () -> f64 loc(#loc8)
    affine.store %5, %alloca[0] : memref<1xf64> loc(#loc8)
    %alloca_2 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
    scf.if %true {
      scf.execute_region {
        func.call @carts_benchmarks_start() : () -> () loc(#loc10)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc11)
        func.call @carts_e2e_timer_start(%12) : (memref<?xi8>) -> () loc(#loc11)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc12)
        %13 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc12)
        func.call @carts_phase_timer_start(%12, %13) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc12)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %6 = scf.if %true -> (i32) {
      %12 = scf.execute_region -> i32 {
        %13 = scf.if %true -> (i32) {
          %14 = scf.execute_region -> i32 {
            scf.yield %c9600_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %14 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %13 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %12 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    %7 = arith.index_cast %6 : i32 to index loc(#loc)
    %8 = arith.index_cast %6 : i32 to index loc(#loc)
    %9 = scf.if %true -> (i32) {
      %12 = scf.execute_region -> i32 {
        %13 = scf.if %true -> (i32) {
          %14 = scf.execute_region -> i32 {
            scf.yield %c320_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %14 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %13 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %12 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    %10 = arith.index_cast %9 : i32 to index loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %13 = polygeist.typeSize memref<?xf64> : index loc(#loc13)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc14)
            %15 = arith.index_cast %13 : index to i64 loc(#loc15)
            %16 = arith.muli %14, %15 : i64 loc(#loc16)
            %17 = arith.index_cast %16 : i64 to index loc(#loc13)
            %18 = arith.divui %17, %13 : index loc(#loc13)
            %alloc = memref.alloc(%18) : memref<?xmemref<?xf64>> loc(#loc13)
            affine.store %alloc, %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
            scf.yield %alloc : memref<?xmemref<?xf64>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %12 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc17)
            %13 = polygeist.typeSize f64 : index loc(#loc18)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc19)
            %15 = arith.index_cast %13 : index to i64 loc(#loc20)
            %16 = arith.muli %14, %15 : i64 loc(#loc21)
            %17 = arith.index_cast %16 : i64 to index loc(#loc18)
            %18 = arith.divui %17, %13 : index loc(#loc18)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":31:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %19 = polygeist.subindex %12[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc17)
                  %alloc = memref.alloc(%18) : memref<?xf64> loc(#loc18)
                  affine.store %alloc, %19[0] : memref<?xmemref<?xf64>> loc(#loc23)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc22)
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
            %12 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc24)
            %13 = arith.sitofp %6 : i32 to f64 loc(#loc25)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":36:3) = 0 to %7 {
              %14 = arith.index_cast %arg2 : index to i32 loc(#loc26)
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %15 = polygeist.subindex %12[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc24)
                      %16 = arith.sitofp %14 : i32 to f64 loc(#loc27)
                      affine.for %arg3 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":37:5) = 0 to %7 {
                        %17 = arith.index_cast %arg3 : index to i32 loc(#loc28)
                        scf.if %true {
                          scf.execute_region {
                            %18 = affine.load %15[0] : memref<?xmemref<?xf64>> loc(#loc24)
                            %19 = polygeist.subindex %18[%arg3] () : memref<?xf64> -> memref<?xf64> loc(#loc24)
                            %20 = arith.addi %17, %c2_i32 : i32 loc(#loc29)
                            %21 = arith.sitofp %20 : i32 to f64 loc(#loc30)
                            %22 = arith.mulf %16, %21 : f64 loc(#loc31)
                            %23 = arith.addf %22, %cst : f64 loc(#loc32)
                            %24 = arith.divf %23, %13 : f64 loc(#loc33)
                            affine.store %24, %19[0] : memref<?xf64> loc(#loc34)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc28)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc26)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc35)
        func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc35)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc36)
        func.call @carts_kernel_timer_start(%12) : (memref<?xi8>) -> () loc(#loc36)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %12 = arith.addi %6, %c-1_i32 : i32 loc(#loc37)
            %13 = arith.index_cast %12 : i32 to index loc(#loc38)
            %14 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc39)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":46:3) = 0 to %10 {
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      scf.for %arg3 = %c1 to %13 step %c1 {
                        %15 = arith.index_cast %arg3 : index to i32 loc(#loc41)
                        scf.if %true {
                          scf.execute_region {
                            scf.if %true {
                              scf.execute_region {
                                %16 = polygeist.subindex %14[%arg3] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc39)
                                %17 = arith.addi %15, %c-1_i32 : i32 loc(#loc42)
                                %18 = arith.index_cast %17 : i32 to index loc(#loc43)
                                %19 = polygeist.subindex %14[%18] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc44)
                                %20 = arith.addi %15, %c1_i32 : i32 loc(#loc45)
                                %21 = arith.index_cast %20 : i32 to index loc(#loc46)
                                %22 = polygeist.subindex %14[%21] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc47)
                                affine.for %arg4 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":49:7) = 1 to #map()[%7] {
                                  %23 = arith.index_cast %arg4 : index to i32 loc(#loc48)
                                  scf.if %true {
                                    scf.execute_region {
                                      %24 = affine.load %16[0] : memref<?xmemref<?xf64>> loc(#loc39)
                                      %25 = polygeist.subindex %24[%arg4] () : memref<?xf64> -> memref<?xf64> loc(#loc39)
                                      %26 = affine.load %19[0] : memref<?xmemref<?xf64>> loc(#loc44)
                                      %27 = arith.addi %23, %c-1_i32 : i32 loc(#loc49)
                                      %28 = arith.index_cast %27 : i32 to index loc(#loc50)
                                      %29 = polygeist.subindex %26[%28] () : memref<?xf64> -> memref<?xf64> loc(#loc44)
                                      %30 = affine.load %29[0] : memref<?xf64> loc(#loc44)
                                      %31 = polygeist.subindex %26[%arg4] () : memref<?xf64> -> memref<?xf64> loc(#loc51)
                                      %32 = affine.load %31[0] : memref<?xf64> loc(#loc51)
                                      %33 = arith.addf %30, %32 : f64 loc(#loc52)
                                      %34 = arith.addi %23, %c1_i32 : i32 loc(#loc53)
                                      %35 = arith.index_cast %34 : i32 to index loc(#loc54)
                                      %36 = polygeist.subindex %26[%35] () : memref<?xf64> -> memref<?xf64> loc(#loc55)
                                      %37 = affine.load %36[0] : memref<?xf64> loc(#loc55)
                                      %38 = arith.addf %33, %37 : f64 loc(#loc56)
                                      %39 = polygeist.subindex %24[%28] () : memref<?xf64> -> memref<?xf64> loc(#loc57)
                                      %40 = affine.load %39[0] : memref<?xf64> loc(#loc57)
                                      %41 = arith.addf %38, %40 : f64 loc(#loc58)
                                      %42 = affine.load %25[0] : memref<?xf64> loc(#loc59)
                                      %43 = arith.addf %41, %42 : f64 loc(#loc60)
                                      %44 = polygeist.subindex %24[%35] () : memref<?xf64> -> memref<?xf64> loc(#loc61)
                                      %45 = affine.load %44[0] : memref<?xf64> loc(#loc61)
                                      %46 = arith.addf %43, %45 : f64 loc(#loc62)
                                      %47 = affine.load %22[0] : memref<?xmemref<?xf64>> loc(#loc47)
                                      %48 = polygeist.subindex %47[%28] () : memref<?xf64> -> memref<?xf64> loc(#loc47)
                                      %49 = affine.load %48[0] : memref<?xf64> loc(#loc47)
                                      %50 = arith.addf %46, %49 : f64 loc(#loc63)
                                      %51 = polygeist.subindex %47[%arg4] () : memref<?xf64> -> memref<?xf64> loc(#loc64)
                                      %52 = affine.load %51[0] : memref<?xf64> loc(#loc64)
                                      %53 = arith.addf %50, %52 : f64 loc(#loc65)
                                      %54 = polygeist.subindex %47[%35] () : memref<?xf64> -> memref<?xf64> loc(#loc66)
                                      %55 = affine.load %54[0] : memref<?xf64> loc(#loc66)
                                      %56 = arith.addf %53, %55 : f64 loc(#loc67)
                                      %57 = arith.divf %56, %cst_1 : f64 loc(#loc68)
                                      affine.store %57, %25[0] : memref<?xf64> loc(#loc69)
                                      scf.yield loc(#loc)
                                    } loc(#loc)
                                  } loc(#loc)
                                } loc(#loc48)
                                scf.yield loc(#loc)
                              } loc(#loc)
                            } loc(#loc)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc41)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc40)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc70)
        func.call @carts_kernel_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc70)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc71)
        %13 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc71)
        func.call @carts_phase_timer_start(%12, %13) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc71)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.store %cst_0, %alloca[0] : memref<1xf64> loc(#loc8)
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
            %12 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc72)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":63:3) = 0 to %8 {
              scf.if %true {
                scf.execute_region {
                  %13 = polygeist.subindex %12[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc72)
                  %14 = affine.load %13[0] : memref<?xmemref<?xf64>> loc(#loc72)
                  %15 = polygeist.subindex %14[%arg2] () : memref<?xf64> -> memref<?xf64> loc(#loc72)
                  %16 = affine.load %15[0] : memref<?xf64> loc(#loc72)
                  %17 = affine.load %alloca[0] : memref<1xf64> loc(#loc74)
                  %18 = arith.addf %17, %16 : f64 loc(#loc74)
                  affine.store %18, %alloca[0] : memref<1xf64> loc(#loc74)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc73)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca[0] : memref<1xf64> loc(#loc75)
        func.call @carts_bench_checksum_d(%12) : (f64) -> () loc(#loc76)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc77)
        func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc77)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc78)
        %13 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc78)
        func.call @carts_phase_timer_start(%12, %13) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc78)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %12 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc79)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":71:3) = 0 to %8 {
              scf.if %true {
                scf.execute_region {
                  %13 = polygeist.subindex %12[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc79)
                  %14 = affine.load %13[0] : memref<?xmemref<?xf64>> loc(#loc81)
                  memref.dealloc %14 : memref<?xf64> loc(#loc81)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc80)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc82)
        memref.dealloc %12 : memref<?xmemref<?xf64>> loc(#loc82)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc83)
        func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc83)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @carts_e2e_timer_stop() : () -> () loc(#loc84)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %11 = scf.if %true -> (i32) {
      %12 = scf.execute_region -> i32 {
        %13 = scf.if %true -> (i32) {
          %14 = scf.execute_region -> i32 {
            scf.yield %c0_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %14 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %13 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %12 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    return %11 : i32 loc(#loc85)
  } loc(#loc5)
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc86)
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc87)
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc88)
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc89)
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc90)
  func.func private @carts_kernel_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc91)
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc92)
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc93)
} loc(#loc)
#loc = loc(unknown)
#loc1 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:65)
#loc2 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:27)
#loc3 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:65)
#loc4 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":23:25)
#loc6 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":20:1)
#loc7 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":71:8)
#loc8 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":62:3)
#loc9 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.h":49:21)
#loc10 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":54:34)
#loc11 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":79:37)
#loc12 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:41)
#loc13 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":30:19)
#loc14 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":30:40)
#loc15 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":30:44)
#loc16 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":30:42)
#loc17 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":32:5)
#loc18 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":32:12)
#loc19 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":32:32)
#loc20 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":32:36)
#loc21 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":32:34)
#loc23 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":32:10)
#loc24 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":38:7)
#loc25 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":38:48)
#loc27 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":38:18)
#loc29 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":38:36)
#loc30 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":38:33)
#loc31 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":38:31)
#loc32 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":38:41)
#loc33 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":38:46)
#loc34 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":38:15)
#loc35 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":96:36)
#loc36 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":139:40)
#loc37 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":48:27)
#loc38 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":48:23)
#loc39 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:9)
#loc41 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":48:5)
#loc42 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:24)
#loc43 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:27)
#loc44 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:20)
#loc45 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:62)
#loc46 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:65)
#loc47 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:58)
#loc49 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:31)
#loc50 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:34)
#loc51 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:38)
#loc52 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:36)
#loc53 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:63)
#loc54 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:66)
#loc55 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:52)
#loc56 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:50)
#loc57 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:20)
#loc58 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:68)
#loc59 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:34)
#loc60 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:32)
#loc61 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:44)
#loc62 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:42)
#loc63 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:56)
#loc64 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":52:20)
#loc65 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":51:74)
#loc66 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":52:34)
#loc67 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":52:32)
#loc68 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":52:51)
#loc69 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":50:17)
#loc70 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":140:39)
#loc71 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:3)
#loc72 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":64:17)
#loc74 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":64:14)
#loc75 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":66:24)
#loc76 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":251:3)
#loc77 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":99:41)
#loc78 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:41)
#loc79 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":72:10)
#loc81 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":72:5)
#loc82 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":74:3)
#loc83 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":101:36)
#loc84 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":80:32)
#loc85 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c":81:1)
#loc86 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":50:6)
#loc87 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":74:6)
#loc88 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":90:6)
#loc89 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":93:6)
#loc90 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":128:6)
#loc91 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":131:6)
#loc92 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":207:6)
#loc93 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":77:6)
