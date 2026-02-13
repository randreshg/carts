// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTRA
// RUN: %carts-compile %s --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTER

// INTRA: distribution_kind = #arts.distribution_kind<block_cyclic>
// INTRA: distribution_pattern = #arts.distribution_pattern<triangular>

// INTER: distribution_kind = #arts.distribution_kind<two_level>
// INTER: distribution_pattern = #arts.distribution_pattern<triangular>

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func @main(%argc: i32, %argv: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index

    %buf = memref.alloc() : memref<64xi32>

    scf.parallel (%i) = (%c0) to (%c64) step (%c1) {
      scf.for %j = %i to %c64 step %c1 {
        %v = arith.index_cast %j : index to i32
        memref.store %v, %buf[%j] : memref<64xi32>
        %x = memref.load %buf[%j] : memref<64xi32>
        func.call @sink_i32(%x) : (i32) -> ()
      }
      scf.yield
    }

    memref.dealloc %buf : memref<64xi32>
    return %c0_i32 : i32
  }

  func.func private @sink_i32(i32) -> () attributes {llvm.linkage = #llvm.linkage<external>}
}

