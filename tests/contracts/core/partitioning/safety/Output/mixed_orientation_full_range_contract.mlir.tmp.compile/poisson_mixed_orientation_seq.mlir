#loc28 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":45:3)
#loc41 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":54:3)
#loc49 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":6:13)
#loc51 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":10:3)
#loc52 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":11:5)
#loc58 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":17:13)
#loc60 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":21:3)
#loc63 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":22:5)
#loc67 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":28:3)
#loc80 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":29:5)
#set = affine_set<(d0) : (d0 == 0)>
module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c64_i32 = arith.constant 64 : i32 loc(#loc)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %c64_i64 = arith.constant 64 : i64 loc(#loc)
    %true = arith.constant true loc(#loc2)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc3)
    %alloca = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc4)
    %alloca_0 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc5)
    %alloca_1 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc6)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %2 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %3 = polygeist.typeSize memref<?xf64> : index loc(#loc7)
            %4 = arith.index_cast %3 : index to i64 loc(#loc8)
            %5 = arith.muli %4, %c64_i64 : i64 loc(#loc9)
            %6 = arith.index_cast %5 : i64 to index loc(#loc7)
            %7 = arith.divui %6, %3 : index loc(#loc7)
            %alloc = memref.alloc(%7) : memref<?xmemref<?xf64>> loc(#loc7)
            affine.store %alloc, %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc6)
            scf.yield %alloc : memref<?xmemref<?xf64>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %2 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %3 = polygeist.typeSize memref<?xf64> : index loc(#loc10)
            %4 = arith.index_cast %3 : index to i64 loc(#loc11)
            %5 = arith.muli %4, %c64_i64 : i64 loc(#loc12)
            %6 = arith.index_cast %5 : i64 to index loc(#loc10)
            %7 = arith.divui %6, %3 : index loc(#loc10)
            %alloc = memref.alloc(%7) : memref<?xmemref<?xf64>> loc(#loc10)
            affine.store %alloc, %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc5)
            scf.yield %alloc : memref<?xmemref<?xf64>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %2 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %3 = polygeist.typeSize memref<?xf64> : index loc(#loc13)
            %4 = arith.index_cast %3 : index to i64 loc(#loc14)
            %5 = arith.muli %4, %c64_i64 : i64 loc(#loc15)
            %6 = arith.index_cast %5 : i64 to index loc(#loc13)
            %7 = arith.divui %6, %3 : index loc(#loc13)
            %alloc = memref.alloc(%7) : memref<?xmemref<?xf64>> loc(#loc13)
            affine.store %alloc, %alloca[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc4)
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
            %2 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc16)
            %3 = polygeist.typeSize f64 : index loc(#loc17)
            %4 = arith.index_cast %3 : index to i64 loc(#loc18)
            %5 = arith.muli %4, %c64_i64 : i64 loc(#loc19)
            %6 = arith.index_cast %5 : i64 to index loc(#loc17)
            %7 = arith.divui %6, %3 : index loc(#loc17)
            %8 = affine.load %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc20)
            %9 = polygeist.typeSize f64 : index loc(#loc21)
            %10 = arith.index_cast %9 : index to i64 loc(#loc22)
            %11 = arith.muli %10, %c64_i64 : i64 loc(#loc23)
            %12 = arith.index_cast %11 : i64 to index loc(#loc21)
            %13 = arith.divui %12, %9 : index loc(#loc21)
            %14 = affine.load %alloca[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc24)
            %15 = polygeist.typeSize f64 : index loc(#loc25)
            %16 = arith.index_cast %15 : index to i64 loc(#loc26)
            %17 = arith.muli %16, %c64_i64 : i64 loc(#loc27)
            %18 = arith.index_cast %17 : i64 to index loc(#loc25)
            %19 = arith.divui %18, %15 : index loc(#loc25)
            affine.for %arg0 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":45:3) = 0 to 64 {
              scf.if %true {
                scf.execute_region {
                  %20 = polygeist.subindex %2[%arg0] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc16)
                  %alloc = memref.alloc(%7) : memref<?xf64> loc(#loc17)
                  affine.store %alloc, %20[0] : memref<?xmemref<?xf64>> loc(#loc29)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %20 = polygeist.subindex %8[%arg0] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc20)
                  %alloc = memref.alloc(%13) : memref<?xf64> loc(#loc21)
                  affine.store %alloc, %20[0] : memref<?xmemref<?xf64>> loc(#loc30)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %20 = polygeist.subindex %14[%arg0] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc24)
                  %alloc = memref.alloc(%19) : memref<?xf64> loc(#loc25)
                  affine.store %alloc, %20[0] : memref<?xmemref<?xf64>> loc(#loc31)
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
    scf.if %true {
      scf.execute_region {
        %2 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc32)
        func.call @rhs(%c64_i32, %c64_i32, %2) : (i32, i32, memref<?xmemref<?xf64>>) -> () loc(#loc33)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %2 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc34)
        %3 = affine.load %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc35)
        %4 = affine.load %alloca[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc36)
        func.call @sweep(%c64_i32, %c64_i32, %2, %3, %4) : (i32, i32, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>) -> () loc(#loc37)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %2 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc38)
            %3 = affine.load %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc39)
            %4 = affine.load %alloca[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc40)
            affine.for %arg0 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":54:3) = 0 to 64 {
              scf.if %true {
                scf.execute_region {
                  %5 = polygeist.subindex %2[%arg0] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc38)
                  %6 = affine.load %5[0] : memref<?xmemref<?xf64>> loc(#loc42)
                  memref.dealloc %6 : memref<?xf64> loc(#loc42)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %5 = polygeist.subindex %3[%arg0] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc39)
                  %6 = affine.load %5[0] : memref<?xmemref<?xf64>> loc(#loc43)
                  memref.dealloc %6 : memref<?xf64> loc(#loc43)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %5 = polygeist.subindex %4[%arg0] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc40)
                  %6 = affine.load %5[0] : memref<?xmemref<?xf64>> loc(#loc44)
                  memref.dealloc %6 : memref<?xf64> loc(#loc44)
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
    scf.if %true {
      scf.execute_region {
        %2 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc45)
        memref.dealloc %2 : memref<?xmemref<?xf64>> loc(#loc45)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %2 = affine.load %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc46)
        memref.dealloc %2 : memref<?xmemref<?xf64>> loc(#loc46)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %2 = affine.load %alloca[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc47)
        memref.dealloc %2 : memref<?xmemref<?xf64>> loc(#loc47)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %1 = scf.if %true -> (i32) {
      %2 = scf.execute_region -> i32 {
        %3 = scf.if %true -> (i32) {
          %4 = scf.execute_region -> i32 {
            scf.yield %c0_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %4 : i32 loc(#loc)
        } else {
          scf.yield %0 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %3 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %2 : i32 loc(#loc)
    } else {
      scf.yield %0 : i32 loc(#loc)
    } loc(#loc)
    return %1 : i32 loc(#loc48)
  } loc(#loc1)
  func.func private @rhs(%arg0: i32 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":6:13), %arg1: i32 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":6:13), %arg2: memref<?xmemref<?xf64>> loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":6:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %true = arith.constant true loc(#loc50)
    %0 = arith.index_cast %arg1 : i32 to index loc(#loc49)
    %1 = arith.index_cast %arg0 : i32 to index loc(#loc49)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.for %arg3 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":10:3) = 0 to %0 {
              %2 = arith.index_cast %arg3 : index to i32 loc(#loc51)
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      affine.for %arg4 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":11:5) = 0 to %1 {
                        %3 = arith.index_cast %arg4 : index to i32 loc(#loc52)
                        scf.if %true {
                          scf.execute_region {
                            %4 = polygeist.subindex %arg2[%arg4] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc53)
                            %5 = affine.load %4[0] : memref<?xmemref<?xf64>> loc(#loc53)
                            %6 = polygeist.subindex %5[%arg3] () : memref<?xf64> -> memref<?xf64> loc(#loc53)
                            %7 = arith.addi %3, %2 : i32 loc(#loc54)
                            %8 = arith.sitofp %7 : i32 to f64 loc(#loc55)
                            affine.store %8, %6[0] : memref<?xf64> loc(#loc56)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc52)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
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
    return loc(#loc57)
  } loc(#loc49)
  func.func private @sweep(%arg0: i32 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":17:13), %arg1: i32 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":17:13), %arg2: memref<?xmemref<?xf64>> loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":17:13), %arg3: memref<?xmemref<?xf64>> loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":17:13), %arg4: memref<?xmemref<?xf64>> loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":17:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %true = arith.constant true loc(#loc59)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %c1_i32 = arith.constant 1 : i32 loc(#loc)
    %cst = arith.constant 2.500000e-01 : f64 loc(#loc)
    %c-1_i32 = arith.constant -1 : i32 loc(#loc)
    %0 = arith.index_cast %arg0 : i32 to index loc(#loc58)
    %1 = arith.index_cast %arg1 : i32 to index loc(#loc58)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.for %arg5 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":21:3) = 0 to %0 {
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %2 = polygeist.subindex %arg3[%arg5] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc61)
                      %3 = polygeist.subindex %arg4[%arg5] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc62)
                      affine.for %arg6 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":22:5) = 0 to %1 {
                        scf.if %true {
                          scf.execute_region {
                            %4 = affine.load %2[0] : memref<?xmemref<?xf64>> loc(#loc61)
                            %5 = polygeist.subindex %4[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc61)
                            %6 = affine.load %3[0] : memref<?xmemref<?xf64>> loc(#loc62)
                            %7 = polygeist.subindex %6[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc62)
                            %8 = affine.load %7[0] : memref<?xf64> loc(#loc62)
                            affine.store %8, %5[0] : memref<?xf64> loc(#loc64)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc63)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc60)
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
            %2 = arith.addi %arg0, %c-1_i32 : i32 loc(#loc65)
            %3 = arith.addi %arg1, %c-1_i32 : i32 loc(#loc66)
            affine.for %arg5 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":28:3) = 0 to %0 {
              %4 = arith.index_cast %arg5 : index to i32 loc(#loc67)
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %5 = arith.cmpi eq, %4, %2 : i32 loc(#loc68)
                      %6 = polygeist.subindex %arg4[%arg5] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc69)
                      %7 = polygeist.subindex %arg2[%arg5] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc70)
                      %8 = polygeist.subindex %arg4[%arg5] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc71)
                      %9 = arith.addi %4, %c-1_i32 : i32 loc(#loc72)
                      %10 = arith.index_cast %9 : i32 to index loc(#loc73)
                      %11 = polygeist.subindex %arg3[%10] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc74)
                      %12 = polygeist.subindex %arg3[%arg5] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc75)
                      %13 = arith.addi %4, %c1_i32 : i32 loc(#loc76)
                      %14 = arith.index_cast %13 : i32 to index loc(#loc77)
                      %15 = polygeist.subindex %arg3[%14] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc78)
                      %16 = polygeist.subindex %arg2[%arg5] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc79)
                      affine.for %arg6 loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":29:5) = 0 to %1 {
                        %17 = arith.index_cast %arg6 : index to i32 loc(#loc80)
                        scf.if %true {
                          scf.execute_region {
                            scf.if %true {
                              scf.execute_region {
                                %18 = affine.if #set(%arg5) -> i1 {
                                  affine.yield %true : i1 loc(#loc81)
                                } else {
                                  %21 = arith.cmpi eq, %17, %c0_i32 : i32 loc(#loc82)
                                  affine.yield %21 : i1 loc(#loc81)
                                } loc(#loc81)
                                %19 = scf.if %18 -> (i1) {
                                  scf.yield %true : i1 loc(#loc83)
                                } else {
                                  scf.yield %5 : i1 loc(#loc83)
                                } loc(#loc83)
                                %20 = scf.if %19 -> (i1) {
                                  scf.yield %true : i1 loc(#loc84)
                                } else {
                                  %21 = arith.cmpi eq, %17, %3 : i32 loc(#loc85)
                                  scf.yield %21 : i1 loc(#loc84)
                                } loc(#loc84)
                                scf.if %20 {
                                  scf.if %true {
                                    scf.execute_region {
                                      %21 = affine.load %6[0] : memref<?xmemref<?xf64>> loc(#loc69)
                                      %22 = polygeist.subindex %21[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc69)
                                      %23 = affine.load %7[0] : memref<?xmemref<?xf64>> loc(#loc70)
                                      %24 = polygeist.subindex %23[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc70)
                                      %25 = affine.load %24[0] : memref<?xf64> loc(#loc70)
                                      affine.store %25, %22[0] : memref<?xf64> loc(#loc87)
                                      scf.yield loc(#loc)
                                    } loc(#loc)
                                  } loc(#loc)
                                } else {
                                  scf.if %true {
                                    scf.execute_region {
                                      %21 = affine.load %8[0] : memref<?xmemref<?xf64>> loc(#loc71)
                                      %22 = polygeist.subindex %21[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc71)
                                      %23 = affine.load %11[0] : memref<?xmemref<?xf64>> loc(#loc74)
                                      %24 = polygeist.subindex %23[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc74)
                                      %25 = affine.load %24[0] : memref<?xf64> loc(#loc74)
                                      %26 = affine.load %12[0] : memref<?xmemref<?xf64>> loc(#loc75)
                                      %27 = arith.addi %17, %c1_i32 : i32 loc(#loc88)
                                      %28 = arith.index_cast %27 : i32 to index loc(#loc89)
                                      %29 = polygeist.subindex %26[%28] () : memref<?xf64> -> memref<?xf64> loc(#loc75)
                                      %30 = affine.load %29[0] : memref<?xf64> loc(#loc75)
                                      %31 = arith.addf %25, %30 : f64 loc(#loc90)
                                      %32 = arith.addi %17, %c-1_i32 : i32 loc(#loc91)
                                      %33 = arith.index_cast %32 : i32 to index loc(#loc92)
                                      %34 = polygeist.subindex %26[%33] () : memref<?xf64> -> memref<?xf64> loc(#loc93)
                                      %35 = affine.load %34[0] : memref<?xf64> loc(#loc93)
                                      %36 = arith.addf %31, %35 : f64 loc(#loc94)
                                      %37 = affine.load %15[0] : memref<?xmemref<?xf64>> loc(#loc78)
                                      %38 = polygeist.subindex %37[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc78)
                                      %39 = affine.load %38[0] : memref<?xf64> loc(#loc78)
                                      %40 = arith.addf %36, %39 : f64 loc(#loc95)
                                      %41 = affine.load %16[0] : memref<?xmemref<?xf64>> loc(#loc79)
                                      %42 = polygeist.subindex %41[%arg6] () : memref<?xf64> -> memref<?xf64> loc(#loc79)
                                      %43 = affine.load %42[0] : memref<?xf64> loc(#loc79)
                                      %44 = arith.addf %40, %43 : f64 loc(#loc96)
                                      %45 = arith.mulf %44, %cst : f64 loc(#loc97)
                                      affine.store %45, %22[0] : memref<?xf64> loc(#loc98)
                                      scf.yield loc(#loc)
                                    } loc(#loc)
                                  } loc(#loc)
                                } loc(#loc86)
                                scf.yield loc(#loc)
                              } loc(#loc)
                            } loc(#loc)
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
            } loc(#loc67)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc99)
  } loc(#loc58)
} loc(#loc)
#loc = loc(unknown)
#loc1 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":40:5)
#loc2 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":40:1)
#loc3 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":54:8)
#loc4 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":43:3)
#loc5 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":42:3)
#loc6 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":41:3)
#loc7 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":41:16)
#loc8 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":41:39)
#loc9 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":41:37)
#loc10 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":42:16)
#loc11 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":42:39)
#loc12 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":42:37)
#loc13 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":43:19)
#loc14 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":43:42)
#loc15 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":43:40)
#loc16 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":46:5)
#loc17 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":46:12)
#loc18 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":46:34)
#loc19 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":46:32)
#loc20 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":47:5)
#loc21 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":47:12)
#loc22 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":47:34)
#loc23 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":47:32)
#loc24 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":48:5)
#loc25 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":48:15)
#loc26 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":48:37)
#loc27 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":48:35)
#loc29 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":46:10)
#loc30 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":47:10)
#loc31 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":48:13)
#loc32 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":51:15)
#loc33 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":51:3)
#loc34 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":52:17)
#loc35 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":52:20)
#loc36 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":52:23)
#loc37 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":52:3)
#loc38 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":55:10)
#loc39 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":56:10)
#loc40 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":57:10)
#loc42 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":55:5)
#loc43 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":56:5)
#loc44 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":57:5)
#loc45 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":59:3)
#loc46 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":60:3)
#loc47 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":61:3)
#loc48 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":64:1)
#loc50 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":6:1)
#loc53 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":12:7)
#loc54 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":12:28)
#loc55 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":12:17)
#loc56 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":12:15)
#loc57 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":15:1)
#loc59 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":17:1)
#loc61 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":23:7)
#loc62 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":23:17)
#loc64 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":23:15)
#loc65 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":30:39)
#loc66 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":30:54)
#loc68 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":30:33)
#loc69 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":31:9)
#loc70 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":31:22)
#loc71 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:9)
#loc72 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:34)
#loc73 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:37)
#loc74 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:30)
#loc75 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:44)
#loc76 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":34:34)
#loc77 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":34:37)
#loc78 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":34:30)
#loc79 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":34:44)
#loc81 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":30:18)
#loc82 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":30:23)
#loc83 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":30:28)
#loc84 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":30:43)
#loc85 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":30:48)
#loc86 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":30:7)
#loc87 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":31:20)
#loc88 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:51)
#loc89 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:54)
#loc90 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:42)
#loc91 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:65)
#loc92 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:68)
#loc93 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:58)
#loc94 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:56)
#loc95 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:70)
#loc96 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":34:42)
#loc97 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:27)
#loc98 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":33:20)
#loc99 = loc("/home/raherrer/projects/carts/tests/examples/mixed_orientation/poisson_mixed_orientation.c":38:1)
