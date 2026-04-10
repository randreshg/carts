// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline pre-lowering --arts-config %S/../../examples/arts.cfg -- -I%S/../../../external/carts-benchmarks/sw4lite/common -DNX=20 -DNY=20 -DNZ=20 -DNREPS=2 >/dev/null && ! grep -q "arts.db_alloc\\[<in>, <stack>" %t.compile/rhs4sg_base.pre-lowering.mlir && cat %t.compile/rhs4sg_base.pre-lowering.mlir' | %FileCheck %s

// The loop-local stencil weights must stay private to the outlined EDT even
// when the kernel call is wrapped in an outer serial repetition loop.

// CHECK: func.func private @__arts_edt_1
// CHECK: %[[W:.+]] = memref.alloca() {{.*}} : memref<5xf32>
// CHECK: memref.load %[[W]][
