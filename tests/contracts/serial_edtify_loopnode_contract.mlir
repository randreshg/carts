// RUN: %carts-compile %s --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --serial-edtify --stop-at loop-reordering --debug-only=serial_edtify 2>&1 | %FileCheck %s --check-prefix=EDTIFY
// RUN: %carts-compile %s --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --distributed-db --stop-at concurrency-opt --debug-only=serial_edtify,distributed_db_ownership 2>&1 | %FileCheck %s --check-prefix=DIST

// EDTIFY: SerialEdtify outlined 1 / 1 eligible serial loop(s)
// EDTIFY: arts.edt <task> <internode>
// EDTIFY: arts.for(

// DIST: SerialEdtify outlined 1 / 1 eligible serial loop(s)
// DIST: DistributedDbOwnership marked 0 / 1 DbAlloc operations

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func @main(%argc: i32, %argv: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index

    %buf = memref.alloc() : memref<128xi32>

    scf.for %i = %c0 to %c128 step %c1 {
      %v = arith.index_cast %i : index to i32
      %sum = arith.addi %v, %argc : i32
      memref.store %sum, %buf[%i] : memref<128xi32>
    }
    call @sink_memref(%buf) : (memref<128xi32>) -> ()
    memref.dealloc %buf : memref<128xi32>
    return %c0_i32 : i32
  }
  func.func private @sink_memref(memref<128xi32>) -> () attributes {llvm.linkage = #llvm.linkage<external>}
}
