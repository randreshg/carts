module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("seidel-2d\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c10 = arith.constant 10 : index
    %c9 = arith.constant 9 : index
    %c148 = arith.constant 148 : index
    %c-1 = arith.constant -1 : index
    %c960 = arith.constant 960 : index
    %c150 = arith.constant 150 : index
    %c2 = arith.constant 2 : index
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
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c9600, %c9600] {arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "seidel-2d.c:30:19", totalAccesses = 12 : i64, readCount = 10 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 737280000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = false, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 96 : i64>, arts.metadata_origin_id = 39 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
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
        %18 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %17, %18[%arg2, %arg3] : memref<?x?xf64>
      } {arts.id = 42 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:37:5">, arts.metadata_provenance = "recovered"}
    } {arts.id = 40 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:36:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%7) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%6) : (memref<?xi8>) -> ()
    scf.for %arg2 = %c0 to %c320 step %c1 {
      %10 = memref.load %alloca_4[] : memref<i1>
      scf.if %10 {
        memref.store %c9599_i32, %alloca_3[] : memref<i32>
        %11 = memref.load %alloca_3[] : memref<i32>
        %12 = arith.index_cast %11 : i32 to index
        %13 = arith.addi %12, %c148 : index
        %14 = arith.divui %13, %c150 : index
        %15 = arith.subi %14, %c1 : index
        %16 = arith.muli %15, %c2 : index
        %17 = arith.addi %16, %c10 : index
        %18 = arts.epoch {
          scf.for %arg3 = %c0 to %17 step %c1 {
            %19 = arith.cmpi uge, %arg3, %c9 : index
            %20 = arith.subi %arg3, %c9 : index
            %21 = arith.select %19, %20, %c0 : index
            %22 = arith.addi %21, %c1 : index
            %23 = arith.divui %22, %c2 : index
            %24 = arith.divui %arg3, %c2 : index
            %25 = arith.addi %24, %c1 : index
            %26 = arith.minui %25, %14 : index
            %27 = arith.muli %23, %c150 : index
            %28 = arith.muli %26, %c150 : index
            %guid_5, %ptr_6 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
            arts.lowering_contract(%ptr_6 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <wavefront_2d>, distributionPattern = <stencil>, revision = 1 : i64>) block_shape[%c150, %c960] min_offsets[%c-1, %c-1] max_offsets[%c1, %c1] write_footprint[%c0, %c0] contract(<ownerDims = [0, 1], spatialDims = [0, 1], supportedBlockHalo = true>)
            arts.edt <parallel> <intranode> route(%c-1_i32) (%ptr_6) : memref<?xmemref<?x?xf64>> attributes {arts.id = 113 : i64, arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0], workers = #arts.workers<64>} {
            ^bb0(%arg4: memref<?xmemref<?x?xf64>>):
              arts.for(%27) to(%28) step(%c150) {{
              ^bb0(%arg5: index):
                %29 = arith.divui %arg5, %c150 : index
                %30 = arith.muli %29, %c2 : index
                %31 = arith.subi %arg3, %30 : index
                %32 = arith.maxui %arg5, %c1 : index
                %33 = arith.addi %arg5, %c150 : index
                %34 = arith.minui %33, %12 : index
                %35 = arith.muli %31, %c960 : index
                %36 = arith.maxui %35, %c1 : index
                %37 = arith.addi %35, %c960 : index
                %38 = arith.minui %37, %c9599 : index
                scf.for %arg6 = %32 to %34 step %c1 {
                  %39 = arith.index_cast %arg6 : index to i32
                  %40 = arith.addi %39, %c-1_i32 : i32
                  %41 = arith.index_cast %40 : i32 to index
                  %42 = arith.addi %39, %c1_i32 : i32
                  %43 = arith.index_cast %42 : i32 to index
                  scf.for %arg7 = %36 to %38 step %c1 {
                    %44 = arith.index_cast %arg7 : index to i32
                    %45 = arith.addi %44, %c-1_i32 : i32
                    %46 = arith.index_cast %45 : i32 to index
                    %47 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                    %48 = memref.load %47[%41, %46] : memref<?x?xf64>
                    %49 = memref.load %47[%41, %arg7] : memref<?x?xf64>
                    %50 = arith.addf %48, %49 : f64
                    %51 = arith.addi %44, %c1_i32 : i32
                    %52 = arith.index_cast %51 : i32 to index
                    %53 = memref.load %47[%41, %52] : memref<?x?xf64>
                    %54 = arith.addf %50, %53 : f64
                    %55 = memref.load %47[%arg6, %46] : memref<?x?xf64>
                    %56 = arith.addf %54, %55 : f64
                    %57 = memref.load %47[%arg6, %arg7] : memref<?x?xf64>
                    %58 = arith.addf %56, %57 : f64
                    %59 = memref.load %47[%arg6, %52] : memref<?x?xf64>
                    %60 = arith.addf %58, %59 : f64
                    %61 = memref.load %47[%43, %46] : memref<?x?xf64>
                    %62 = arith.addf %60, %61 : f64
                    %63 = memref.load %47[%43, %arg7] : memref<?x?xf64>
                    %64 = arith.addf %62, %63 : f64
                    %65 = memref.load %47[%43, %52] : memref<?x?xf64>
                    %66 = arith.addf %64, %65 : f64
                    %67 = arith.divf %66, %cst_2 : f64
                    memref.store %67, %47[%arg6, %arg7] : memref<?x?xf64>
                  } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
                } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:46:3">, arts.metadata_provenance = "recovered"}
              }} {arts.pattern_revision = 1 : i64, arts.skip_loop_metadata_recovery, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]}
              arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
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
        %13 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        %14 = memref.load %13[%arg2, %arg2] : memref<?x?xf64>
        %15 = memref.load %alloca[] : memref<f64>
        %16 = arith.addf %15, %14 : f64
        memref.store %16, %alloca[] : memref<f64>
      } {arts.id = 87 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:63:3">, arts.metadata_provenance = "recovered"}
      %11 = memref.load %alloca[] : memref<f64>
      func.call @carts_bench_checksum_d(%11) : (f64) -> ()
      func.call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
      %12 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%12, %6) : (memref<?xi8>, memref<?xi8>) -> ()
      func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> ()
      func.call @carts_e2e_timer_stop() : () -> ()
      memref.store %false, %alloca_4[] : memref<i1>
    }
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
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
