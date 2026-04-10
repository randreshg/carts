#loc5 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":20:5)
#loc23 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":31:3)
#loc27 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":36:3)
#loc29 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":37:5)
#loc42 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":46:3)
#loc75 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":63:3)
#loc82 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":71:3)
module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32} loc(#loc1)
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32} loc(#loc2)
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32} loc(#loc3)
  llvm.mlir.global internal constant @str0("seidel-2d\00") {addr_space = 0 : i32} loc(#loc4)
  func.func @main(%arg0: i32 loc("benchmarks/polybench/seidel-2d/seidel-2d.c":20:5), %arg1: memref<?xmemref<?xi8>> loc("benchmarks/polybench/seidel-2d/seidel-2d.c":20:5)) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32 loc(#loc)
    %cst = arith.constant 2.000000e+00 : f64 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %false = arith.constant false loc(#loc)
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
    %alloca_2 = memref.alloca() : memref<1xi32> loc(#loc9)
    affine.store %4, %alloca_2[0] : memref<1xi32> loc(#loc9)
    %alloca_3 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc10)
    %alloca_4 = memref.alloca() : memref<i1> loc(#loc6)
    affine.store %true, %alloca_4[] : memref<i1> loc(#loc6)
    scf.if %true {
      scf.execute_region {
        func.call @carts_benchmarks_start() : () -> () loc(#loc11)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc12)
        func.call @carts_e2e_timer_start(%12) : (memref<?xi8>) -> () loc(#loc12)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc13)
        %13 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc13)
        func.call @carts_phase_timer_start(%12, %13) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc13)
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
    %8 = scf.if %true -> (i32) {
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
    %9 = arith.index_cast %8 : i32 to index loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %13 = polygeist.typeSize memref<?xf64> : index loc(#loc14)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc15)
            %15 = arith.index_cast %13 : index to i64 loc(#loc16)
            %16 = arith.muli %14, %15 : i64 loc(#loc17)
            %17 = arith.index_cast %16 : i64 to index loc(#loc14)
            %18 = arith.divui %17, %13 : index loc(#loc14)
            %alloc = memref.alloc(%18) : memref<?xmemref<?xf64>> loc(#loc14)
            affine.store %alloc, %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc10)
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
            %12 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc18)
            %13 = polygeist.typeSize f64 : index loc(#loc19)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc20)
            %15 = arith.index_cast %13 : index to i64 loc(#loc21)
            %16 = arith.muli %14, %15 : i64 loc(#loc22)
            %17 = arith.index_cast %16 : i64 to index loc(#loc19)
            %18 = arith.divui %17, %13 : index loc(#loc19)
            affine.for %arg2 loc("benchmarks/polybench/seidel-2d/seidel-2d.c":31:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %19 = polygeist.subindex %12[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc18)
                  %alloc = memref.alloc(%18) : memref<?xf64> loc(#loc19)
                  affine.store %alloc, %19[0] : memref<?xmemref<?xf64>> loc(#loc24)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc23)
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
            %12 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc25)
            %13 = arith.sitofp %6 : i32 to f64 loc(#loc26)
            affine.for %arg2 loc("benchmarks/polybench/seidel-2d/seidel-2d.c":36:3) = 0 to %7 {
              %14 = arith.index_cast %arg2 : index to i32 loc(#loc27)
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %15 = polygeist.subindex %12[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc25)
                      %16 = arith.sitofp %14 : i32 to f64 loc(#loc28)
                      affine.for %arg3 loc("benchmarks/polybench/seidel-2d/seidel-2d.c":37:5) = 0 to %7 {
                        %17 = arith.index_cast %arg3 : index to i32 loc(#loc29)
                        scf.if %true {
                          scf.execute_region {
                            %18 = affine.load %15[0] : memref<?xmemref<?xf64>> loc(#loc25)
                            %19 = polygeist.subindex %18[%arg3] () : memref<?xf64> -> memref<?xf64> loc(#loc25)
                            %20 = arith.addi %17, %c2_i32 : i32 loc(#loc30)
                            %21 = arith.sitofp %20 : i32 to f64 loc(#loc31)
                            %22 = arith.mulf %16, %21 : f64 loc(#loc32)
                            %23 = arith.addf %22, %cst : f64 loc(#loc33)
                            %24 = arith.divf %23, %13 : f64 loc(#loc34)
                            affine.store %24, %19[0] : memref<?xf64> loc(#loc35)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc29)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc27)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc36)
        func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc36)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc37)
        func.call @carts_kernel_timer_start(%12) : (memref<?xi8>) -> () loc(#loc37)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %12 = arith.addi %6, %c-1_i32 : i32 loc(#loc38)
            %13 = arith.addi %6, %c-1_i32 : i32 loc(#loc39)
            %14 = arith.index_cast %13 : i32 to index loc(#loc40)
            %15 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc41)
            affine.for %arg2 loc("benchmarks/polybench/seidel-2d/seidel-2d.c":46:3) = 0 to %9 {
              %16 = affine.load %alloca_4[] : memref<i1> loc(#loc)
              scf.if %16 {
                scf.execute_region {
                  scf.if %16 {
                    scf.execute_region {
                      scf.if %16 {
                        %19 = scf.execute_region -> i32 {
                          affine.store %12, %alloca_2[0] : memref<1xi32> loc(#loc9)
                          scf.yield %12 : i32 loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      %17 = affine.load %alloca_2[0] : memref<1xi32> loc(#loc38)
                      %18 = arith.index_cast %17 : i32 to index loc(#loc43)
                      omp.parallel {
                        scf.execute_region {
                          omp.wsloop schedule(static) {
                            omp.loop_nest (%arg3) : index = (%c1) to (%18) step (%c1) {
                              scf.execute_region {
                                %19 = arith.index_cast %arg3 : index to i32 loc(#loc43)
                                affine.store %true, %alloca_4[] : memref<i1> loc(#loc43)
                                scf.if %true {
                                  scf.execute_region {
                                    scf.if %true {
                                      scf.execute_region {
                                        %20 = polygeist.subindex %15[%arg3] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc41)
                                        %21 = arith.addi %19, %c-1_i32 : i32 loc(#loc44)
                                        %22 = arith.index_cast %21 : i32 to index loc(#loc45)
                                        %23 = polygeist.subindex %15[%22] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc46)
                                        %24 = arith.addi %19, %c1_i32 : i32 loc(#loc47)
                                        %25 = arith.index_cast %24 : i32 to index loc(#loc48)
                                        %26 = polygeist.subindex %15[%25] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc49)
                                        scf.for %arg4 = %c1 to %14 step %c1 {
                                          %27 = arith.index_cast %arg4 : index to i32 loc(#loc50)
                                          scf.if %true {
                                            scf.execute_region {
                                              %28 = affine.load %20[0] : memref<?xmemref<?xf64>> loc(#loc41)
                                              %29 = polygeist.subindex %28[%arg4] () : memref<?xf64> -> memref<?xf64> loc(#loc41)
                                              %30 = affine.load %23[0] : memref<?xmemref<?xf64>> loc(#loc46)
                                              %31 = arith.addi %27, %c-1_i32 : i32 loc(#loc51)
                                              %32 = arith.index_cast %31 : i32 to index loc(#loc52)
                                              %33 = polygeist.subindex %30[%32] () : memref<?xf64> -> memref<?xf64> loc(#loc46)
                                              %34 = affine.load %33[0] : memref<?xf64> loc(#loc46)
                                              %35 = polygeist.subindex %30[%arg4] () : memref<?xf64> -> memref<?xf64> loc(#loc53)
                                              %36 = affine.load %35[0] : memref<?xf64> loc(#loc53)
                                              %37 = arith.addf %34, %36 : f64 loc(#loc54)
                                              %38 = arith.addi %27, %c1_i32 : i32 loc(#loc55)
                                              %39 = arith.index_cast %38 : i32 to index loc(#loc56)
                                              %40 = polygeist.subindex %30[%39] () : memref<?xf64> -> memref<?xf64> loc(#loc57)
                                              %41 = affine.load %40[0] : memref<?xf64> loc(#loc57)
                                              %42 = arith.addf %37, %41 : f64 loc(#loc58)
                                              %43 = polygeist.subindex %28[%32] () : memref<?xf64> -> memref<?xf64> loc(#loc59)
                                              %44 = affine.load %43[0] : memref<?xf64> loc(#loc59)
                                              %45 = arith.addf %42, %44 : f64 loc(#loc60)
                                              %46 = affine.load %29[0] : memref<?xf64> loc(#loc61)
                                              %47 = arith.addf %45, %46 : f64 loc(#loc62)
                                              %48 = polygeist.subindex %28[%39] () : memref<?xf64> -> memref<?xf64> loc(#loc63)
                                              %49 = affine.load %48[0] : memref<?xf64> loc(#loc63)
                                              %50 = arith.addf %47, %49 : f64 loc(#loc64)
                                              %51 = affine.load %26[0] : memref<?xmemref<?xf64>> loc(#loc49)
                                              %52 = polygeist.subindex %51[%32] () : memref<?xf64> -> memref<?xf64> loc(#loc49)
                                              %53 = affine.load %52[0] : memref<?xf64> loc(#loc49)
                                              %54 = arith.addf %50, %53 : f64 loc(#loc65)
                                              %55 = polygeist.subindex %51[%arg4] () : memref<?xf64> -> memref<?xf64> loc(#loc66)
                                              %56 = affine.load %55[0] : memref<?xf64> loc(#loc66)
                                              %57 = arith.addf %54, %56 : f64 loc(#loc67)
                                              %58 = polygeist.subindex %51[%39] () : memref<?xf64> -> memref<?xf64> loc(#loc68)
                                              %59 = affine.load %58[0] : memref<?xf64> loc(#loc68)
                                              %60 = arith.addf %57, %59 : f64 loc(#loc69)
                                              %61 = arith.divf %60, %cst_1 : f64 loc(#loc70)
                                              affine.store %61, %29[0] : memref<?xf64> loc(#loc71)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                        } loc(#loc50)
                                        scf.yield loc(#loc)
                                      } loc(#loc)
                                    } loc(#loc)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                                scf.yield loc(#loc43)
                              } loc(#loc43)
                              omp.yield loc(#loc43)
                            } loc(#loc43)
                          } loc(#loc43)
                          scf.yield loc(#loc43)
                        } loc(#loc43)
                        omp.terminator loc(#loc43)
                      } loc(#loc43)
                      affine.store %true, %alloca_4[] : memref<i1> loc(#loc43)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
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
    %10 = affine.load %alloca_4[] : memref<i1> loc(#loc)
    scf.if %10 {
      scf.execute_region {
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc72)
        func.call @carts_kernel_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc72)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        %12 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc73)
        %13 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc73)
        func.call @carts_phase_timer_start(%12, %13) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc73)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        scf.if %10 {
          scf.execute_region {
            affine.store %cst_0, %alloca[0] : memref<1xf64> loc(#loc8)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        scf.if %10 {
          scf.execute_region {
            %12 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc74)
            affine.for %arg2 loc("benchmarks/polybench/seidel-2d/seidel-2d.c":63:3) = 0 to %7 {
              scf.if %10 {
                scf.execute_region {
                  %13 = polygeist.subindex %12[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc74)
                  %14 = affine.load %13[0] : memref<?xmemref<?xf64>> loc(#loc74)
                  %15 = polygeist.subindex %14[%arg2] () : memref<?xf64> -> memref<?xf64> loc(#loc74)
                  %16 = affine.load %15[0] : memref<?xf64> loc(#loc74)
                  %17 = affine.load %alloca[0] : memref<1xf64> loc(#loc76)
                  %18 = arith.addf %17, %16 : f64 loc(#loc76)
                  affine.store %18, %alloca[0] : memref<1xf64> loc(#loc76)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc75)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        %12 = affine.load %alloca[0] : memref<1xf64> loc(#loc77)
        func.call @carts_bench_checksum_d(%12) : (f64) -> () loc(#loc78)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        %12 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc79)
        func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc79)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        %12 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc80)
        %13 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc80)
        func.call @carts_phase_timer_start(%12, %13) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc80)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        scf.if %10 {
          scf.execute_region {
            %12 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc81)
            affine.for %arg2 loc("benchmarks/polybench/seidel-2d/seidel-2d.c":71:3) = 0 to %7 {
              scf.if %10 {
                scf.execute_region {
                  %13 = polygeist.subindex %12[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc81)
                  %14 = affine.load %13[0] : memref<?xmemref<?xf64>> loc(#loc83)
                  memref.dealloc %14 : memref<?xf64> loc(#loc83)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc82)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        %12 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc84)
        memref.dealloc %12 : memref<?xmemref<?xf64>> loc(#loc84)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        %12 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc85)
        func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc85)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %10 {
      scf.execute_region {
        func.call @carts_e2e_timer_stop() : () -> () loc(#loc86)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %11 = scf.if %10 -> (i32) {
      %12 = scf.execute_region -> i32 {
        %13 = scf.if %10 -> (i32) {
          %14 = scf.execute_region -> i32 {
            affine.store %false, %alloca_4[] : memref<i1> loc(#loc87)
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
    return %11 : i32 loc(#loc88)
  } loc(#loc5)
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc89)
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc90)
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc91)
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc92)
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc93)
  func.func private @carts_kernel_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc94)
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc95)
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc96)
} loc(#loc)
#loc = loc(unknown)
#loc1 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:65)
#loc2 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:27)
#loc3 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:65)
#loc4 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":23:25)
#loc6 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":20:1)
#loc7 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":71:8)
#loc8 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":62:3)
#loc9 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":48:25)
#loc10 = loc("benchmarks/polybench/seidel-2d/seidel-2d.h":49:21)
#loc11 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":54:34)
#loc12 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":79:37)
#loc13 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:41)
#loc14 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":30:19)
#loc15 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":30:40)
#loc16 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":30:44)
#loc17 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":30:42)
#loc18 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":32:5)
#loc19 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":32:12)
#loc20 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":32:32)
#loc21 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":32:36)
#loc22 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":32:34)
#loc24 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":32:10)
#loc25 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":38:7)
#loc26 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":38:48)
#loc28 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":38:18)
#loc30 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":38:36)
#loc31 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":38:33)
#loc32 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":38:31)
#loc33 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":38:41)
#loc34 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":38:46)
#loc35 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":38:15)
#loc36 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":96:36)
#loc37 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":139:40)
#loc38 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":48:27)
#loc39 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":49:29)
#loc40 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":49:25)
#loc41 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:9)
#loc43 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":47:1)
#loc44 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:24)
#loc45 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:27)
#loc46 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:20)
#loc47 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:62)
#loc48 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:65)
#loc49 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:58)
#loc50 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":49:7)
#loc51 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:31)
#loc52 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:34)
#loc53 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:38)
#loc54 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:36)
#loc55 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:63)
#loc56 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:66)
#loc57 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:52)
#loc58 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:50)
#loc59 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:20)
#loc60 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:68)
#loc61 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:34)
#loc62 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:32)
#loc63 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:44)
#loc64 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:42)
#loc65 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:56)
#loc66 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":52:20)
#loc67 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":51:74)
#loc68 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":52:34)
#loc69 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":52:32)
#loc70 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":52:51)
#loc71 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":50:17)
#loc72 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":140:39)
#loc73 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:3)
#loc74 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":64:17)
#loc76 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":64:14)
#loc77 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":66:24)
#loc78 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":251:3)
#loc79 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":99:41)
#loc80 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:41)
#loc81 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":72:10)
#loc83 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":72:5)
#loc84 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":74:3)
#loc85 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":101:36)
#loc86 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":80:32)
#loc87 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":80:3)
#loc88 = loc("benchmarks/polybench/seidel-2d/seidel-2d.c":81:1)
#loc89 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":50:6)
#loc90 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":74:6)
#loc91 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":90:6)
#loc92 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":93:6)
#loc93 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":128:6)
#loc94 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":131:6)
#loc95 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":207:6)
#loc96 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":77:6)
