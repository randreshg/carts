module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("atax\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1999 = arith.constant 1999 : index
    %c2 = arith.constant 2 : index
    %cst = arith.constant 3.1415926535897931 : f64
    %c2000 = arith.constant 2000 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst_0 = arith.constant 4.000000e+03 : f64
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c4000 = arith.constant 4000 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_1 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    call @carts_benchmarks_start() : () -> ()
    %4 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%4) : (memref<?xi8>) -> ()
    %5 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%5, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %c1_2 = arith.constant 1 : index
    %c1_3 = arith.constant 1 : index
    %c1_4 = arith.constant 1 : index
    %6 = arts.runtime_query <total_workers> -> i32
    %7 = arith.index_castui %6 : i32 to index
    %8 = arith.maxui %7, %c1_4 : index
    %9 = arith.subi %8, %c1_4 : index
    %10 = arith.addi %c4000, %9 : index
    %11 = arith.divui %10, %8 : index
    %12 = arith.maxui %11, %c1_4 : index
    %c1_5 = arith.constant 1 : index
    %13 = arith.maxui %12, %c1_5 : index
    %c1_i64 = arith.constant 1 : i64
    %14 = arith.index_cast %c4000 : index to i64
    %15 = arith.index_cast %13 : index to i64
    %16 = arith.addi %14, %15 : i64
    %17 = arith.subi %16, %c1_i64 : i64
    %18 = arith.divui %17, %15 : i64
    %19 = arith.index_cast %18 : i64 to index
    %c0_6 = arith.constant 0 : index
    %c1_7 = arith.constant 1 : index
    %c0_8 = arith.constant 0 : index
    %c1_9 = arith.constant 1 : index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%19] elementType(f64) elementSizes[%13, %c4000] {arts.id = 32 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "atax.c:87:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 128000000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 32 : i64, lastUseId = 33 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 32 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf64>>) block_shape[%13] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_10 = arith.constant 0 : index
    %c1_11 = arith.constant 1 : index
    %c1_12 = arith.constant 1 : index
    %20 = arith.minui %c2000, %c2000 : index
    %c1_13 = arith.constant 1 : index
    %21 = arith.maxui %20, %c1_13 : index
    %c1_i64_14 = arith.constant 1 : i64
    %22 = arith.index_cast %c4000 : index to i64
    %23 = arith.index_cast %21 : index to i64
    %24 = arith.addi %22, %23 : i64
    %25 = arith.subi %24, %c1_i64_14 : i64
    %26 = arith.divui %25, %23 : i64
    %27 = arith.index_cast %26 : i64 to index
    %c0_15 = arith.constant 0 : index
    %c1_16 = arith.constant 1 : index
    %c0_17 = arith.constant 0 : index
    %c1_18 = arith.constant 1 : index
    %guid_19, %ptr_20 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%27] elementType(f64) elementSizes[%21] {arts.id = 34 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:89:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 34 : i64, lastUseId = 85 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 34 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_20 : memref<?xmemref<?xf64>>) block_shape[%21] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_21 = arith.constant 0 : index
    %c1_22 = arith.constant 1 : index
    %c1_23 = arith.constant 1 : index
    %c1_24 = arith.constant 1 : index
    %28 = arith.maxui %c2000, %c1_24 : index
    %c1_i64_25 = arith.constant 1 : i64
    %29 = arith.index_cast %c4000 : index to i64
    %30 = arith.index_cast %28 : index to i64
    %31 = arith.addi %29, %30 : i64
    %32 = arith.subi %31, %c1_i64_25 : i64
    %33 = arith.divui %32, %30 : i64
    %34 = arith.index_cast %33 : i64 to index
    %c0_26 = arith.constant 0 : index
    %c1_27 = arith.constant 1 : index
    %c0_28 = arith.constant 0 : index
    %c1_29 = arith.constant 1 : index
    %guid_30, %ptr_31 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%34] elementType(f64) elementSizes[%28] {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:90:20", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 86 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_31 : memref<?xmemref<?xf64>>) block_shape[%28] contract(<ownerDims = [0], postDbRefined = true>)
    scf.for %arg2 = %c0 to %c4000 step %c1 {
      %40 = arith.index_cast %arg2 : index to i32
      %41 = arith.sitofp %40 : i32 to f64
      scf.for %arg3 = %c0 to %c4000 step %c1 {
        %42 = arith.index_cast %arg3 : index to i32
        %43 = arith.addi %42, %c1_i32 : i32
        %44 = arith.sitofp %43 : i32 to f64
        %45 = arith.mulf %41, %44 : f64
        %46 = arith.divf %45, %cst_0 : f64
        %c1_32 = arith.constant 1 : index
        %47 = arith.maxui %13, %c1_32 : index
        %48 = arith.divui %arg2, %47 : index
        %49 = arith.subi %48, %c0_10 : index
        %50 = arith.remui %arg2, %47 : index
        %51 = arts.db_ref %ptr[%49] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %46, %51[%50, %arg3] : memref<?x?xf64>
      } {arts.id = 48 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 50 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    %35 = arts.epoch attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %40 = arith.muli %arg2, %c2000 : index
        %41 = arith.cmpi uge, %40, %c4000 : index
        %42 = arith.subi %c4000, %40 : index
        %43 = arith.select %41, %c0, %42 : index
        %44 = arith.minui %43, %c2000 : index
        %45 = arith.cmpi ugt, %44, %c0 : index
        scf.if %45 {
          %46 = arith.divui %40, %c4000 : index
          %47 = arith.addi %40, %c1999 : index
          %48 = arith.divui %47, %c4000 : index
          %49 = arith.cmpi ugt, %46, %c0 : index
          %50 = arith.select %49, %48, %c0 : index
          %51 = arith.subi %50, %46 : index
          %52 = arith.addi %51, %c1 : index
          %53 = arith.select %49, %c0, %46 : index
          %54 = arith.select %49, %c0, %52 : index
          %c1_32 = arith.constant 1 : index
          %c0_33 = arith.constant 0 : index
          %55 = arith.maxui %28, %c1_32 : index
          %56 = arith.muli %34, %55 : index
          %57 = arith.divui %40, %55 : index
          %58 = arith.addi %40, %c2000 : index
          %59 = arith.subi %58, %c1_32 : index
          %60 = arith.divui %59, %55 : index
          %61 = arith.subi %34, %c1_32 : index
          %62 = arith.cmpi ugt, %57, %61 : index
          %63 = arith.minui %60, %61 : index
          %64 = arith.select %62, %60, %63 : index
          %65 = arith.subi %64, %57 : index
          %66 = arith.addi %65, %c1_32 : index
          %67 = arith.select %62, %c0_33, %57 : index
          %68 = arith.select %62, %c0_33, %66 : index
          %guid_34, %ptr_35 = arts.db_acquire[<inout>] (%guid_30 : memref<?xi64>, %ptr_31 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%40], sizes[%c2000]), offsets[%67], sizes[%c1_32] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_35 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0]>)
          %69 = arith.addi %40, %44 : index
          %70 = arith.cmpi uge, %40, %c1 : index
          %71 = arith.subi %40, %c1 : index
          %72 = arith.select %70, %71, %c0 : index
          %73 = arith.addi %69, %c1 : index
          %74 = arith.minui %73, %c4000 : index
          %75 = arith.cmpi uge, %74, %72 : index
          %76 = arith.subi %74, %72 : index
          %77 = arith.select %75, %76, %c0 : index
          %78 = arith.maxui %77, %c1 : index
          %79 = arith.divui %72, %c4000 : index
          %80 = arith.addi %72, %78 : index
          %81 = arith.subi %80, %c1 : index
          %82 = arith.divui %81, %c4000 : index
          %83 = arith.cmpi ugt, %79, %c0 : index
          %84 = arith.select %83, %82, %c0 : index
          %85 = arith.subi %84, %79 : index
          %86 = arith.addi %85, %c1 : index
          %87 = arith.select %83, %c0, %79 : index
          %88 = arith.select %83, %c0, %86 : index
          %c1_36 = arith.constant 1 : index
          %c0_37 = arith.constant 0 : index
          %89 = arith.maxui %13, %c1_36 : index
          %90 = arith.muli %19, %89 : index
          %91 = arith.divui %72, %89 : index
          %92 = arith.addi %72, %77 : index
          %93 = arith.subi %92, %c1_36 : index
          %94 = arith.divui %93, %89 : index
          %95 = arith.subi %19, %c1_36 : index
          %96 = arith.cmpi ugt, %91, %95 : index
          %97 = arith.minui %94, %95 : index
          %98 = arith.select %96, %94, %97 : index
          %99 = arith.subi %98, %91 : index
          %100 = arith.addi %99, %c1_36 : index
          %101 = arith.select %96, %c0_37, %91 : index
          %102 = arith.select %96, %c0_37, %100 : index
          %guid_38, %ptr_39 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%72], sizes[%77]), offsets[%101], sizes[%102] element_offsets[%72] element_sizes[%77] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000, 4000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_39 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0]>)
          %c1_40 = arith.constant 1 : index
          %c0_41 = arith.constant 0 : index
          %103 = arith.maxui %21, %c1_40 : index
          %104 = arith.muli %27, %103 : index
          %105 = arith.divui %40, %103 : index
          %106 = arith.addi %40, %c2000 : index
          %107 = arith.subi %106, %c1_40 : index
          %108 = arith.divui %107, %103 : index
          %109 = arith.subi %27, %c1_40 : index
          %110 = arith.cmpi ugt, %105, %109 : index
          %111 = arith.minui %108, %109 : index
          %112 = arith.select %110, %108, %111 : index
          %113 = arith.subi %112, %105 : index
          %114 = arith.addi %113, %c1_40 : index
          %115 = arith.select %110, %c0_41, %105 : index
          %116 = arith.select %110, %c0_41, %114 : index
          %guid_42, %ptr_43 = arts.db_acquire[<out>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%40], sizes[%c2000]), offsets[%115], sizes[%c1_40] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_43 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_35, %ptr_39, %ptr_43) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 82 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000, 4000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?xf64>>):
            %c1_44 = arith.constant 1 : index
            %c0_45 = arith.constant 0 : index
            %117 = arith.maxui %21, %c1_44 : index
            %c0_46 = arith.constant 0 : index
            %c2000_47 = arith.constant 2000 : index
            %118 = arith.muli %arg2, %c2000_47 : index
            %119 = arith.minui %c2000_47, %c2000_47 : index
            %c1_48 = arith.constant 1 : index
            %120 = arith.maxui %119, %c1_48 : index
            %c1_49 = arith.constant 1 : index
            %121 = arith.maxui %120, %c1_49 : index
            %122 = arith.divui %118, %121 : index
            %123 = arith.select %110, %c0_46, %122 : index
            %c1_50 = arith.constant 1 : index
            %c0_51 = arith.constant 0 : index
            %124 = arith.maxui %13, %c1_50 : index
            %c0_52 = arith.constant 0 : index
            %c2000_53 = arith.constant 2000 : index
            %125 = arith.muli %arg2, %c2000_53 : index
            %c1_54 = arith.constant 1 : index
            %126 = arith.subi %125, %c1_54 : index
            %c0_55 = arith.constant 0 : index
            %127 = arith.select %70, %126, %c0_55 : index
            %c4000_56 = arith.constant 4000 : index
            %c1_57 = arith.constant 1 : index
            %128 = arith.maxui %7, %c1_57 : index
            %129 = arith.subi %128, %c1_57 : index
            %130 = arith.addi %c4000_56, %129 : index
            %131 = arith.divui %130, %128 : index
            %132 = arith.maxui %131, %c1_57 : index
            %c1_58 = arith.constant 1 : index
            %133 = arith.maxui %132, %c1_58 : index
            %c1_59 = arith.constant 1 : index
            %134 = arith.maxui %133, %c1_59 : index
            %135 = arith.divui %127, %134 : index
            %136 = arith.select %96, %c0_52, %135 : index
            %c1_60 = arith.constant 1 : index
            %c0_61 = arith.constant 0 : index
            %137 = arith.maxui %28, %c1_60 : index
            %c0_62 = arith.constant 0 : index
            %c2000_63 = arith.constant 2000 : index
            %138 = arith.muli %arg2, %c2000_63 : index
            %c1_64 = arith.constant 1 : index
            %139 = arith.maxui %c2000_63, %c1_64 : index
            %c1_65 = arith.constant 1 : index
            %140 = arith.maxui %139, %c1_65 : index
            %141 = arith.divui %138, %140 : index
            %142 = arith.select %62, %c0_62, %141 : index
            %143 = arith.subi %c0, %40 : index
            %144 = arith.cmpi slt, %143, %c0 : index
            %145 = arith.select %144, %c0, %143 : index
            %146 = arith.cmpi slt, %42, %c0 : index
            %147 = arith.select %146, %c0, %42 : index
            %148 = arith.minui %147, %44 : index
            scf.for %arg6 = %145 to %148 step %c1 {
              %149 = arith.addi %40, %arg6 : index
              %c1_66 = arith.constant 1 : index
              %150 = arith.maxui %137, %c1_66 : index
              %c0_67 = arith.constant 0 : index
              %c1_68 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %151 = arith.addi %40, %145 : index
              %152 = arith.divui %151, %150 : index
              %153 = arith.subi %arg6, %145 : index
              %154 = arith.addi %145, %153 : index
              %155 = arith.subi %152, %142 : index
              %156 = arts.db_ref %arg3[%c0_61] : memref<?xmemref<?xf64>> -> memref<?xf64>
              memref.store %cst_1, %156[%arg6] : memref<?xf64>
              scf.for %arg7 = %c0 to %c4000 step %c1 {
                %c1_69 = arith.constant 1 : index
                %157 = arith.maxui %137, %c1_69 : index
                %c0_70 = arith.constant 0 : index
                %c1_71 = arith.constant 1 : index
                %c-1_72 = arith.constant -1 : index
                %158 = arith.addi %40, %145 : index
                %159 = arith.divui %158, %157 : index
                %160 = arith.subi %arg6, %145 : index
                %161 = arith.addi %145, %160 : index
                %162 = arith.subi %159, %142 : index
                %163 = arts.db_ref %arg3[%c0_61] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %164 = memref.load %163[%arg6] : memref<?xf64>
                %c1_73 = arith.constant 1 : index
                %165 = arith.maxui %124, %c1_73 : index
                %166 = arith.divui %149, %165 : index
                %167 = arith.subi %166, %136 : index
                %168 = arith.remui %149, %165 : index
                %169 = arts.db_ref %arg4[%167] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %170 = memref.load %169[%168, %arg7] : memref<?x?xf64>
                %171 = arith.index_cast %arg7 : index to i32
                %172 = arith.sitofp %171 : i32 to f64
                %173 = arith.mulf %172, %cst : f64
                %174 = arith.mulf %170, %173 : f64
                %175 = arith.addf %164, %174 : f64
                %c1_74 = arith.constant 1 : index
                %176 = arith.maxui %137, %c1_74 : index
                %c0_75 = arith.constant 0 : index
                %c1_76 = arith.constant 1 : index
                %c-1_77 = arith.constant -1 : index
                %177 = arith.addi %40, %145 : index
                %178 = arith.divui %177, %176 : index
                %179 = arith.subi %arg6, %145 : index
                %180 = arith.addi %145, %179 : index
                %181 = arith.subi %178, %142 : index
                %182 = arts.db_ref %arg3[%c0_61] : memref<?xmemref<?xf64>> -> memref<?xf64>
                memref.store %175, %182[%arg6] : memref<?xf64>
              } {arts.id = 61 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "recovered"}
            } {arts.id = 80 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg5) : memref<?xmemref<?xf64>>
          }
        }
      }
    } : i64
    %36 = arts.epoch attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %40 = arith.muli %arg2, %c2000 : index
        %41 = arith.cmpi uge, %40, %c4000 : index
        %42 = arith.subi %c4000, %40 : index
        %43 = arith.select %41, %c0, %42 : index
        %44 = arith.minui %43, %c2000 : index
        %45 = arith.cmpi ugt, %44, %c0 : index
        scf.if %45 {
          %46 = arith.divui %40, %c4000 : index
          %47 = arith.addi %40, %c1999 : index
          %48 = arith.divui %47, %c4000 : index
          %49 = arith.cmpi ugt, %46, %c0 : index
          %50 = arith.select %49, %48, %c0 : index
          %51 = arith.subi %50, %46 : index
          %52 = arith.addi %51, %c1 : index
          %53 = arith.select %49, %c0, %46 : index
          %54 = arith.select %49, %c0, %52 : index
          %c0_32 = arith.constant 0 : index
          %55 = arith.cmpi slt, %42, %c0 : index
          %56 = arith.select %55, %c0, %42 : index
          %57 = arith.minui %56, %44 : index
          %c1_33 = arith.constant 1 : index
          %c0_34 = arith.constant 0 : index
          %guid_35, %ptr_36 = arts.db_acquire[<inout>] (%guid_30 : memref<?xi64>, %ptr_31 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%40], sizes[%c2000]), offsets[%c0_34], sizes[%34] {arts.id = 84 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_36 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %58 = arith.addi %40, %44 : index
          %59 = arith.cmpi uge, %40, %c1 : index
          %60 = arith.subi %40, %c1 : index
          %61 = arith.select %59, %60, %c0 : index
          %62 = arith.addi %58, %c1 : index
          %63 = arith.minui %62, %c4000 : index
          %64 = arith.cmpi uge, %63, %61 : index
          %65 = arith.subi %63, %61 : index
          %66 = arith.select %64, %65, %c0 : index
          %67 = arith.maxui %66, %c1 : index
          %68 = arith.divui %61, %c4000 : index
          %69 = arith.addi %61, %67 : index
          %70 = arith.subi %69, %c1 : index
          %71 = arith.divui %70, %c4000 : index
          %72 = arith.cmpi ugt, %68, %c0 : index
          %73 = arith.select %72, %71, %c0 : index
          %74 = arith.subi %73, %68 : index
          %75 = arith.addi %74, %c1 : index
          %76 = arith.select %72, %c0, %68 : index
          %77 = arith.select %72, %c0, %75 : index
          %c0_37 = arith.constant 0 : index
          %78 = arith.cmpi slt, %42, %c0 : index
          %79 = arith.select %78, %c0, %42 : index
          %80 = arith.minui %79, %44 : index
          %c1_38 = arith.constant 1 : index
          %c0_39 = arith.constant 0 : index
          %guid_40, %ptr_41 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%61], sizes[%66]), offsets[%c0_39], sizes[%19] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [4000, 2000], stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_41 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c4000] contract(<ownerDims = [1]>)
          %c1_42 = arith.constant 1 : index
          %c0_43 = arith.constant 0 : index
          %81 = arith.maxui %21, %c1_42 : index
          %82 = arith.muli %27, %81 : index
          %83 = arith.divui %40, %81 : index
          %84 = arith.addi %40, %c2000 : index
          %85 = arith.subi %84, %c1_42 : index
          %86 = arith.divui %85, %81 : index
          %87 = arith.subi %27, %c1_42 : index
          %88 = arith.cmpi ugt, %83, %87 : index
          %89 = arith.minui %86, %87 : index
          %90 = arith.select %88, %86, %89 : index
          %91 = arith.subi %90, %83 : index
          %92 = arith.addi %91, %c1_42 : index
          %93 = arith.select %88, %c0_43, %83 : index
          %94 = arith.select %88, %c0_43, %92 : index
          %guid_44, %ptr_45 = arts.db_acquire[<out>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%40], sizes[%c2000]), offsets[%93], sizes[%c1_42] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_45 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0]>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_36, %ptr_41, %ptr_45) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 83 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?xf64>>):
            %c1_46 = arith.constant 1 : index
            %c0_47 = arith.constant 0 : index
            %95 = arith.maxui %21, %c1_46 : index
            %c0_48 = arith.constant 0 : index
            %c2000_49 = arith.constant 2000 : index
            %96 = arith.muli %arg2, %c2000_49 : index
            %97 = arith.minui %c2000_49, %c2000_49 : index
            %c1_50 = arith.constant 1 : index
            %98 = arith.maxui %97, %c1_50 : index
            %c1_51 = arith.constant 1 : index
            %99 = arith.maxui %98, %c1_51 : index
            %100 = arith.divui %96, %99 : index
            %101 = arith.select %88, %c0_48, %100 : index
            %c1_52 = arith.constant 1 : index
            %c0_53 = arith.constant 0 : index
            %102 = arith.maxui %13, %c1_52 : index
            %c0_54 = arith.constant 0 : index
            %c1_55 = arith.constant 1 : index
            %c0_56 = arith.constant 0 : index
            %103 = arith.maxui %28, %c1_55 : index
            %c0_57 = arith.constant 0 : index
            %104 = arith.subi %c0, %40 : index
            %105 = arith.cmpi slt, %104, %c0 : index
            %106 = arith.select %105, %c0, %104 : index
            %107 = arith.cmpi slt, %42, %c0 : index
            %108 = arith.select %107, %c0, %42 : index
            %109 = arith.minui %108, %44 : index
            scf.for %arg6 = %106 to %109 step %c1 {
              %110 = arith.addi %40, %arg6 : index
              %c1_58 = arith.constant 1 : index
              %111 = arith.maxui %95, %c1_58 : index
              %c0_59 = arith.constant 0 : index
              %c1_60 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %112 = arith.addi %40, %106 : index
              %113 = arith.divui %112, %111 : index
              %114 = arith.subi %arg6, %106 : index
              %115 = arith.addi %106, %114 : index
              %116 = arith.subi %113, %101 : index
              %117 = arts.db_ref %arg5[%c0_47] : memref<?xmemref<?xf64>> -> memref<?xf64>
              memref.store %cst_1, %117[%arg6] : memref<?xf64>
              scf.for %arg7 = %c0 to %c4000 step %c1 {
                %c1_61 = arith.constant 1 : index
                %118 = arith.maxui %95, %c1_61 : index
                %c0_62 = arith.constant 0 : index
                %c1_63 = arith.constant 1 : index
                %c-1_64 = arith.constant -1 : index
                %119 = arith.addi %40, %106 : index
                %120 = arith.divui %119, %118 : index
                %121 = arith.subi %arg6, %106 : index
                %122 = arith.addi %106, %121 : index
                %123 = arith.subi %120, %101 : index
                %124 = arts.db_ref %arg5[%c0_47] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %125 = memref.load %124[%arg6] : memref<?xf64>
                %c1_65 = arith.constant 1 : index
                %126 = arith.maxui %102, %c1_65 : index
                %127 = arith.divui %arg7, %126 : index
                %128 = arith.subi %127, %c0_54 : index
                %129 = arith.remui %arg7, %126 : index
                %130 = arts.db_ref %arg4[%128] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %131 = memref.load %130[%129, %110] : memref<?x?xf64>
                %c1_66 = arith.constant 1 : index
                %132 = arith.maxui %103, %c1_66 : index
                %133 = arith.divui %arg7, %132 : index
                %134 = arith.subi %133, %c0_57 : index
                %135 = arith.remui %arg7, %132 : index
                %136 = arts.db_ref %arg3[%134] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %137 = memref.load %136[%135] : memref<?xf64>
                %138 = arith.mulf %131, %137 : f64
                %139 = arith.addf %125, %138 : f64
                %c1_67 = arith.constant 1 : index
                %140 = arith.maxui %95, %c1_67 : index
                %c0_68 = arith.constant 0 : index
                %c1_69 = arith.constant 1 : index
                %c-1_70 = arith.constant -1 : index
                %141 = arith.addi %40, %106 : index
                %142 = arith.divui %141, %140 : index
                %143 = arith.subi %arg6, %106 : index
                %144 = arith.addi %106, %143 : index
                %145 = arith.subi %142, %101 : index
                %146 = arts.db_ref %arg5[%c0_47] : memref<?xmemref<?xf64>> -> memref<?xf64>
                memref.store %139, %146[%arg6] : memref<?xf64>
              } {arts.id = 72 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "recovered"}
            } {arts.id = 81 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg5) : memref<?xmemref<?xf64>>
          }
        }
      }
    } : i64
    call @carts_kernel_timer_accum(%4) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%4) : (memref<?xi8>) -> ()
    %37 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%37, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %38 = scf.for %arg2 = %c0 to %c4000 step %c1 iter_args(%arg3 = %cst_1) -> (f64) {
      %c1_32 = arith.constant 1 : index
      %40 = arith.maxui %21, %c1_32 : index
      %41 = arith.divui %arg2, %40 : index
      %42 = arith.subi %41, %c0_21 : index
      %43 = arith.remui %arg2, %40 : index
      %44 = arts.db_ref %ptr_20[%42] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %45 = memref.load %44[%43] : memref<?xf64>
      %46 = arith.addf %arg3, %45 : f64
      scf.yield %46 : f64
    } {arts.id = 76 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:111:21">, arts.metadata_provenance = "recovered"}
    call @carts_bench_checksum_d(%38) : (f64) -> ()
    call @carts_phase_timer_stop(%37) : (memref<?xi8>) -> ()
    %39 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%39, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%39) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_30) : memref<?xi64>
    arts.db_free(%ptr_31) : memref<?xmemref<?xf64>>
    arts.db_free(%guid_19) : memref<?xi64>
    arts.db_free(%ptr_20) : memref<?xmemref<?xf64>>
    return %c0_i32 : i32
  }
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_accum(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_print(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>}
}
