// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_64t.cfg --pipeline edt-distribution | %FileCheck %s --check-prefix=ATOMIC
// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_multinode_4x16.cfg --pipeline edt-distribution | %FileCheck %s --check-prefix=TREE

// Verify that the downstream consumer differentiates atomic and tree:
// ATOMIC uses arts.atomic_add in the result EDT.
// TREE uses a pairwise fold with iter_args and a conditional RHS.

// ATOMIC-LABEL: func.func @main
// ATOMIC: arts.edt <task> <intranode>
// ATOMIC: %[[LOCAL:.+]] = memref.alloca() : memref<i32>
// ATOMIC: memref.store %{{.+}}, %[[LOCAL]][] : memref<i32>
// ATOMIC: %[[PARTIAL:.+]] = arts.db_ref %arg3[%{{.+}}] : memref<?xi32> -> memref<?xi32>
// ATOMIC: memref.load %[[LOCAL]][] : memref<i32>
// ATOMIC: memref.store %{{.+}}, %[[PARTIAL]][%{{.+}}] : memref<?xi32>
// ATOMIC: arts.edt <task> <intranode>
// ATOMIC: arts.atomic_add(%{{.+}}, %{{.+}} : memref<?xi32>, i32)
// ATOMIC-NOT: iter_args(

// TREE-LABEL: func.func @main
// TREE: arts.edt <task> <internode>
// TREE: %[[LOCAL:.+]] = memref.alloca() : memref<i32>
// TREE: memref.store %{{.+}}, %[[LOCAL]][] : memref<i32>
// TREE: %[[PARTIAL:.+]] = arts.db_ref %arg3[%{{.+}}] : memref<?xi32> -> memref<?xi32>
// TREE: memref.load %[[LOCAL]][] : memref<i32>
// TREE: memref.store %{{.+}}, %[[PARTIAL]][%{{.+}}] : memref<?xi32>
// TREE: arts.edt <task> <internode>
// TREE: scf.for {{.+}} step %{{.+}} iter_args(%{{.+}} = %{{.+}}) -> (i32)
// TREE: scf.if %{{.+}} -> (i32)
// TREE-NOT: arts.atomic_add

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
