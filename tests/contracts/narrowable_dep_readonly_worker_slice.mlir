// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from concurrency --pipeline edt-distribution | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.lowering_contract(%ptr : memref<?xmemref<?xf64>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) {contract_kind = 2 : i64, distribution_version = 1 : i64, narrowable_dep}
// CHECK: arts.db_acquire[<in>] {{.*}}partitioning(<block>, offsets[%[[BLOCK_OFF:.*]]], sizes[%[[BLOCK_SIZE:.*]]]), offsets[%[[DB_OFF:.*]]], sizes[%[[DB_SIZE:.*]]] element_offsets[%[[BLOCK_OFF]]] element_sizes[%[[BLOCK_SIZE]]]

module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %c1024 = arith.constant 1024 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %cst = arith.constant 2.000000e+00 : f64
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c1024] {arts.id = 80 : i64, arts.local_only, arts.read_only_after_init, contract_kind = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, narrowable_dep} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?xf64>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) {contract_kind = 2 : i64, distribution_version = 1 : i64, narrowable_dep}
    %guid_0, %ptr_1 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c1024] {arts.id = 81 : i64, arts.local_only, arts.read_only_after_init, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    scf.for %arg0 = %c0 to %c1024 step %c1 {
      %0 = arith.index_cast %arg0 : index to i32
      %1 = arith.sitofp %0 : i32 to f64
      %2 = arts.db_ref %ptr[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %1, %2[%arg0] : memref<?xf64>
    }
    %guid_2, %ptr_3 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guid_4, %ptr_5 = arts.db_acquire[<out>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <parallel> <intranode> route(%c-1_i32) (%ptr_3, %ptr_5) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 82 : i64, workers = #arts.workers<8>} {
    ^bb0(%arg0: memref<?xmemref<?xf64>>, %arg1: memref<?xmemref<?xf64>>):
      arts.for(%c0) to(%c1024) step(%c1) {{
      ^bb0(%arg2: index):
        %0 = arts.db_ref %arg0[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        %1 = memref.load %0[%arg2] : memref<?xf64>
        %2 = arith.mulf %1, %cst : f64
        %3 = arts.db_ref %arg1[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        memref.store %2, %3[%arg2] : memref<?xf64>
      }}
      arts.db_release(%arg0) : memref<?xmemref<?xf64>>
      arts.db_release(%arg1) : memref<?xmemref<?xf64>>
    }
    %0 = scf.for %arg0 = %c0 to %c1024 step %c1 iter_args(%arg1 = %c0) -> (index) {
      %1 = arts.db_ref %ptr_1[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %2 = memref.load %1[%arg0] : memref<?xf64>
      %3 = arith.truncf %2 : f64 to f32
      %4 = arith.bitcast %3 : f32 to i32
      %5 = arith.index_cast %4 : i32 to index
      %6 = arith.addi %arg1, %5 : index
      scf.yield %6 : index
    }
    arts.db_free(%guid_0) : memref<?xi64>
    arts.db_free(%ptr_1) : memref<?xmemref<?xf64>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf64>>
    %1 = arith.index_cast %0 : index to i32
    return %1 : i32
  }
}
