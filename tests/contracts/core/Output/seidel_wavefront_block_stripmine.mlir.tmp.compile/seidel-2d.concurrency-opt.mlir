module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("seidel-2d\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c9599_i64 = arith.constant 9599 : i64
    %c1200 = arith.constant 1200 : index
    %c252 = arith.constant 252 : index
    %cst = arith.constant 9.000000e+00 : f64
    %c1_i32 = arith.constant 1 : i32
    %c600 = arith.constant 600 : index
    %c9599 = arith.constant 9599 : index
    %c-1 = arith.constant -1 : index
    %c64 = arith.constant 64 : index
    %c90 = arith.constant 90 : index
    %c15 = arith.constant 15 : index
    %c38 = arith.constant 38 : index
    %c253 = arith.constant 253 : index
    %c2 = arith.constant 2 : index
    %c320 = arith.constant 320 : index
    %c9600 = arith.constant 9600 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst_0 = arith.constant 9.600000e+03 : f64
    %c1 = arith.constant 1 : index
    %cst_1 = arith.constant 2.000000e+00 : f64
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_2 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %c2_i32 = arith.constant 2 : i32
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    call @carts_benchmarks_start() : () -> ()
    %4 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%4) : (memref<?xi8>) -> ()
    %5 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%5, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %6 = arts.runtime_query <total_workers> -> i32
    %7 = arith.index_castui %6 : i32 to index
    %8 = arith.maxui %7, %c1 : index
    %9 = arith.minui %8, %c1200 : index
    %10 = arith.addi %9, %c9599 : index
    %11 = arith.divui %10, %9 : index
    %12 = arith.maxui %11, %c1 : index
    %13 = arith.maxui %12, %c1 : index
    %14 = arith.index_cast %13 : index to i64
    %15 = arith.addi %14, %c9599_i64 : i64
    %16 = arith.divui %15, %14 : i64
    %17 = arith.index_cast %16 : i64 to index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <stencil>, <stencil>] route(%c-1_i32 : i32) sizes[%17] elementType(f64) elementSizes[%13, %c9600] {arts.id = 116 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf64>>) block_shape[%13] {owner_dims = array<i64: 0>, post_db_refined}
    %18 = arith.maxui %13, %c1 : index
    scf.for %arg2 = %c0 to %c9600 step %c1 {
      %22 = arith.index_cast %arg2 : index to i32
      %23 = arith.sitofp %22 : i32 to f64
      %24 = arith.divui %arg2, %18 : index
      %25 = arith.remui %arg2, %18 : index
      %26 = arts.db_ref %ptr[%24] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      scf.for %arg3 = %c0 to %c9600 step %c1 {
        %27 = arith.index_cast %arg3 : index to i32
        %28 = arith.addi %27, %c2_i32 : i32
        %29 = arith.sitofp %28 : i32 to f64
        %30 = arith.mulf %23, %29 : f64
        %31 = arith.addf %30, %cst_1 : f64
        %32 = arith.divf %31, %cst_0 : f64
        memref.store %32, %26[%25, %arg3] : memref<?x?xf64>
      } {arts.id = 40 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:37:23">}
    } {arts.id = 38 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:36:21">}
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    scf.for %arg2 = %c0 to %c320 step %c1 {
      %22 = arts.epoch {
        scf.for %arg3 = %c0 to %c90 step %c1 {
          %23 = arith.cmpi uge, %arg3, %c15 : index
          %24 = arith.subi %arg3, %c15 : index
          %25 = arith.select %23, %24, %c0 : index
          %26 = arith.addi %25, %c1 : index
          %27 = arith.divui %26, %c2 : index
          %28 = arith.divui %arg3, %c2 : index
          %29 = arith.addi %28, %c1 : index
          %30 = arith.minui %29, %c38 : index
          %31 = arith.muli %27, %c253 : index
          %32 = arith.muli %30, %c253 : index
          %33 = arith.subi %32, %31 : index
          %34 = arith.addi %33, %c252 : index
          %35 = arith.divui %34, %c253 : index
          %36 = arith.minui %35, %c64 : index
          %37 = arith.maxui %36, %c1 : index
          %38 = arith.divui %35, %c64 : index
          %39 = arith.remui %35, %c64 : index
          %40 = arith.addi %38, %c1 : index
          scf.for %arg4 = %c0 to %37 step %c1 {
            %41 = arith.cmpi ult, %arg4, %39 : index
            %42 = arith.select %41, %40, %38 : index
            %43 = arith.minui %arg4, %39 : index
            %44 = arith.muli %arg4, %38 : index
            %45 = arith.addi %44, %43 : index
            %46 = arith.cmpi uge, %45, %35 : index
            %47 = arith.subi %35, %45 : index
            %48 = arith.select %46, %c0, %47 : index
            %49 = arith.minui %42, %48 : index
            %50 = arith.cmpi ugt, %49, %c0 : index
            scf.if %50 {
              %51 = arith.muli %45, %c253 : index
              %52 = arith.addi %31, %51 : index
              %53 = arith.subi %32, %52 : index
              %54 = arith.cmpi slt, %53, %c0 : index
              %55 = arith.select %54, %c0, %53 : index
              %56 = arith.addi %55, %c252 : index
              %57 = arith.divui %56, %c253 : index
              %58 = arith.minui %57, %49 : index
              %59 = arith.maxui %58, %c1 : index
              %60 = arith.divui %52, %18 : index
              %61 = arith.addi %52, %59 : index
              %62 = arith.subi %61, %c1 : index
              %63 = arith.divui %62, %18 : index
              %64 = arith.subi %17, %c1 : index
              %65 = arith.cmpi ugt, %60, %64 : index
              %66 = arith.minui %63, %64 : index
              %67 = arith.select %65, %63, %66 : index
              %68 = arith.subi %67, %60 : index
              %69 = arith.addi %68, %c1 : index
              %70 = arith.select %65, %c0, %60 : index
              %71 = arith.select %65, %c0, %69 : index
              %guid_3, %ptr_4 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>), offsets[%70], sizes[%71] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [253, 600], stencil_center_offset = 1 : i64, stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_4 : memref<?xmemref<?x?xf64>>) dep_pattern(<wavefront_2d>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c253] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined, spatial_dims = array<i64: 0, 1>, supported_block_halo}
              arts.edt <task> <intranode> route(%c-1_i32) (%ptr_4) : memref<?xmemref<?x?xf64>> attributes {arts.id = 117 : i64, arts.pattern_revision = 1 : i64, critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [253, 600], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]} {
              ^bb0(%arg5: memref<?xmemref<?x?xf64>>):
                %72 = arith.subi %31, %52 : index
                %73 = arith.cmpi slt, %72, %c0 : index
                %74 = arith.select %73, %c0, %72 : index
                %75 = arith.addi %74, %c252 : index
                %76 = arith.divui %75, %c253 : index
                %77 = arith.maxui %18, %c1 : index
                %78 = arith.cmpi ugt, %77, %c2 : index
                scf.for %arg6 = %76 to %58 step %c1 {
                  %79 = arith.muli %arg6, %c253 : index
                  %80 = arith.addi %52, %79 : index
                  %81 = arith.divui %80, %c253 : index
                  %82 = arith.muli %81, %c2 : index
                  %83 = arith.subi %arg3, %82 : index
                  %84 = arith.maxui %80, %c1 : index
                  %85 = arith.addi %80, %c253 : index
                  %86 = arith.minui %85, %c9599 : index
                  %87 = arith.muli %83, %c600 : index
                  %88 = arith.maxui %87, %c1 : index
                  %89 = arith.addi %87, %c600 : index
                  %90 = arith.minui %89, %c9599 : index
                  scf.if %78 {
                    %91 = arith.subi %77, %c1 : index
                    %92 = arith.divui %84, %77 : index
                    %93 = arith.addi %86, %91 : index
                    %94 = arith.divui %93, %77 : index
                    %95 = arith.cmpi ugt, %86, %84 : index
                    %96 = arith.select %95, %94, %92 : index
                    scf.for %arg7 = %92 to %96 step %c1 {
                      %97 = arith.muli %arg7, %77 : index
                      %98 = arith.cmpi uge, %97, %84 : index
                      %99 = arith.subi %84, %97 : index
                      %100 = arith.select %98, %c0, %99 : index
                      %101 = arith.minui %100, %77 : index
                      %102 = arith.cmpi uge, %97, %86 : index
                      %103 = arith.subi %86, %97 : index
                      %104 = arith.select %102, %c0, %103 : index
                      %105 = arith.minui %104, %77 : index
                      %106 = arith.minui %105, %c1 : index
                      %107 = arith.addi %arg7, %c-1 : index
                      %108 = arith.subi %arg7, %70 : index
                      %109 = arith.subi %107, %70 : index
                      %110 = arts.db_ref %arg5[%108] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      %111 = arts.db_ref %arg5[%109] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      scf.for %arg8 = %101 to %106 step %c1 {
                        %119 = arith.addi %arg8, %c-1 : index
                        %120 = arith.addi %119, %77 : index
                        %121 = arith.addi %arg8, %c1 : index
                        scf.for %arg9 = %88 to %90 step %c1 {
                          %122 = arith.addi %arg9, %c-1 : index
                          %123 = memref.load %111[%120, %122] : memref<?x?xf64>
                          %124 = memref.load %111[%120, %arg9] : memref<?x?xf64>
                          %125 = arith.addf %123, %124 : f64
                          %126 = arith.addi %arg9, %c1 : index
                          %127 = memref.load %111[%120, %126] : memref<?x?xf64>
                          %128 = arith.addf %125, %127 : f64
                          %129 = memref.load %110[%arg8, %122] : memref<?x?xf64>
                          %130 = arith.addf %128, %129 : f64
                          %131 = memref.load %110[%arg8, %arg9] : memref<?x?xf64>
                          %132 = arith.addf %130, %131 : f64
                          %133 = memref.load %110[%arg8, %126] : memref<?x?xf64>
                          %134 = arith.addf %132, %133 : f64
                          %135 = memref.load %110[%121, %122] : memref<?x?xf64>
                          %136 = arith.addf %134, %135 : f64
                          %137 = memref.load %110[%121, %arg9] : memref<?x?xf64>
                          %138 = arith.addf %136, %137 : f64
                          %139 = memref.load %110[%121, %126] : memref<?x?xf64>
                          %140 = arith.addf %138, %139 : f64
                          %141 = arith.divf %140, %cst : f64
                          memref.store %141, %110[%arg8, %arg9] : memref<?x?xf64>
                        } {arts.id = 49 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:23">}
                      }
                      %112 = arith.maxui %101, %c1 : index
                      %113 = arith.minui %105, %91 : index
                      scf.for %arg8 = %112 to %113 step %c1 {
                        %119 = arith.addi %arg8, %c-1 : index
                        %120 = arith.addi %arg8, %c1 : index
                        scf.for %arg9 = %88 to %90 step %c1 {
                          %121 = arith.addi %arg9, %c-1 : index
                          %122 = memref.load %110[%119, %121] : memref<?x?xf64>
                          %123 = memref.load %110[%119, %arg9] : memref<?x?xf64>
                          %124 = arith.addf %122, %123 : f64
                          %125 = arith.addi %arg9, %c1 : index
                          %126 = memref.load %110[%119, %125] : memref<?x?xf64>
                          %127 = arith.addf %124, %126 : f64
                          %128 = memref.load %110[%arg8, %121] : memref<?x?xf64>
                          %129 = arith.addf %127, %128 : f64
                          %130 = memref.load %110[%arg8, %arg9] : memref<?x?xf64>
                          %131 = arith.addf %129, %130 : f64
                          %132 = memref.load %110[%arg8, %125] : memref<?x?xf64>
                          %133 = arith.addf %131, %132 : f64
                          %134 = memref.load %110[%120, %121] : memref<?x?xf64>
                          %135 = arith.addf %133, %134 : f64
                          %136 = memref.load %110[%120, %arg9] : memref<?x?xf64>
                          %137 = arith.addf %135, %136 : f64
                          %138 = memref.load %110[%120, %125] : memref<?x?xf64>
                          %139 = arith.addf %137, %138 : f64
                          %140 = arith.divf %139, %cst : f64
                          memref.store %140, %110[%arg8, %arg9] : memref<?x?xf64>
                        } {arts.id = 49 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:23">}
                      }
                      %114 = arith.maxui %101, %91 : index
                      %115 = arith.minui %105, %77 : index
                      %116 = arith.addi %arg7, %c1 : index
                      %117 = arith.subi %116, %70 : index
                      %118 = arts.db_ref %arg5[%117] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      scf.for %arg8 = %114 to %115 step %c1 {
                        %119 = arith.addi %arg8, %c-1 : index
                        %120 = arith.addi %arg8, %c1 : index
                        %121 = arith.subi %120, %77 : index
                        scf.for %arg9 = %88 to %90 step %c1 {
                          %122 = arith.addi %arg9, %c-1 : index
                          %123 = memref.load %110[%119, %122] : memref<?x?xf64>
                          %124 = memref.load %110[%119, %arg9] : memref<?x?xf64>
                          %125 = arith.addf %123, %124 : f64
                          %126 = arith.addi %arg9, %c1 : index
                          %127 = memref.load %110[%119, %126] : memref<?x?xf64>
                          %128 = arith.addf %125, %127 : f64
                          %129 = memref.load %110[%arg8, %122] : memref<?x?xf64>
                          %130 = arith.addf %128, %129 : f64
                          %131 = memref.load %110[%arg8, %arg9] : memref<?x?xf64>
                          %132 = arith.addf %130, %131 : f64
                          %133 = memref.load %110[%arg8, %126] : memref<?x?xf64>
                          %134 = arith.addf %132, %133 : f64
                          %135 = memref.load %118[%121, %122] : memref<?x?xf64>
                          %136 = arith.addf %134, %135 : f64
                          %137 = memref.load %118[%121, %arg9] : memref<?x?xf64>
                          %138 = arith.addf %136, %137 : f64
                          %139 = memref.load %118[%121, %126] : memref<?x?xf64>
                          %140 = arith.addf %138, %139 : f64
                          %141 = arith.divf %140, %cst : f64
                          memref.store %141, %110[%arg8, %arg9] : memref<?x?xf64>
                        } {arts.id = 49 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:23">}
                      }
                    }
                  } else {
                    scf.for %arg7 = %84 to %86 step %c1 {
                      %91 = arith.index_cast %arg7 : index to i32
                      %92 = arith.addi %91, %c-1_i32 : i32
                      %93 = arith.index_cast %92 : i32 to index
                      %94 = arith.addi %91, %c1_i32 : i32
                      %95 = arith.index_cast %94 : i32 to index
                      %96 = arith.divui %93, %77 : index
                      %97 = arith.remui %93, %77 : index
                      %98 = arith.divui %arg7, %77 : index
                      %99 = arith.remui %arg7, %77 : index
                      %100 = arith.divui %95, %77 : index
                      %101 = arith.remui %95, %77 : index
                      %102 = arith.subi %96, %70 : index
                      %103 = arith.subi %98, %70 : index
                      %104 = arith.subi %100, %70 : index
                      %105 = arts.db_ref %arg5[%102] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      %106 = arts.db_ref %arg5[%103] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      %107 = arts.db_ref %arg5[%104] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      scf.for %arg8 = %88 to %90 step %c1 {
                        %108 = arith.addi %arg8, %c-1 : index
                        %109 = memref.load %105[%97, %108] : memref<?x?xf64>
                        %110 = memref.load %105[%97, %arg8] : memref<?x?xf64>
                        %111 = arith.addf %109, %110 : f64
                        %112 = arith.addi %arg8, %c1 : index
                        %113 = memref.load %105[%97, %112] : memref<?x?xf64>
                        %114 = arith.addf %111, %113 : f64
                        %115 = memref.load %106[%99, %108] : memref<?x?xf64>
                        %116 = arith.addf %114, %115 : f64
                        %117 = memref.load %106[%99, %arg8] : memref<?x?xf64>
                        %118 = arith.addf %116, %117 : f64
                        %119 = memref.load %106[%99, %112] : memref<?x?xf64>
                        %120 = arith.addf %118, %119 : f64
                        %121 = memref.load %107[%101, %108] : memref<?x?xf64>
                        %122 = arith.addf %120, %121 : f64
                        %123 = memref.load %107[%101, %arg8] : memref<?x?xf64>
                        %124 = arith.addf %122, %123 : f64
                        %125 = memref.load %107[%101, %112] : memref<?x?xf64>
                        %126 = arith.addf %124, %125 : f64
                        %127 = arith.divf %126, %cst : f64
                        memref.store %127, %106[%99, %arg8] : memref<?x?xf64>
                      } {arts.id = 49 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:23">}
                    } {arts.id = 48 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:46:21">}
                  }
                } {arts.id = 118 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:23">}
                arts.db_release(%arg5) : memref<?xmemref<?x?xf64>>
              }
            }
          }
        } {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [253, 600], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]}
      } : i64
    }
    call @carts_kernel_timer_stop(%4) : (memref<?xi8>) -> ()
    %19 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%19, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %20 = scf.for %arg2 = %c0 to %c9600 step %c1 iter_args(%arg3 = %cst_2) -> (f64) {
      %22 = arith.divui %arg2, %18 : index
      %23 = arith.remui %arg2, %18 : index
      %24 = arts.db_ref %ptr[%22] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      %25 = memref.load %24[%23, %arg2] : memref<?x?xf64>
      %26 = arith.addf %arg3, %25 : f64
      scf.yield %26 : f64
    } {arts.id = 71 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:63:21">}
    call @carts_bench_checksum_d(%20) : (f64) -> ()
    call @carts_phase_timer_stop(%19) : (memref<?xi8>) -> ()
    %21 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%21, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%21) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
    return %c0_i32 : i32
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
