// RUN: %carts-compile %S/../../external/carts-benchmarks/polybench/gemm/gemm.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --distributed-db --stop-at concurrency-opt --debug-only=distributed_db_ownership 2>&1 | %FileCheck %s

// CHECK-NOT: reason=read_only_internode_use
// CHECK: DistributedDbOwnership marked 3 / {{[1-9][0-9]*}} DbAlloc operations
// CHECK-COUNT-3: arts.db_alloc{{.*}}distributed
// CHECK: arts.epoch attributes {distribution_kind = #arts.distribution_kind<tiling_2d>, distribution_pattern = #arts.distribution_pattern<matmul>
// CHECK: %[[MATMUL_OUT_GUID:.*]], %[[MATMUL_OUT_PTR:.*]] = arts.db_acquire[<inout>] (%[[OUT_GUID:.*]] : memref<?xi64>, %[[OUT_PTR:.*]] : memref<?xmemref<?x?xf32>>)
// CHECK: %[[MATMUL_IN0_GUID:.*]], %[[MATMUL_IN0_PTR:.*]] = arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?xf32>>)
// CHECK: %[[MATMUL_IN1_GUID:.*]], %[[MATMUL_IN1_PTR:.*]] = arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?xf32>>)
// CHECK: %[[CHK_GUID:.*]], %[[CHK_PTR:.*]] = arts.db_acquire[<in>] (%[[OUT_GUID]] : memref<?xi64>, %[[OUT_PTR]] : memref<?xmemref<?x?xf32>>)

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func @main(%argc: i32, %argv: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c1024 = arith.constant 1024 : index

    %buf = memref.alloc() : memref<1024xi32>

    scf.parallel (%i) = (%c0) to (%c1024) step (%c1) {
      %iv = arith.index_cast %i : index to i32
      memref.store %iv, %buf[%i] : memref<1024xi32>
      %v = memref.load %buf[%i] : memref<1024xi32>
      func.call @sink_i32(%v) : (i32) -> ()
      scf.yield
    }

    memref.dealloc %buf : memref<1024xi32>
    return %c0_i32 : i32
  }

  func.func private @sink_i32(i32) -> () attributes {llvm.linkage = #llvm.linkage<external>}
}
