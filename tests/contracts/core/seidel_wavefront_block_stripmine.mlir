// RUN: %carts-compile %S/../inputs/snapshots/seidel_2d_openmp_to_arts.mlir --pipeline late-concurrency-cleanup --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Verify that the Seidel task kernel addresses the chosen row-owned wavefront
// block layout directly in the hot stencil loop: block indices feed
// `arts.db_ref` and the inner accesses use localized row/column indices.
// CHECK-LABEL: func.func @main
// CHECK: arts.edt <task>
// CHECK: arts.db_ref %arg5[%{{.+}}] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
// CHECK: memref.load %{{.+}}[%{{.+}}, %{{.+}}] : memref<?x?xf64>
// CHECK: memref.store %{{.+}}, %{{.+}}[%{{.+}}, %{{.+}}] : memref<?x?xf64>