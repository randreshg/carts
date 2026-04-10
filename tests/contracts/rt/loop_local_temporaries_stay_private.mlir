// RUN: %carts-compile %S/../inputs/snapshots/rhs4sg_base_20x20x20_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../../examples/arts.cfg | %FileCheck %s
// RUN: %carts-compile %S/../inputs/snapshots/rhs4sg_base_20x20x20_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../../examples/arts.cfg | not grep -q 'arts.db_alloc\[<in>, <stack>'

// The loop-local stencil weights must stay private to the outlined EDT even
// when the kernel call is wrapped in an outer serial repetition loop.

// CHECK: func.func private @__arts_edt_1
// CHECK: %[[W:.+]] = memref.alloca() {{.*}} : memref<5xf32>
// CHECK: memref.load %[[W]][
