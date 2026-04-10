module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("seidel-2d\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c320 = arith.constant 320 : index
    %c9600 = arith.constant 9600 : index
    %c0 = arith.constant 0 : index
    %c9599 = arith.constant 9599 : index
    %c9599_i32 = arith.constant 9599 : i32
    %cst = arith.constant 9.600000e+03 : f64
    %c-1_i32 = arith.constant -1 : i32
    %cst_0 = arith.constant 2.000000e+00 : f64
    %c1 = arith.constant 1 : index
    %false = arith.constant false
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_1 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %cst_2 = arith.constant 9.000000e+00 : f64
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    %4 = "polygeist.undef"() : () -> i32
    %alloca = memref.alloca() {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:62:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 99 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %5 = "polygeist.undef"() : () -> f64
    memref.store %5, %alloca[] : memref<f64>
    %alloca_3 = memref.alloca() {arts.id = 64 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:48:25", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 64 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %4, %alloca_3[] : memref<i32>
    %alloca_4 = memref.alloca() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:20:1", totalAccesses = 6 : i64, readCount = 2 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, firstUseId = 35 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true>, arts.metadata_provenance = "exact"} : memref<i1>
    memref.store %true, %alloca_4[] : memref<i1>
    call @carts_benchmarks_start() : () -> ()
    %6 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%6) : (memref<?xi8>) -> ()
    %7 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%7, %6) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "seidel-2d.c:30:19", totalAccesses = 12 : i64, readCount = 10 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 737280000 : i64, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 96 : i64>, arts.metadata_provenance = "exact"} : memref<9600x9600xf64>
    scf.for %arg2 = %c0 to %c9600 step %c1 {
      %10 = arith.index_cast %arg2 : index to i32
      %11 = arith.sitofp %10 : i32 to f64
      scf.for %arg3 = %c0 to %c9600 step %c1 {
        %12 = arith.index_cast %arg3 : index to i32
        %13 = arith.addi %12, %c2_i32 : i32
        %14 = arith.sitofp %13 : i32 to f64
        %15 = arith.mulf %11, %14 : f64
        %16 = arith.addf %15, %cst_0 : f64
        %17 = arith.divf %16, %cst : f64
        memref.store %17, %alloc[%arg2, %arg3] : memref<9600x9600xf64>
      }
    }
    call @carts_phase_timer_stop(%7) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%6) : (memref<?xi8>) -> ()
    scf.for %arg2 = %c0 to %c320 step %c1 {
      %10 = memref.load %alloca_4[] : memref<i1>
      scf.if %10 {
        memref.store %c9599_i32, %alloca_3[] : memref<i32>
        %11 = memref.load %alloca_3[] : memref<i32>
        %12 = arith.index_cast %11 : i32 to index
        %c2 = arith.constant 2 : index
        %c150 = arith.constant 150 : index
        %c960 = arith.constant 960 : index
        %13 = arith.subi %12, %c1 : index
        %14 = arith.subi %c9599, %c1 : index
        %c149 = arith.constant 149 : index
        %15 = arith.addi %13, %c149 : index
        %16 = arith.divui %15, %c150 : index
        %c959 = arith.constant 959 : index
        %17 = arith.addi %14, %c959 : index
        %18 = arith.divui %17, %c960 : index
        %19 = arith.subi %16, %c1 : index
        %20 = arith.subi %18, %c1 : index
        %21 = arith.muli %19, %c2 : index
        %22 = arith.addi %21, %20 : index
        %23 = arith.addi %22, %c1 : index
        %24 = arts.epoch {
          scf.for %arg3 = %c0 to %23 step %c1 {
            %25 = arith.cmpi uge, %arg3, %20 : index
            %26 = arith.subi %arg3, %20 : index
            %27 = arith.select %25, %26, %c0 : index
            %28 = arith.addi %27, %c1 : index
            %29 = arith.divui %28, %c2 : index
            %30 = arith.divui %arg3, %c2 : index
            %31 = arith.addi %30, %c1 : index
            %32 = arith.minui %31, %16 : index
            arts.edt <parallel> <internode> route(%c-1_i32) attributes {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, no_verify = #arts.no_verify, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]} {
              %33 = arith.muli %29, %c150 : index
              %34 = arith.muli %32, %c150 : index
              arts.for(%33) to(%34) step(%c150) {{
              ^bb0(%arg4: index):
                %35 = arith.divui %arg4, %c150 : index
                %36 = arith.muli %35, %c2 : index
                %37 = arith.subi %arg3, %36 : index
                %38 = arith.maxui %arg4, %c1 : index
                %39 = arith.addi %arg4, %c150 : index
                %40 = arith.minui %39, %12 : index
                %41 = arith.muli %37, %c960 : index
                %42 = arith.maxui %41, %c1 : index
                %43 = arith.addi %41, %c960 : index
                %44 = arith.minui %43, %c9599 : index
                scf.for %arg5 = %38 to %40 step %c1 {
                  %45 = arith.index_cast %arg5 : index to i32
                  memref.store %true, %alloca_4[] : memref<i1>
                  %46 = arith.addi %45, %c-1_i32 : i32
                  %47 = arith.index_cast %46 : i32 to index
                  %48 = arith.addi %45, %c1_i32 : i32
                  %49 = arith.index_cast %48 : i32 to index
                  scf.for %arg6 = %42 to %44 step %c1 {
                    %50 = arith.index_cast %arg6 : index to i32
                    %51 = arith.addi %50, %c-1_i32 : i32
                    %52 = arith.index_cast %51 : i32 to index
                    %53 = memref.load %alloc[%47, %52] : memref<9600x9600xf64>
                    %54 = memref.load %alloc[%47, %arg6] : memref<9600x9600xf64>
                    %55 = arith.addf %53, %54 : f64
                    %56 = arith.addi %50, %c1_i32 : i32
                    %57 = arith.index_cast %56 : i32 to index
                    %58 = memref.load %alloc[%47, %57] : memref<9600x9600xf64>
                    %59 = arith.addf %55, %58 : f64
                    %60 = memref.load %alloc[%arg5, %52] : memref<9600x9600xf64>
                    %61 = arith.addf %59, %60 : f64
                    %62 = memref.load %alloc[%arg5, %arg6] : memref<9600x9600xf64>
                    %63 = arith.addf %61, %62 : f64
                    %64 = memref.load %alloc[%arg5, %57] : memref<9600x9600xf64>
                    %65 = arith.addf %63, %64 : f64
                    %66 = memref.load %alloc[%49, %52] : memref<9600x9600xf64>
                    %67 = arith.addf %65, %66 : f64
                    %68 = memref.load %alloc[%49, %arg6] : memref<9600x9600xf64>
                    %69 = arith.addf %67, %68 : f64
                    %70 = memref.load %alloc[%49, %57] : memref<9600x9600xf64>
                    %71 = arith.addf %69, %70 : f64
                    %72 = arith.divf %71, %cst_2 : f64
                    memref.store %72, %alloc[%arg5, %arg6] : memref<9600x9600xf64>
                  }
                }
              }} {arts.pattern_revision = 1 : i64, arts.skip_loop_metadata_recovery, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]}
            }
          } {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]}
        } : i64
        memref.store %true, %alloca_4[] : memref<i1>
      }
    }
    %8 = memref.load %alloca_4[] : memref<i1>
    %9 = arith.select %8, %c0_i32, %4 : i32
    scf.if %8 {
      func.call @carts_kernel_timer_stop(%6) : (memref<?xi8>) -> ()
      %10 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%10, %6) : (memref<?xi8>, memref<?xi8>) -> ()
      memref.store %cst_1, %alloca[] : memref<f64>
      scf.for %arg2 = %c0 to %c9600 step %c1 {
        %13 = memref.load %alloc[%arg2, %arg2] : memref<9600x9600xf64>
        %14 = memref.load %alloca[] : memref<f64>
        %15 = arith.addf %14, %13 : f64
        memref.store %15, %alloca[] : memref<f64>
      }
      %11 = memref.load %alloca[] : memref<f64>
      func.call @carts_bench_checksum_d(%11) : (f64) -> ()
      func.call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
      %12 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%12, %6) : (memref<?xi8>, memref<?xi8>) -> ()
      func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> ()
      func.call @carts_e2e_timer_stop() : () -> ()
      memref.store %false, %alloca_4[] : memref<i1>
    }
    memref.dealloc %alloc {arts.id = 40 : i64} : memref<9600x9600xf64>
    return %9 : i32
  }
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>}
}
