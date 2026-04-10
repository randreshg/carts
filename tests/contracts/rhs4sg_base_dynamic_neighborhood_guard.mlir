// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline late-concurrency-cleanup --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile/rhs4sg_base.late-concurrency-cleanup.mlir' | %FileCheck %s

// Verify that rhs4sg-base gets the dynamic neighborhood strip-mining guard on
// the owner-dimension loop. The fast path hoists block-local db_ref selection
// out of the hot point loop, while the else path preserves the original
// div/rem formulation for tiny blocks where strip-mining is not profitable.
// CHECK-LABEL: func.func @main
// CHECK: %[[BS:.+]] = arith.minui %{{.+}}, %c9 : index
// CHECK: %[[PROFIT:.+]] = arith.cmpi ugt, %[[BS]], %c0 : index
// CHECK: scf.if %[[PROFIT]] {
// CHECK: arts.edt <task>
// CHECK: %{{.+}} = arith.minui %{{.+}}, %[[BS]] : index
// CHECK: %{{.+}} = arts.db_ref %arg4[%{{.+}}] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
// CHECK: %[[OUT_BLOCK:.+]] = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
// CHECK: scf.for %{{.+}} = %{{.+}} to %{{.+}} step %c1 {
// CHECK: %{{.+}} = arith.addi %{{.+}}, %{{.+}} : index

module {
}
