// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_2t.cfg --pipeline openmp-to-arts | %FileCheck %s
// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_2t.cfg --pipeline edt-distribution | %FileCheck %s --check-prefix=LOWER

// Verify that ConvertSdeToArts preserves the SDE-selected local-accumulate
// reduction strategy on both the lowered EDT and the lowered arts.for. Also
// verify that ForLowering keeps the linear combine path for this case.

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel> <intranode> route(%{{.*}}) attributes {arts.reduction_strategy = "local_accumulate"
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>
// CHECK-SAME: no_verify = #arts.no_verify}
// CHECK: arts.for(%c0) to(%c16) step(%c1) reduction(%{{.*}} : memref<?xi32>)
// CHECK: } {arts.reduction_kinds = [0 : i32], arts.reduction_strategy = "local_accumulate"
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>

// LOWER-LABEL: func.func @main
// LOWER: arts.edt <parallel> <intranode> route(%{{.*}}) (%{{.*}}) : memref<?xmemref<?xi32>> attributes {arts.reduction_strategy = "local_accumulate"
// LOWER: arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?xi32>>) partitioning(<coarse>)
// LOWER: arts.db_acquire[<out>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?xi32>>) partitioning(<coarse>)
// LOWER: arts.edt <task> <intranode> route(%{{.*}}) (%{{.*}}, %{{.*}}) : memref<?xmemref<?xi32>>, memref<?xmemref<?xi32>>
// LOWER: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}} iter_args(%{{.*}} = %{{.*}}) -> (i32)
// LOWER-NOT: scf.if
// LOWER-NOT: arts.atomic_add(

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

  func.func @main(%A: memref<16x4xi32>) {
    %c0 = arith.constant 0 : index
    %c16 = arith.constant 16 : index
    %c4 = arith.constant 4 : index
    %c1 = arith.constant 1 : index
    %sum = memref.alloca() : memref<1xi32>
    %sum_cast = memref.cast %sum : memref<1xi32> to memref<?xi32>
    %c0_i32 = arith.constant 0 : i32
    memref.store %c0_i32, %sum[%c0] : memref<1xi32>
    omp.parallel {
      omp.wsloop reduction(@add_i32 %sum_cast -> %prv : memref<?xi32>) {
        omp.loop_nest (%i) : index = (%c0) to (%c16) step (%c1) {
          scf.for %j = %c0 to %c4 step %c1 {
            %val = memref.load %A[%i, %j] : memref<16x4xi32>
            %acc = memref.load %prv[%c0] : memref<?xi32>
            %next = arith.addi %acc, %val : i32
            memref.store %next, %prv[%c0] : memref<?xi32>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
