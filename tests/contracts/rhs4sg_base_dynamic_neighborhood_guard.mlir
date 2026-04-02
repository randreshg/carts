// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline late-concurrency-cleanup --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile/rhs4sg_base.late-concurrency-cleanup.mlir' | %FileCheck %s

// Verify that rhs4sg-base gets the dynamic neighborhood strip-mining guard on
// the owner-dimension loop. The fast path hoists block-local db_ref selection
// out of the hot point loop, while the else path preserves the original
// div/rem formulation for tiny blocks where strip-mining is not profitable.
// CHECK-LABEL: func.func @main
// CHECK: arts.edt <task>
// CHECK: %[[BS:.+]] = arith.maxui %{{.+}}, %c1 : index
// CHECK-NEXT: %[[GUARD:.+]] = arith.cmpi ugt, %[[BS]], %c2 : index
// CHECK-NEXT: scf.if %[[GUARD]] {
// CHECK: arts.db_ref %{{.+}}[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
// CHECK: arts.db_ref %{{.+}}[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
// CHECK: arts.db_ref %{{.+}}[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
// CHECK: scf.for %[[BLOCK:.+]] = %{{.+}} to %{{.+}} step %c1 {
// CHECK: %[[BLOCK_START:.+]] = arith.muli %[[BLOCK]], %[[BS]] : index
// CHECK: arts.db_ref %{{.+}}[%{{.+}}] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
// CHECK: arts.db_ref %{{.+}}[%{{.+}}] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
// CHECK: scf.for %[[LOCAL:.+]] = %{{.+}} to %{{.+}} step %c1 {
// CHECK: %[[GLOBAL:.+]] = arith.addi %[[BLOCK_START]], %[[LOCAL]] : index
// CHECK: } else {
// CHECK: scf.for %[[FALLBACK_IV:.+]] = %{{.+}} to %{{.+}} step %c1 {
// CHECK: %[[FALLBACK_GLOBAL:.+]] = arith.addi %{{.+}}, %[[FALLBACK_IV]] : index
// CHECK: %{{.+}} = arith.divui %[[FALLBACK_GLOBAL]], %[[BS]] : index
// CHECK: %{{.+}} = arith.remui %[[FALLBACK_GLOBAL]], %[[BS]] : index

module {
}
