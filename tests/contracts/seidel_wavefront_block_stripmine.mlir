// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline late-concurrency-cleanup --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.late-concurrency-cleanup.mlir' | %FileCheck %s

// Verify that the Seidel task kernel addresses the chosen row-owned wavefront
// block layout directly in the hot stencil loop: block indices feed
// `arts.db_ref` and the inner accesses use localized row/column indices.
// CHECK-LABEL: func.func @main
// CHECK: arts.edt <task>
// CHECK: arts.db_ref %arg5[%{{.+}}] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
// CHECK: memref.load %{{.+}}[%{{.+}}, %{{.+}}] : memref<?x?xf64>
// CHECK: memref.store %{{.+}}, %{{.+}}[%{{.+}}, %{{.+}}] : memref<?x?xf64>

module {
}
