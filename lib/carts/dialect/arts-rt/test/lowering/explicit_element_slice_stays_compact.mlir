// RUN: %carts-compile %s --pass-pipeline='builtin.module(edt-lowering)' | %FileCheck %s --check-prefix=RT
// RUN: %carts-compile %s --arts-config %arts_config --start-from pre-lowering --pipeline arts-rt-to-llvm | %FileCheck %s --check-prefix=LLVM

// Explicit element slices are an upstream ARTS/CODIR contract. ARTS-RT must
// lower them mechanically, even when the acquire carries a stencil dep pattern.

// RT-LABEL: func.func @explicit_stencil_slice_stays_compact
// RT: arts_rt.rec_dep
// RT-SAME: byte_offsets(
// RT-SAME: byte_sizes(
// RT-NOT: dep_flags

// LLVM-LABEL: func.func @explicit_stencil_slice_stays_compact
// LLVM-NOT: call @arts_add_dependence(
// LLVM: call @arts_add_dependence_at
// LLVM-NOT: call @arts_add_dependence(

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i64:64-n32:64-S128",
  llvm.target_triple = "x86_64-unknown-linux-gnu"
} {
  func.func @explicit_stencil_slice_stays_compact() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c3 = arith.constant 3 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c10 = arith.constant 10 : index

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c3] elementType(f32) elementSizes[%c10, %c4] {local_only} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %slice_guid, %slice_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf32>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] element_offsets[%c8, %c0] element_sizes[%c1, %c4] {depPattern = #arts.dep_pattern<stencil_tiling_nd>} -> (memref<?xi64>, memref<?xmemref<?x?xf32>>)

    arts.edt <task> <intranode> route(%route) (%slice_ptr) : memref<?xmemref<?x?xf32>> {
    ^bb0(%dep: memref<?xmemref<?x?xf32>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
      %v = memref.load %payload[%c0, %c0] : memref<?x?xf32>
      memref.store %v, %payload[%c0, %c0] : memref<?x?xf32>
      arts.yield
    }

    return
  }
}
