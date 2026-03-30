// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline pre-lowering --arts-config %S/inputs/arts_multinode.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.pre-lowering.mlir' | %FileCheck %s

// Verify that two-level pre-lowering preserves Seidel's 2-D owner-space
// dependency shape. The leading row window may be renormalized from the task
// partition hints, but the untouched owner slots must remain materialized so
// the outlined dependency view stays rank-consistent.
// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: arts.dep_gep(%arg3) offset[%c0 : index] indices[%{{.+}}, %c0 : index, index] strides[%c1, %c1]
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>, offsets[%{{.+}}], sizes[%{{.+}}]), offsets[%{{.+}}, %c0], sizes[%{{.+}}, %c1] {{.*}}distribution_kind = #arts.distribution_kind<two_level>

module {
}
