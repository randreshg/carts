// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.pre-lowering.mlir' | %FileCheck %s

// Verify that Seidel keeps 2-D stencil ownership through the acquire/contract
// attrs while using block distribution instead of the matmul-oriented
// tiling_2d lane-slicing path.
// CHECK-LABEL: func.func @main
// CHECK-NOT: distribution_kind = #arts.distribution_kind<tiling_2d>
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <stencil>, <stencil>] {{.*}} elementType(f64) elementSizes[%{{.+}}, %{{.+}}]
// CHECK: %[[EPOCH:.+]] = arts.create_epoch : i64
// CHECK: scf.for %[[WAVE:.+]] = %c0 to %{{.+}} step %c1 {
// CHECK: %[[ACTIVE_ROWS:.+]] = arith.minui %[[NUM_ROWS:.+]], %c64 : index
// CHECK: %[[DISPATCH:.+]] = arith.maxui %[[ACTIVE_ROWS]], %c1 : index
// CHECK: scf.for %[[WORKER:.+]] = %c0 to %[[DISPATCH]] step %c1 {
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>), offsets[%{{.+}}, %c0], sizes[%{{.+}}, %c1] {depPattern = #arts.dep_pattern<wavefront_2d>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [{{.+}}], stencil_center_offset = 1 : i64, stencil_owner_dims = [0, 1]
// CHECK: arts.wait_on_epoch %[[EPOCH]] : i64

module {
}
