// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_64t.cfg --pipeline openmp-to-arts | %FileCheck %s --check-prefix=ATOMIC
// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_multinode_4x16.cfg --pipeline openmp-to-arts | %FileCheck %s --check-prefix=TREE
// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_64t.cfg --pipeline edt-distribution | %FileCheck %s --check-prefix=ATOMIC-LOWER
// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_multinode_4x16.cfg --pipeline edt-distribution | %FileCheck %s --check-prefix=TREE-LOWER

// Verify that ConvertSdeToArts preserves the SDE-selected reduction strategy
// on both the lowered EDT and the lowered arts.for. Also verify that the
// strategy materially changes the result-EDT combine path after ForLowering.

// ATOMIC-LABEL: func.func @main
// ATOMIC: arts.edt <parallel> <intranode> route(%{{.*}}) attributes {arts.reduction_strategy = "atomic"
// ATOMIC-SAME: distribution_kind = #arts.distribution_kind<block>
// ATOMIC-SAME: no_verify = #arts.no_verify}
// ATOMIC: arts.for(%c0) to(%c128) step(%c1) reduction(%{{.*}} : memref<?xi32>)
// ATOMIC: } {arts.reduction_kinds = [0 : i32], arts.reduction_strategy = "atomic"
// ATOMIC-SAME: distribution_kind = #arts.distribution_kind<block>

// TREE-LABEL: func.func @main
// TREE: arts.edt <parallel> <internode> route(%{{.*}}) attributes {arts.reduction_strategy = "tree"
// TREE-SAME: no_verify = #arts.no_verify}
// TREE: arts.for(%c0) to(%c128) step(%c1) reduction(%{{.*}} : memref<?xi32>)
// TREE: } {arts.reduction_kinds = [0 : i32], arts.reduction_strategy = "tree"

// ATOMIC-LOWER-LABEL: func.func @main
// ATOMIC-LOWER: arts.edt <parallel> <intranode> route(%{{.*}}) (%{{.*}}) : memref<?xmemref<?xi32>> attributes {arts.reduction_strategy = "atomic"
// ATOMIC-LOWER: arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?xi32>>) partitioning(<coarse>)
// ATOMIC-LOWER: arts.db_acquire[<inout>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?xi32>>) partitioning(<coarse>)
// ATOMIC-LOWER: arts.edt <task> <intranode> route(%{{.*}}) (%{{.*}}, %{{.*}}) : memref<?xmemref<?xi32>>, memref<?xmemref<?xi32>>
// ATOMIC-LOWER: arts.atomic_add(%{{.*}}, %{{.*}} : memref<?xi32>, i32)

// TREE-LOWER-LABEL: func.func @main
// TREE-LOWER: arts.edt <parallel> <internode> route(%{{.*}}) (%{{.*}}) : memref<?xmemref<?xi32>> attributes {arts.reduction_strategy = "tree"
// TREE-LOWER: arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?xi32>>) partitioning(<coarse>)
// TREE-LOWER: arts.db_acquire[<out>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?xi32>>) partitioning(<coarse>)
// TREE-LOWER: arts.edt <task> <internode> route(%{{.*}}) (%{{.*}}, %{{.*}}) : memref<?xmemref<?xi32>>, memref<?xmemref<?xi32>>
// TREE-LOWER: scf.if %{{.*}} -> (i32)
// TREE-LOWER-NOT: arts.atomic_add(

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  omp.declare_reduction @add_i32 : i32 init {
  ^bb0(%arg0: i32):
    %c0_i32 = arith.constant 0 : i32
    omp.yield(%c0_i32 : i32)
  } combiner {
  ^bb0(%lhs: i32, %rhs: i32):
    %sum = arith.addi %lhs, %rhs : i32
    omp.yield(%sum : i32)
  }

  func.func @main(%A: memref<128xi32>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    %sum = memref.alloca() : memref<1xi32>
    %sum_cast = memref.cast %sum : memref<1xi32> to memref<?xi32>
    %c0_i32 = arith.constant 0 : i32
    memref.store %c0_i32, %sum[%c0] : memref<1xi32>
    omp.parallel {
      omp.wsloop reduction(@add_i32 %sum_cast -> %prv : memref<?xi32>) {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %val = memref.load %A[%i] : memref<128xi32>
          %acc = memref.load %prv[%c0] : memref<?xi32>
          %next = arith.addi %acc, %val : i32
          memref.store %next, %prv[%c0] : memref<?xi32>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
